#!/usr/bin/env python3
"""
End-to-end IR test runner.

Runs the compiler on every testcase under testcases/IR-1, feeds the emitted
LLVM IR to clang to generate RISC-V assembly, links it with the builtin
runtime, and executes the result with the reimu RISC-V emulator.
"""

from __future__ import annotations

import argparse
import json
import shutil
import subprocess
import sys
import tempfile
from dataclasses import dataclass
from pathlib import Path
from typing import List, Optional, Sequence


DEFAULT_STAGE = "IR-1"
RISCV_TARGET = "riscv32-unknown-elf"


class Color:
    GREEN = "\033[0;32m"
    RED = "\033[0;31m"
    YELLOW = "\033[1;33m"
    BLUE = "\033[0;34m"
    NC = "\033[0m"


def colorize(msg: str, color: str) -> str:
    if not sys.stdout.isatty():
        return msg
    return f"{color}{msg}{Color.NC}"


def find_clang(explicit: Optional[str]) -> Path:
    candidates: Sequence[str]
    if explicit:
        candidates = [explicit]
    else:
        candidates = (
            "clang-18",
            "clang-17",
            "clang-16",
            "clang-15",
            "clang",
        )
    for candidate in candidates:
        clang_path = shutil.which(candidate)
        if clang_path:
            return Path(clang_path)
    raise RuntimeError(
        "Unable to find clang (looked for clang-18 ... clang). "
        "Install LLVM 15+ or pass --clang."
    )


def find_reimu(explicit: Optional[str]) -> Path:
    path = shutil.which(explicit) if explicit else shutil.which("reimu")
    if not path:
        raise RuntimeError(
            "Unable to locate the 'reimu' RISC-V simulator. "
            "Install it and ensure it is available on PATH, or pass --reimu."
        )
    return Path(path)


def canonicalize_output(text: str) -> List[str]:
    """
    Mirror the behavior of `diff -ZB`: ignore trailing whitespace and blank lines.
    """
    normalized: List[str] = []
    for line in text.splitlines():
        stripped = line.rstrip(" \t")
        if stripped == "":
            continue
        normalized.append(stripped)
    return normalized

def resolve_under_root(candidate: str, root: Path) -> Path:
    path = Path(candidate)
    if path.is_absolute():
        return path
    return (root / path).resolve()


@dataclass
class TestCase:
    name: str
    stage: str
    source: Path
    input_file: Path
    expected_output: Path


@dataclass
class TestResult:
    case: TestCase
    passed: bool
    message: str = ""
    tempdir: Optional[Path] = None


class IRTestRunner:
    def __init__(self, args: argparse.Namespace) -> None:
        self.args = args
        self.project_root = Path(__file__).resolve().parent.parent
        self.testcases_root = self.project_root / "testcases"
        self.stage_dir = self.testcases_root / args.stage
        self.compiler = resolve_under_root(args.compiler, self.project_root)
        self.builtin_c = resolve_under_root(args.builtin, self.project_root)
        self.keep_temps = args.keep_temps
        self.verbose = args.verbose
        self.fail_fast = args.fail_fast
        self.filtered = set(args.tests or [])

        self.clang = find_clang(args.clang)
        self.reimu = find_reimu(args.reimu)

        self.builtin_s: Optional[Path] = None
        self.tempdirs_to_clean: List[Path] = []

    def __del__(self) -> None:
        self.cleanup()

    # -- setup -----------------------------------------------------------------
    def ensure_environment(self) -> None:
        if not self.compiler.exists():
            raise RuntimeError(
                f"Compiler not found at {self.compiler}. "
                "Build the project first (e.g. `cmake --build build`)."
            )
        if not self.stage_dir.is_dir():
            raise RuntimeError(f"Stage directory missing: {self.stage_dir}")
        if not self.builtin_c.is_file():
            raise RuntimeError(f"Builtin runtime missing: {self.builtin_c}")

    def cleanup(self) -> None:
        if self.keep_temps:
            return
        for tmp in self.tempdirs_to_clean:
            shutil.rmtree(tmp, ignore_errors=True)
        self.tempdirs_to_clean.clear()

    def prepare_builtin(self) -> None:
        tempdir = Path(tempfile.mkdtemp(prefix="rc_ir_builtin_"))
        builtin_ll = tempdir / "builtin.ll"
        builtin_s_source = tempdir / "builtin.s.source"
        builtin_s = tempdir / "builtin.s"

        cmd_ll = [
            str(self.clang),
            "-S",
            "-emit-llvm",
            f"--target={self.args.target}",
            "-O2",
            "-fno-builtin",
            str(self.builtin_c),
            "-o",
            str(builtin_ll),
        ]
        run_command(cmd_ll, "compile builtin to LLVM IR")

        cmd_s = [
            str(self.clang),
            "-S",
            f"--target={self.args.target}",
            "-O2",
            "-fno-builtin",
            str(builtin_ll),
            "-o",
            str(builtin_s_source),
        ]
        run_command(cmd_s, "compile builtin to assembly")

        read_text = builtin_s_source.read_text()
        builtin_s.write_text(read_text.replace("@plt", ""))

        self.builtin_s = builtin_s
        if self.keep_temps:
            print(
                colorize(
                    f"Keeping builtin artifacts in {tempdir}", Color.YELLOW
                )
            )
        else:
            self.tempdirs_to_clean.append(tempdir)

    # -- manifest ---------------------------------------------------------------
    def load_testcases(self) -> List[TestCase]:
        manifest_path = self.stage_dir / "global.json"
        if not manifest_path.is_file():
            raise RuntimeError(f"global.json missing: {manifest_path}")
        tests_data = json.loads(manifest_path.read_text())
        cases: List[TestCase] = []

        for entry in tests_data:
            if not entry.get("active", True):
                continue
            name = entry.get("name")
            if not name:
                continue
            if self.filtered and name not in self.filtered:
                continue

            def resolve(field: str) -> Optional[Path]:
                payload = entry.get(field)
                if not payload:
                    return None
                rel = payload[0] if isinstance(payload, list) else payload
                return self.stage_dir / rel

            source = resolve("source")
            input_file = resolve("input")
            expected_output = resolve("output")
            if not (source and input_file and expected_output):
                continue
            cases.append(
                TestCase(
                    name=name,
                    stage=self.args.stage,
                    source=source,
                    input_file=input_file,
                    expected_output=expected_output,
                )
            )

        if self.filtered and len(cases) != len(self.filtered):
            missing = self.filtered - {c.name for c in cases}
            available = ", ".join(sorted(missing))
            raise RuntimeError(f"Requested test(s) not found: {available}")

        cases.sort(key=lambda c: c.name)
        return cases

    # -- runner ----------------------------------------------------------------
    def run_all(self) -> List[TestResult]:
        if not self.builtin_s:
            raise RuntimeError("Builtin assembly has not been prepared.")
        cases = self.load_testcases()
        if not cases:
            print("No IR testcases selected.")
            return []

        results: List[TestResult] = []
        for case in cases:
            result = self.run_case(case)
            results.append(result)
            if result.passed:
                print(colorize(f"✓ {case.name}", Color.GREEN))
            else:
                print(colorize(f"✗ {case.name}: {result.message}", Color.RED))
                if result.tempdir:
                    print(
                        colorize(
                            f"  artifacts kept in {result.tempdir}", Color.YELLOW
                        )
                    )
                if self.fail_fast:
                    break
        return results

    def run_case(self, case: TestCase) -> TestResult:
        tempdir = Path(tempfile.mkdtemp(prefix=f"rc_ir_{case.name}_"))

        try:
            program_ll = tempdir / "output.ll"
            compile_result = self.run_compiler(case, program_ll)
            if compile_result is not None:
                compile_result.tempdir = tempdir
                return compile_result

            program_s = tempdir / "output.s.source"
            final_program_s = tempdir / "output.s"
            cmd = [
                str(self.clang),
                "-S",
                f"--target={self.args.target}",
                str(program_ll),
                "-o",
                str(program_s),
            ]
            run_command(cmd, f"clang assembling {case.name}")
            program_text = program_s.read_text()
            final_program_s.write_text(program_text.replace("@plt", ""))

            actual_out = tempdir / "test.out"
            cmd_reimu = [
                str(self.reimu),
                f"-i={case.input_file}",
                f"-o={actual_out}",
                str(self.builtin_s),
                str(final_program_s),
            ]
            reimu_proc = subprocess.run(
                cmd_reimu,
                cwd=tempdir,
                capture_output=True,
                text=True,
            )
            if reimu_proc.returncode != 0:
                msg = (
                    f"reimu failed with exit code {reimu_proc.returncode}. "
                    f"stderr:\n{reimu_proc.stderr.strip()}"
                )
                return TestResult(case, False, msg, tempdir)

            expected_lines = canonicalize_output(
                case.expected_output.read_text()
            )
            actual_lines = canonicalize_output(actual_out.read_text())
            if expected_lines != actual_lines:
                preview = "\n".join(actual_out.read_text().splitlines()[:5])
                msg = "output mismatch"
                if self.verbose:
                    msg += f"\nexpected: {expected_lines}\nactual: {actual_lines}"
                else:
                    msg += f"\nactual preview:\n{preview}"
                return TestResult(case, False, msg, tempdir)

            if self.keep_temps:
                self.tempdirs_to_clean.append(tempdir)
            else:
                shutil.rmtree(tempdir, ignore_errors=True)
            return TestResult(case, True)
        except subprocess.CalledProcessError as ex:
            return TestResult(case, False, str(ex), tempdir)
        except Exception as ex:  # pylint: disable=broad-except
            return TestResult(case, False, str(ex), tempdir)

    def run_compiler(self, case: TestCase, output_ll: Path) -> Optional[TestResult]:
        cmd = [str(self.compiler), str(case.source)]
        with output_ll.open("w") as ir_out:
            proc = subprocess.run(
                cmd,
                stdout=ir_out,
                stderr=subprocess.PIPE,
                text=True,
            )
        if proc.returncode != 0:
            msg = (
                f"compiler exited with {proc.returncode} "
                f"while processing {case.source}"
            )
            if proc.stderr:
                msg += f"\nstderr:\n{proc.stderr.strip()}"
            return TestResult(case, False, msg)
        if proc.stderr and self.verbose:
            print(colorize(proc.stderr, Color.YELLOW))
        return None


def run_command(cmd: Sequence[str], description: str) -> None:
    result = subprocess.run(cmd, capture_output=True, text=True)
    if result.returncode != 0:
        raise RuntimeError(
            f"Failed to {description}.\n"
            f"Command: {' '.join(cmd)}\n"
            f"stdout:\n{result.stdout}\n"
            f"stderr:\n{result.stderr}"
        )


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Run IR stage testcases end-to-end.",
    )
    parser.add_argument(
        "--compiler",
        default="build/rcompiler",
        help="Path to the compiler executable (default: build/rcompiler)",
    )
    parser.add_argument(
        "--stage",
        default=DEFAULT_STAGE,
        help="Testcase stage folder name under testcases/ (default: IR-1)",
    )
    parser.add_argument(
        "--builtin",
        default="testcases/IR-1/builtin/builtin.c",
        help="Path to builtin runtime C source",
    )
    parser.add_argument(
        "--clang",
        default=None,
        help="clang executable to use (auto-detect if omitted)",
    )
    parser.add_argument(
        "--reimu",
        default=None,
        help="Path to reimu executable (auto-detect if omitted)",
    )
    parser.add_argument(
        "--target",
        default=RISCV_TARGET,
        help=f"Target triple for clang (default: {RISCV_TARGET})",
    )
    parser.add_argument(
        "--tests",
        nargs="+",
        help="Only run the specified test names",
    )
    parser.add_argument(
        "--keep-temps",
        action="store_true",
        help="Keep temporary directories for inspection",
    )
    parser.add_argument(
        "--verbose",
        action="store_true",
        help="Show additional diagnostics",
    )
    parser.add_argument(
        "--fail-fast",
        action="store_true",
        help="Abort after the first failing testcase",
    )
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    runner = IRTestRunner(args)
    runner.ensure_environment()
    runner.prepare_builtin()
    results = runner.run_all()

    passed = sum(1 for r in results if r.passed)
    total = len(results)
    print()
    print(
        colorize(
            f"IR tests: {passed}/{total} passed",
            Color.GREEN if passed == total else Color.RED,
        )
    )
    return 0 if passed == total else 1


if __name__ == "__main__":
    sys.exit(main())

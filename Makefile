# RCompiler Makefile for Online Judge Testing
# - make build: Compile the compiler
# - make run: Run from STDIN, output RV64GC assembly to STDOUT, builtin.s to STDERR

# Build directory
BUILD_DIR := build

# RV64 builtin runtime assembly emitted on stderr by the judge protocol
BUILTIN_ASM := ci/files/IR/builtin.s

.PHONY: build run clean

# Compile the compiler
build:
	@mkdir -p $(BUILD_DIR)
	@cd $(BUILD_DIR) && cmake .. -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF >/dev/null 2>&1
	@cmake --build $(BUILD_DIR) --target rcompiler -j$(shell sysctl -n hw.ncpu 2>/dev/null || echo 4) >/dev/null 2>&1

# Run: read Rx from STDIN, output RV64GC assembly to STDOUT, builtin.s to STDERR
run: build
	@cat $(BUILTIN_ASM) >&2
	@$(BUILD_DIR)/rcompiler --emit-asm-rv64

# Clean build artifacts
clean:
	@rm -rf $(BUILD_DIR)

#!/bin/bash

# RCompiler Semantic Analysis Benchmark Script
# Tests semantic analysis by comparing compiler exit codes with expected results

set -e

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

# Configuration
BUILD_DIR="build"
TESTCASES_DIR="testcases" 
COMPILER_EXEC="rcompiler"
VERBOSE=false
STAGE_FILTER="semantic-1"

# Stats
TOTAL_TESTS=0
PASSED_TESTS=0
FAILED_TESTS=0

# Simple output functions
print_success() { echo -e "${GREEN}✓ $1${NC}"; }
print_error() { echo -e "${RED}✗ $1${NC}"; }
print_info() { echo -e "${BLUE}ℹ $1${NC}"; }

# Usage
usage() {
    cat << EOF
Usage: $0 [OPTIONS]

Test semantic analysis against testcases.

OPTIONS:
    -h, --help          Show help
    -v, --verbose       Show compiler output
    -s, --stage STAGE   Test stage (default: semantic-1)
    -q, --quiet         Only show summary

EXAMPLES:
    $0                  # Test semantic-1 with summary
    $0 -v               # Test with verbose output  
    $0 -s semantic-2    # Test different stage
    $0 -q               # Quiet mode
EOF
}

# Parse arguments
parse_args() {
    while [[ $# -gt 0 ]]; do
        case $1 in
            -h|--help) usage; exit 0 ;;
            -v|--verbose) VERBOSE=true; shift ;;
            -s|--stage) STAGE_FILTER="$2"; shift 2 ;;
            -q|--quiet) QUIET=true; shift ;;
            *) echo "Unknown option: $1"; usage; exit 1 ;;
        esac
    done
}

# Check environment
check_env() {
    if [[ ! -f "$BUILD_DIR/$COMPILER_EXEC" ]]; then
        print_error "Compiler not found: $BUILD_DIR/$COMPILER_EXEC"
        echo "Run: cmake --build build"
        exit 1
    fi
    
    if ! command -v jq &> /dev/null; then
        print_error "jq is required: brew install jq"
        exit 1
    fi
}

# Find testcases
find_testcases() {
    find "$TESTCASES_DIR" -name "testcase_info.json" | while read -r metadata_file; do
        local dir=$(dirname "$metadata_file")
        local testname=$(basename "$dir")
        local stage=$(basename "$(dirname "$dir")")
        
        # Filter by stage
        if [[ -n "$STAGE_FILTER" && "$stage" != "$STAGE_FILTER" ]]; then
            continue
        fi
        
        # Check if .rx file exists
        if [[ -f "$dir/$testname.rx" ]]; then
            echo "$dir"
        fi
    done
}

# Run single test
run_test() {
    local testdir="$1"
    local testname=$(basename "$testdir")
    local stage=$(basename "$(dirname "$testdir")")
    local rx_file="$testdir/$testname.rx"
    local metadata_file="$testdir/testcase_info.json"
    
    # Get expected exit code
    local expected=$(jq -r '.compileexitcode // "UNKNOWN"' "$metadata_file" 2>/dev/null)
    if [[ "$expected" == "UNKNOWN" ]]; then
        return 1
    fi
    
    # Run compiler
    local actual=0
    local output=""
    output=$("$BUILD_DIR/$COMPILER_EXEC" < "$rx_file" 2>&1) || actual=$?
    
    # Check result
    local status="FAIL"
    if [[ "$expected" == "0" && "$actual" == "0" ]] || [[ "$expected" == "-1" && "$actual" != "0" ]]; then
        status="PASS"
        ((PASSED_TESTS++))
    else
        ((FAILED_TESTS++))
    fi
    
    ((TOTAL_TESTS++))
    
    # Output result
    if [[ "$QUIET" != true ]]; then
        if [[ "$status" == "PASS" ]]; then
            print_success "$stage/$testname"
        else
            print_error "$stage/$testname (expected: $expected, got: $actual)"
            if [[ "$VERBOSE" == true ]]; then
                echo "$output" | head -5 | sed 's/^/  /'
            fi
        fi
    fi
}

# Run all tests
run_all_tests() {
    local testcases=($(find_testcases))
    TOTAL_TESTS=0
    PASSED_TESTS=0 
    FAILED_TESTS=0
    
    if [[ ${#testcases[@]} -eq 0 ]]; then
        print_error "No testcases found for stage: $STAGE_FILTER"
        exit 1
    fi
    
    print_info "Running ${#testcases[@]} testcases for stage: $STAGE_FILTER"
    
    for testdir in "${testcases[@]}"; do
        run_test "$testdir"
    done
}

# Print summary
print_summary() {
    local pass_rate=0
    if [[ "$TOTAL_TESTS" -gt 0 ]]; then
        pass_rate=$(( PASSED_TESTS * 100 / TOTAL_TESTS ))
    fi
    
    echo
    echo "Results: $PASSED_TESTS/$TOTAL_TESTS passed ($pass_rate%)"
    
    if [[ "$FAILED_TESTS" -eq 0 ]]; then
        print_success "All tests passed!"
        exit 0
    else
        print_error "$FAILED_TESTS tests failed"
        exit 1
    fi
}

# Main
main() {
    parse_args "$@"
    check_env
    run_all_tests
    print_summary
}

main "$@"

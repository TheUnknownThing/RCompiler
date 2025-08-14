#!/bin/bash

# RCompiler Parser Test Runner
# Provides convenient interface for running parser tests

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Default values
BUILD_DIR="build"
VERBOSE=false
CATEGORY=""

# Function to print colored output
print_header() {
    echo -e "${BLUE}=== $1 ===${NC}"
}

print_success() {
    echo -e "${GREEN}✓ $1${NC}"
}

print_error() {
    echo -e "${RED}✗ $1${NC}"
}

print_warning() {
    echo -e "${YELLOW}⚠ $1${NC}"
}

# Function to show usage
usage() {
    cat << EOF
Usage: $0 [OPTIONS] [CATEGORY]

RCompiler Parser Test Runner

OPTIONS:
    -h, --help          Show this help message
    -v, --verbose       Enable verbose output
    -b, --build-dir DIR Specify build directory (default: build)
    --build             Build project before running tests
    --clean             Clean build before running tests

CATEGORIES:
    all                 Run all tests (default)
    unit                Run all unit tests
    parser              Run all parser tests  
    pattern             Run pattern parsing tests
    error               Run error handling tests
    stable              Run only stable tests
    unstable            Run unstable tests (may fail)
    integration         Run integration tests
    lexer               Run lexer tests
    preprocessor        Run preprocessor tests

EXAMPLES:
    $0                  # Run all tests
    $0 unit             # Run unit tests only
    $0 -v parser        # Run parser tests with verbose output
    $0 --build stable   # Build and run stable tests
    $0 --clean all      # Clean build and run all tests

EOF
}

# Parse command line arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        -h|--help)
            usage
            exit 0
            ;;
        -v|--verbose)
            VERBOSE=true
            shift
            ;;
        -b|--build-dir)
            BUILD_DIR="$2"
            shift 2
            ;;
        --build)
            BUILD=true
            shift
            ;;
        --clean)
            CLEAN=true
            BUILD=true
            shift
            ;;
        -*)
            echo "Unknown option $1"
            usage
            exit 1
            ;;
        *)
            CATEGORY="$1"
            shift
            ;;
    esac
done

# Set default category
if [[ -z "$CATEGORY" ]]; then
    CATEGORY="all"
fi

# Check if build directory exists
if [[ ! -d "$BUILD_DIR" ]]; then
    print_error "Build directory '$BUILD_DIR' does not exist"
    echo "Run cmake to configure the project first, or use --build-dir to specify correct path"
    exit 1
fi

cd "$BUILD_DIR"

# Clean build if requested
if [[ "$CLEAN" == true ]]; then
    print_header "Cleaning Build"
    make clean || true
fi

# Build if requested
if [[ "$BUILD" == true ]]; then
    print_header "Building Project"
    make -j$(nproc)
fi

# Prepare ctest arguments
CTEST_ARGS="--output-on-failure"
if [[ "$VERBOSE" == true ]]; then
    CTEST_ARGS="$CTEST_ARGS --verbose"
fi

# Function to run tests with category
run_tests() {
    local category="$1"
    local label="$2"
    local description="$3"
    
    print_header "$description"
    
    if [[ -n "$label" ]]; then
        if ctest $CTEST_ARGS -L "$label"; then
            print_success "$description completed successfully"
            return 0
        else
            print_error "$description failed"
            return 1
        fi
    else
        if ctest $CTEST_ARGS; then
            print_success "$description completed successfully"
            return 0
        else
            print_error "$description failed"
            return 1
        fi
    fi
}

# Function to run specific test
run_specific_test() {
    local test_name="$1"
    local description="$2"
    
    print_header "$description"
    
    if ctest $CTEST_ARGS -R "$test_name"; then
        print_success "$description completed successfully"
        return 0
    else
        print_error "$description failed"
        return 1
    fi
}

# Track overall success
overall_success=true

# Run tests based on category
case "$CATEGORY" in
    all)
        run_tests "all" "" "All Tests" || overall_success=false
        ;;
    unit)
        run_tests "unit" "unit" "Unit Tests" || overall_success=false
        ;;
    parser)
        run_tests "parser" "parser" "Parser Tests" || overall_success=false
        ;;
    pattern)
        run_tests "pattern" "pattern" "Pattern Parsing Tests" || overall_success=false
        ;;
    error)
        run_tests "error" "error" "Error Handling Tests" || overall_success=false
        ;;
    stable)
        run_tests "stable" "stable" "Stable Tests" || overall_success=false
        ;;
    unstable)
        print_warning "Running unstable tests (may fail)"
        run_tests "unstable" "unstable" "Unstable Tests"
        # Don't mark overall failure for unstable tests
        ;;
    integration)
        run_tests "integration" "integration" "Integration Tests" || overall_success=false
        ;;
    lexer)
        run_tests "lexer" "lexer" "Lexer Tests" || overall_success=false
        ;;
    preprocessor)
        run_tests "preprocessor" "preprocessor" "Preprocessor Tests" || overall_success=false
        ;;
    # Specific test names
    parser_unit_tests)
        run_specific_test "parser_unit_tests" "Parser Unit Tests" || overall_success=false
        ;;
    parser_advanced_tests)
        run_specific_test "parser_advanced_tests" "Parser Advanced Tests" || overall_success=false
        ;;
    parser_pattern_tests)
        run_specific_test "parser_pattern_tests" "Parser Pattern Tests" || overall_success=false
        ;;
    parser_error_tests)
        run_specific_test "parser_error_tests" "Parser Error Tests" || overall_success=false
        ;;
    *)
        print_error "Unknown test category: $CATEGORY"
        echo ""
        echo "Available categories:"
        echo "  all, unit, parser, pattern, error, stable, unstable, integration, lexer, preprocessor"
        echo ""
        echo "Available specific tests:"
        echo "  parser_unit_tests, parser_advanced_tests, parser_pattern_tests, parser_error_tests"
        exit 1
        ;;
esac

# Print summary
echo ""
if [[ "$overall_success" == true ]]; then
    print_success "All requested tests completed successfully!"
    exit 0
else
    print_error "Some tests failed!"
    exit 1
fi

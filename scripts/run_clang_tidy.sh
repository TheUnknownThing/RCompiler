#!/bin/bash

# RCompiler clang-tidy runner script
# This script runs clang-tidy on all C++ source files in the project

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Configuration
CLANG_TIDY_BIN="/opt/homebrew/opt/llvm/bin/clang-tidy"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
BUILD_DIR="$PROJECT_ROOT/build"

# Check if clang-tidy exists
if [ ! -f "$CLANG_TIDY_BIN" ]; then
    echo -e "${RED}Error: clang-tidy not found at $CLANG_TIDY_BIN${NC}"
    echo -e "${YELLOW}Please install LLVM: brew install llvm${NC}"
    exit 1
fi

# Check if build directory exists
if [ ! -d "$BUILD_DIR" ]; then
    echo -e "${YELLOW}Build directory not found. Creating and building project...${NC}"
    cd "$PROJECT_ROOT"
    mkdir -p build
    cd build
    cmake ..
    make -j$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)
fi

# Check if compile_commands.json exists
if [ ! -f "$BUILD_DIR/compile_commands.json" ]; then
    echo -e "${YELLOW}compile_commands.json not found. Regenerating...${NC}"
    cd "$BUILD_DIR"
    cmake -DCMAKE_EXPORT_COMPILE_COMMANDS=ON ..
fi

echo -e "${BLUE}Running clang-tidy on RCompiler project...${NC}"

# Default mode: check all files
MODE="check"
FIX_MODE=""
SPECIFIC_FILE=""

# Parse command line arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        --fix)
            FIX_MODE="--fix"
            echo -e "${YELLOW}Running in fix mode - will apply suggested fixes${NC}"
            shift
            ;;
        --file)
            SPECIFIC_FILE="$2"
            shift 2
            ;;
        --help|-h)
            echo "Usage: $0 [OPTIONS]"
            echo "Options:"
            echo "  --fix         Apply clang-tidy fixes automatically"
            echo "  --file FILE   Run clang-tidy on specific file"
            echo "  --help, -h    Show this help message"
            exit 0
            ;;
        *)
            echo -e "${RED}Unknown option: $1${NC}"
            exit 1
            ;;
    esac
done

# Change to project root
cd "$PROJECT_ROOT"

# Function to run clang-tidy on a file
run_clang_tidy() {
    local file="$1"
    echo -e "${BLUE}Analyzing: $file${NC}"
    
    if $CLANG_TIDY_BIN \
        -p="$BUILD_DIR" \
        $FIX_MODE \
        "$file" 2>&1; then
        echo -e "${GREEN}✓ $file passed${NC}"
        return 0
    else
        echo -e "${RED}✗ $file has issues${NC}"
        return 1
    fi
}

# Find source files to analyze
if [ -n "$SPECIFIC_FILE" ]; then
    if [ ! -f "$SPECIFIC_FILE" ]; then
        echo -e "${RED}Error: File $SPECIFIC_FILE not found${NC}"
        exit 1
    fi
    FILES=("$SPECIFIC_FILE")
else
    # Find all C++ source files (POSIX-compatible, no mapfile)
    FILES=()
    while IFS= read -r file; do
        FILES+=("$file")
    done < <(find src include tests -name "*.cpp" -o -name "*.hpp" | grep -v "\.dir/" | sort)
fi

# Statistics
TOTAL_FILES=${#FILES[@]}
PASSED_FILES=0
FAILED_FILES=0

echo -e "${BLUE}Found $TOTAL_FILES files to analyze${NC}"

# Run clang-tidy on each file
for file in "${FILES[@]}"; do
    if run_clang_tidy "$file"; then
        ((PASSED_FILES++))
    else
        ((FAILED_FILES++))
    fi
    echo
done

# Summary
echo -e "${BLUE}=== SUMMARY ===${NC}"
echo -e "Total files analyzed: $TOTAL_FILES"
echo -e "${GREEN}Passed: $PASSED_FILES${NC}"
if [ $FAILED_FILES -gt 0 ]; then
    echo -e "${RED}Failed: $FAILED_FILES${NC}"
    exit 1
else
    echo -e "${GREEN}All files passed clang-tidy checks!${NC}"
fi

# RCompiler Makefile for Online Judge Testing
# - make build: Compile the compiler
# - make run: Run from STDIN, output IR to STDOUT, builtin to STDERR

# Build directory
BUILD_DIR := build

# Builtin file location
BUILTIN := ci/files/IR/builtin.c

.PHONY: build run clean

# Compile the compiler
build:
	@mkdir -p $(BUILD_DIR)
	@cd $(BUILD_DIR) && cmake .. -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF >/dev/null 2>&1
	@cd $(BUILD_DIR) && make rcompiler -j$(shell sysctl -n hw.ncpu 2>/dev/null || echo 4) >/dev/null 2>&1

# Run: read from STDIN, output IR to STDOUT, builtin to STDERR
run:
	@cat $(BUILTIN) >&2
	@$(BUILD_DIR)/rcompiler

# Clean build artifacts
clean:
	@rm -rf $(BUILD_DIR)

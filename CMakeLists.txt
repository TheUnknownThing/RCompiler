cmake_minimum_required(VERSION 3.16)
project(RCompiler VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Include directories
include_directories(include)

# Source files
set(SOURCES src/main.cpp)

# Header files
set(HEADERS include/*)

# Create executable
add_executable(rcompiler ${SOURCES} ${HEADERS})

# Compiler flags
target_compile_options(rcompiler PRIVATE -Wall -Wextra -std=c++20)

# Enable testing
# enable_testing()
# add_subdirectory(tests)
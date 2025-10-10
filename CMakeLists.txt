cmake_minimum_required(VERSION 3.16)
project(RCompiler VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Include directories (apply globally for simplicity)
include_directories(include)

# Source files
set(SOURCES 
    src/main.cpp
)

# Header files (not used for build, but helpful for IDEs)
file(GLOB_RECURSE HEADERS "include/*.hpp")

# Create executable
add_executable(rcompiler ${SOURCES})

# Compiler flags
if (CMAKE_CXX_COMPILER_ID MATCHES "Clang" OR CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  target_compile_options(rcompiler PRIVATE -Wall -Wextra -O3 -std=c++20)
endif()

# --- Testing setup ---
include(CTest)
option(BUILD_TESTING "Build tests" OFF)

if (BUILD_TESTING)
  enable_testing()

  
endif()

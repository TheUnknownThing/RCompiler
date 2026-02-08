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
option(BUILD_TESTING "Build tests" ON)

if (BUILD_TESTING)
  enable_testing()

  add_executable(ci_preprocessor_tests tests/ci_preprocessor_tests.cpp)
  target_compile_definitions(ci_preprocessor_tests PRIVATE PROJECT_ROOT_DIR="${CMAKE_SOURCE_DIR}" LOGGING_LEVEL_WARN)

  add_executable(ci_lexer_tests tests/ci_lexer_tests.cpp)
  target_compile_definitions(ci_lexer_tests PRIVATE PROJECT_ROOT_DIR="${CMAKE_SOURCE_DIR}" LOGGING_LEVEL_WARN)

  add_executable(ci_parser_tests tests/ci_parser_tests.cpp)
  target_compile_definitions(ci_parser_tests PRIVATE PROJECT_ROOT_DIR="${CMAKE_SOURCE_DIR}" LOGGING_LEVEL_WARN)

  add_executable(ci_dce_tests tests/ci_dce_tests.cpp)
  target_compile_definitions(ci_dce_tests PRIVATE LOGGING_LEVEL_WARN)

  add_test(NAME preprocessor_ci COMMAND ci_preprocessor_tests)
  set_tests_properties(preprocessor_ci PROPERTIES LABELS "ci;preprocessor")

  add_test(NAME lexer_ci COMMAND ci_lexer_tests)
  set_tests_properties(lexer_ci PROPERTIES LABELS "ci;lexer")

  add_test(NAME parser_ci COMMAND ci_parser_tests)
  set_tests_properties(parser_ci PROPERTIES LABELS "ci;parser")

  add_test(NAME dce_ci COMMAND ci_dce_tests)
  set_tests_properties(dce_ci PROPERTIES LABELS "ci;opt;dce")
endif()

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

# Function to apply clang-tidy to test executables
function(add_test_executable target_name source_files)
    add_executable(${target_name} ${source_files})
endfunction()

# --- Testing setup ---
include(CTest)
option(BUILD_TESTING "Build tests" OFF)

if (BUILD_TESTING)
  enable_testing()

  # Unit: preprocessor existing test
  add_test_executable(unit_preprocessor tests/unit/preprocessor.cpp)
  add_test(NAME unit_preprocessor COMMAND unit_preprocessor)
  set_tests_properties(unit_preprocessor PROPERTIES WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/tests/unit" LABELS "preprocessor;stable")

  # Unit: Constant evaluator test
  add_test_executable(const_evaluator_test tests/unit/const_evaluator_test.cpp)
  add_test(NAME const_evaluator_test COMMAND const_evaluator_test)
  set_tests_properties(const_evaluator_test PROPERTIES WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}" LABELS "semantic;stable")

  # Unit: Simple parser unit tests
  add_test_executable(parser_simple_tests tests/unit/parser_simple_tests.cpp)
  add_test(NAME parser_simple_tests COMMAND parser_simple_tests)
  set_tests_properties(parser_simple_tests PROPERTIES WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}" LABELS "parser;legacy")

  # New: parser basic aggregated test (valid vs invalid classification)
  add_test_executable(parser_basic tests/unit/parser_basic.cpp)
  add_test(NAME parser_basic COMMAND parser_basic)
  set_tests_properties(parser_basic PROPERTIES WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}" LABELS "parser;basic;stable")

  # New: parser comprehensive aggregated test (full dataset, pretty print)
  add_test_executable(parser_comprehensive "tests/unit/parser_comprehensive.cpp")
  add_test(NAME parser_comprehensive COMMAND parser_comprehensive)
  set_tests_properties(parser_comprehensive PROPERTIES WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}" LABELS "parser;comprehensive;unstable")

  # Unit: Comprehensive parser tests (AST validation) - for manual testing
  add_test_executable(parser_comprehensive_runner "tests/unit/parser_comprehensive_runner.cpp")

  # Lexer case runner: compares tokens with expected files
  add_test_executable(lexer_case_runner tests/unit/lexer_case_runner.cpp)

  # Register all lexer example tests (.tokens paired with .rs)
  file(GLOB LEXER_TOKEN_FILES "${CMAKE_SOURCE_DIR}/examples/lexer/*.tokens")
  foreach(token_file IN LISTS LEXER_TOKEN_FILES)
    get_filename_component(case_name "${token_file}" NAME_WE)
    set(rs_file "${CMAKE_SOURCE_DIR}/examples/lexer/${case_name}.rs")
    if (EXISTS "${rs_file}")
      set(test_name lexer_${case_name})
      add_test(NAME ${test_name}
               COMMAND lexer_case_runner "${rs_file}" "${token_file}")
      set_tests_properties(${test_name} PROPERTIES LABELS "lexer;stable")
      if (case_name STREQUAL "char_literals" OR
          case_name STREQUAL "integer_followed_by_identifier" OR
          case_name STREQUAL "unterminated_string")
        set_tests_properties(${test_name} PROPERTIES LABELS "lexer;unstable")
      endif()
    endif()
  endforeach()

  # Parser smoke runner: ensures parsing succeeds on AST examples
  add_test_executable(parser_smoke_runner tests/unit/parser_smoke_runner.cpp)

  file(GLOB AST_RS_FILES "${CMAKE_SOURCE_DIR}/examples/ast/*.rs")
  foreach(ast_file IN LISTS AST_RS_FILES)
    get_filename_component(ast_name "${ast_file}" NAME_WE)
    set(test_name parser_ast_${ast_name})
    add_test(NAME ${test_name}
             COMMAND parser_smoke_runner "${ast_file}")
    set_tests_properties(${test_name} PROPERTIES LABELS "parser;stable")
    if (ast_name STREQUAL "8" OR ast_name STREQUAL "9" OR ast_name STREQUAL "13" OR
        ast_name STREQUAL "14" OR ast_name STREQUAL "15" OR ast_name STREQUAL "16" OR
        ast_name STREQUAL "18" OR ast_name STREQUAL "19" OR ast_name STREQUAL "20" OR
        ast_name STREQUAL "22" OR ast_name STREQUAL "23" OR ast_name STREQUAL "24" OR
        ast_name STREQUAL "25" OR ast_name STREQUAL "26")
      set_tests_properties(${test_name} PROPERTIES LABELS "parser;unstable")
    endif()
  endforeach()
  
  # Optional: integration sample (does not assert), keep as smoke
  add_test_executable(integration_lexer_with_preprocessor tests/integration/lexer_w_preprocessor.cpp)
  add_test(NAME integration_lexer_with_preprocessor COMMAND integration_lexer_with_preprocessor)
  set_tests_properties(integration_lexer_with_preprocessor PROPERTIES WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/tests/integration" LABELS "integration;stable")

  # Semantic analysis benchmark test
  add_test(NAME semantic_benchmark 
           COMMAND ${CMAKE_SOURCE_DIR}/scripts/semantic_benchmark.sh -q
           WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
  set_tests_properties(semantic_benchmark PROPERTIES LABELS "semantic;benchmark")
endif()

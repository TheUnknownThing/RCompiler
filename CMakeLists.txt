cmake_minimum_required(VERSION 3.16)
project(RCompiler VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Header files (not used for build, but helpful for IDEs)
file(GLOB_RECURSE HEADERS CONFIGURE_DEPENDS "include/*.hpp")
file(GLOB_RECURSE PROJECT_SOURCES CONFIGURE_DEPENDS "src/*.cpp")
list(REMOVE_ITEM PROJECT_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp")

add_library(rcompiler_core INTERFACE)
target_sources(rcompiler_core INTERFACE ${PROJECT_SOURCES} ${HEADERS})
target_include_directories(rcompiler_core INTERFACE include)

# Compiler flags
if (CMAKE_CXX_COMPILER_ID MATCHES "Clang" OR CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  target_compile_options(rcompiler_core INTERFACE -Wall -Wextra -O3 -std=c++20)
endif()

# Create executable
add_executable(rcompiler src/main.cpp)
target_link_libraries(rcompiler PRIVATE rcompiler_core)
target_compile_definitions(rcompiler PRIVATE LOGGING_LEVEL_NONE)

# --- Testing setup ---
include(CTest)
option(BUILD_TESTING "Build tests" ON)

if (BUILD_TESTING)
  enable_testing()

  add_executable(ci_preprocessor_tests tests/ci_preprocessor_tests.cpp)
  target_link_libraries(ci_preprocessor_tests PRIVATE rcompiler_core)
  target_compile_definitions(ci_preprocessor_tests PRIVATE PROJECT_ROOT_DIR="${CMAKE_SOURCE_DIR}" LOGGING_LEVEL_WARN)

  add_executable(ci_lexer_tests tests/ci_lexer_tests.cpp)
  target_link_libraries(ci_lexer_tests PRIVATE rcompiler_core)
  target_compile_definitions(ci_lexer_tests PRIVATE PROJECT_ROOT_DIR="${CMAKE_SOURCE_DIR}" LOGGING_LEVEL_WARN)

  add_executable(ci_parser_tests tests/ci_parser_tests.cpp)
  target_link_libraries(ci_parser_tests PRIVATE rcompiler_core)
  target_compile_definitions(ci_parser_tests PRIVATE PROJECT_ROOT_DIR="${CMAKE_SOURCE_DIR}" LOGGING_LEVEL_WARN)

  add_executable(ci_dce_tests tests/ci_dce_tests.cpp)
  target_link_libraries(ci_dce_tests PRIVATE rcompiler_core)
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

#include <cassert>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include "../../include/ast/parser.hpp"
#include "../../include/semantic/analyzer/symbolTable.hpp"

namespace rc {

class SymbolTestsRunner {
  int passed_ = 0;
  int total_ = 0;

  void assert_true(bool cond, const std::string &name, const std::string &msg = "") {
    ++total_;
    if (cond) {
      ++passed_;
      std::cout << "\xE2\x9C\x93 " << name << "\n";
    } else {
      std::cout << "\xE2\x9C\x97 " << name;
      if (!msg.empty()) std::cout << " - " << msg;
      std::cout << "\n";
    }
  }

public:
  void run_all() {
    std::cout << "=== Symbol Table Unit Tests ===\n";
    test_scope_basic();
    test_function_and_params();
    test_struct_enum_module_symbols();
    test_duplicate_detection();
    test_nested_modules_scopes();
    print_summary();
  }

  void print_summary() const {
    std::cout << "\nSummary: " << passed_ << "/" << total_ << " passed" << std::endl;
  }

  void test_scope_basic() {
    SymbolTable st;
    assert_true(st.depth() == 1, "initial_scope_depth");

    Symbol v{"x", SymbolKind::Variable};
    v.is_mutable = true;
    v.type = LiteralType::base(PrimitiveLiteralType::I32);
    assert_true(st.declare(v), "declare_variable");
    assert_true(st.lookup("x").has_value(), "lookup_declared");
    assert_true(st.contains("x"), "contains_declared");

    st.enterScope();
    assert_true(st.depth() == 2, "enter_scope_depth");
    assert_true(st.lookup("x").has_value(), "lookup_outer_visible");
    Symbol v2{"x", SymbolKind::Variable};
    assert_true(st.declare(v2), "shadow_outer_symbol");
    st.exitScope();
    assert_true(st.depth() == 1, "exit_scope_depth");
  }

  void test_function_and_params() {
    // Build a simple AST: fn add(x: i32, y: i32) -> i32 { x }
    std::string code = "fn add(x: i32, y: i32) -> i32 { x }";
    Lexer lex(code);
    auto toks = lex.tokenize();
    Parser p(toks);
    auto root = p.parse();

    SymbolTable st;
    SymbolChecker checker(st);
    checker.build(root);

    auto f = st.lookup("add");
    assert_true(f.has_value(), "function_declared");
    assert_true(f->kind == SymbolKind::Function, "function_kind");
    assert_true(f->function_sig.has_value(), "function_sig_present");
    assert_true(f->function_sig->parameters.has_value(), "function_params_present");
    assert_true(f->function_sig->parameters->size() == 2, "function_param_count");

    // Ensure params are in function scope only (not in global after exit)
    assert_true(!st.lookup("x").has_value(), "param_not_in_global_scope");
  }

  void test_struct_enum_module_symbols() {
    std::string code = R"RS(
      struct Point { x: i32, y: i32 }
      struct Color(i32, i32);
      enum E { A, B, C }
      mod m { fn f() {} }
    )RS";

    Lexer lex(code);
    auto toks = lex.tokenize();
    Parser p(toks);
    auto root = p.parse();

    SymbolTable st;
    SymbolChecker checker(st);
    checker.build(root);

    // Struct Point
    auto s1 = st.lookup("Point");
    assert_true(s1.has_value(), "struct_point_declared");
    assert_true(s1->kind == SymbolKind::Struct, "struct_point_kind");
    assert_true(s1->struct_info.has_value(), "struct_point_info");
    assert_true(!s1->struct_info->is_tuple, "struct_point_is_named");
    assert_true(s1->struct_info->fields.size() == 2, "struct_point_field_count");

    // Tuple struct Color
    auto s2 = st.lookup("Color");
    assert_true(s2.has_value(), "struct_color_declared");
    assert_true(s2->struct_info->is_tuple, "struct_color_is_tuple");
    assert_true(s2->struct_info->tuple_fields.size() == 2, "struct_color_field_count");

    // Enum E
    auto e = st.lookup("E");
    assert_true(e.has_value(), "enum_declared");
    assert_true(e->kind == SymbolKind::Enum, "enum_kind");
    assert_true(e->enum_info.has_value(), "enum_info_present");
    assert_true(e->enum_info->variants.size() == 3, "enum_variant_count");

    // Module m
    auto m = st.lookup("m");
    assert_true(m.has_value(), "module_declared");
    assert_true(m->kind == SymbolKind::Module, "module_kind");
    assert_true(m->module_info.has_value(), "module_info_present");
    assert_true(!m->module_info->path.empty(), "module_pathnonempty");
  }

  void test_duplicate_detection() {
    std::string code = R"RS(
      fn f() {}
      fn f() {}
    )RS";

    Lexer lex(code);
    auto toks = lex.tokenize();
    Parser p(toks);
    auto root = p.parse();

    SymbolTable st;
    SymbolChecker checker(st);
    bool threw = false;
    try {
      checker.build(root);
    } catch (const SemanticException &) {
      threw = true;
    }
    assert_true(threw, "duplicate_function_detected");
  }

  void test_nested_modules_scopes() {
    std::string code = R"RS(
      mod a { mod b { const X: i32 = 1; } }
    )RS";

    Lexer lex(code);
    auto toks = lex.tokenize();
    Parser p(toks);
    auto root = p.parse();

    SymbolTable st;
    SymbolChecker checker(st);
    checker.build(root);

    // Only modules 'a' and 'b' are in their respective scopes; const X is inside b's scope
    auto a = st.lookup("a");
    auto b = st.lookup("b");
    auto x = st.lookup("X");
    assert_true(a.has_value(), "module_a_present");
    assert_true(!b.has_value(), "module_b_not_in_global_scope");
    assert_true(!x.has_value(), "const_X_not_in_global_scope");
  }
};

} // namespace rc

int main() {
  rc::SymbolTestsRunner t;
  t.run_all();
  return 0; // Do not enforce pass rate; individual asserts indicate status
}

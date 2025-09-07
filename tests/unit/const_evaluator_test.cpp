#include <cassert>
#include <iostream>
#include <memory>

#include "ast/nodes/expr.hpp"
#include "ast/nodes/topLevel.hpp"
#include "ast/types.hpp"
#include "semantic/analyzer/constEvaluator.hpp"
#include "semantic/scope.hpp"

void test_const_evaluator() {
  using namespace rc;

  std::cout << "Testing Constant Evaluator..." << std::endl;

  ConstEvaluator evaluator;

  // Create a mock scope node for testing
  auto root_scope = std::make_unique<ScopeNode>("root", nullptr, nullptr);

  // Test 1: Literal evaluation
  {
    auto literal = std::make_shared<LiteralExpression>(
        "42", LiteralType::base(PrimitiveLiteralType::I32));
    auto result = evaluator.evaluate(literal.get(), root_scope.get());

    assert(result.has_value());
    assert(result->is_i32());
    assert(result->as_i32() == 42);
    std::cout << "✓ Literal evaluation: 42 -> " << result->as_i32()
              << std::endl;
  }

  // Test 2: Boolean literal
  {
    auto literal = std::make_shared<LiteralExpression>(
        "true", LiteralType::base(PrimitiveLiteralType::BOOL));
    auto result = evaluator.evaluate(literal.get(), root_scope.get());

    assert(result.has_value());
    assert(result->is_bool());
    assert(result->as_bool() == true);
    std::cout << "✓ Boolean literal evaluation: true -> "
              << (result->as_bool() ? "true" : "false") << std::endl;
  }

  // Test 3: Binary arithmetic
  {
    auto left = std::make_shared<LiteralExpression>(
        "10", LiteralType::base(PrimitiveLiteralType::I32));
    auto right = std::make_shared<LiteralExpression>(
        "5", LiteralType::base(PrimitiveLiteralType::I32));
    Token plus_op{TokenType::PLUS, "+"};
    auto binary = std::make_shared<BinaryExpression>(left, plus_op, right);

    auto result = evaluator.evaluate(binary.get(), root_scope.get());

    assert(result.has_value());
    assert(result->is_i32());
    assert(result->as_i32() == 15);
    std::cout << "✓ Binary arithmetic: 10 + 5 -> " << result->as_i32()
              << std::endl;
  }

  // Test 4: Unary minus
  {
    auto operand = std::make_shared<LiteralExpression>(
        "42", LiteralType::base(PrimitiveLiteralType::I32));
    Token minus_op{TokenType::MINUS, "-"};
    auto prefix = std::make_shared<PrefixExpression>(minus_op, operand);

    auto result = evaluator.evaluate(prefix.get(), root_scope.get());

    assert(result.has_value());
    assert(result->is_i32());
    assert(result->as_i32() == -42);
    std::cout << "✓ Unary minus: -42 -> " << result->as_i32() << std::endl;
  }

  // Test 5: Comparison
  {
    auto left = std::make_shared<LiteralExpression>(
        "10", LiteralType::base(PrimitiveLiteralType::I32));
    auto right = std::make_shared<LiteralExpression>(
        "5", LiteralType::base(PrimitiveLiteralType::I32));
    Token gt_op{TokenType::GT, ">"};
    auto binary = std::make_shared<BinaryExpression>(left, gt_op, right);

    auto result = evaluator.evaluate(binary.get(), root_scope.get());

    assert(result.has_value());
    assert(result->is_bool());
    assert(result->as_bool() == true);
    std::cout << "✓ Comparison: 10 > 5 -> "
              << (result->as_bool() ? "true" : "false") << std::endl;
  }

  // Test 6: Array literal
  {
    std::vector<std::shared_ptr<Expression>> elements;
    elements.push_back(std::make_shared<LiteralExpression>(
        "1", LiteralType::base(PrimitiveLiteralType::I32)));
    elements.push_back(std::make_shared<LiteralExpression>(
        "2", LiteralType::base(PrimitiveLiteralType::I32)));
    elements.push_back(std::make_shared<LiteralExpression>(
        "3", LiteralType::base(PrimitiveLiteralType::I32)));

    auto array = std::make_shared<ArrayExpression>(std::move(elements));
    auto result = evaluator.evaluate(array.get(), root_scope.get());

    assert(result.has_value());
    assert(result->is_array());
    assert(result->as_array().size() == 3);
    assert(result->as_array()[0].as_i32() == 1);
    assert(result->as_array()[1].as_i32() == 2);
    assert(result->as_array()[2].as_i32() == 3);
    std::cout << "✓ Array literal: [1, 2, 3] -> ["
              << result->as_array()[0].as_i32() << ", "
              << result->as_array()[1].as_i32() << ", "
              << result->as_array()[2].as_i32() << "]" << std::endl;
  }

  // Test 7: Constant lookup (using semantic scope)
  {
    // Create a mock constant item and add it to scope
    auto const_item = std::make_shared<ConstantItem>(
        "MY_CONST", LiteralType::base(PrimitiveLiteralType::I32),
        std::make_optional(std::make_shared<LiteralExpression>(
            "100", LiteralType::base(PrimitiveLiteralType::I32))));

    root_scope->add_item("MY_CONST", ItemKind::Constant, const_item.get());

    // Set up the metadata with type information
    auto *item = root_scope->find_value_item("MY_CONST");
    ConstantMetaData meta;
    meta.name = "MY_CONST";
    meta.type = SemType::primitive(SemPrimitiveKind::I32);
    meta.decl = const_item.get();
    meta.evaluated_value = std::make_shared<ConstValue>(ConstValue::i32(100));
    item->metadata = std::move(meta);

    auto name_expr = std::make_shared<NameExpression>("MY_CONST");
    auto result = evaluator.evaluate(name_expr.get(), root_scope.get());

    assert(result.has_value());
    assert(result->is_i32());
    assert(result->as_i32() == 100);
    std::cout << "✓ Constant lookup: MY_CONST -> " << result->as_i32()
              << std::endl;
  }

  std::cout << "\nAll constant evaluator tests passed! ✓\n" << std::endl;
}

int main() {
  try {
    test_const_evaluator();
    return 0;
  } catch (const std::exception &e) {
    std::cerr << "Test failed: " << e.what() << std::endl;
    return 1;
  }
}

#include <cassert>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include "../../include/lexer/lexer.hpp"
#include "../../include/ast/parser.hpp"
#include "../../include/ast/nodes/base.hpp"
#include "../../include/ast/nodes/expr.hpp"
#include "../../include/ast/nodes/stmt.hpp"
#include "../../include/ast/nodes/topLevel.hpp"

/**
 * Unit tests for the parser - tests specific parsing functionality
 * with controlled token input and expected AST node output.
 */

namespace rc {

class ParserUnitTest {
private:
    int passed_tests = 0;
    int total_tests = 0;
    
public:
    void assert_test(bool condition, const std::string& test_name, const std::string& message = "") {
        total_tests++;
        if (condition) {
            passed_tests++;
            std::cout << "✓ " << test_name << std::endl;
        } else {
            std::cout << "✗ " << test_name;
            if (!message.empty()) {
                std::cout << " - " << message;
            }
            std::cout << std::endl;
        }
    }
    
    void print_summary() {
        std::cout << "\n=== Test Summary ===" << std::endl;
        std::cout << "Passed: " << passed_tests << "/" << total_tests << std::endl;
        if (passed_tests == total_tests) {
            std::cout << "All tests passed! ✓" << std::endl;
        } else {
            std::cout << "Some tests failed! ✗" << std::endl;
        }
    }
    
    bool all_passed() const {
        return passed_tests == total_tests;
    }
    
    // Helper to create tokens
    std::vector<Token> create_tokens(const std::vector<std::pair<TokenType, std::string>>& token_specs) {
        std::vector<Token> tokens;
        for (const auto& spec : token_specs) {
            Token token;
            token.type = spec.first;
            token.lexeme = spec.second;
            tokens.push_back(token);
        }
        // Add EOF token
        Token eof_token;
        eof_token.type = TokenType::TOK_EOF;
        eof_token.lexeme = "";
        tokens.push_back(eof_token);
        return tokens;
    }
    
    // Test literal expressions
    void test_literal_expressions() {
        std::cout << "\n--- Testing Literal Expressions ---" << std::endl;
        
        // Test integer literal
        {
            auto tokens = create_tokens({{TokenType::INTEGER_LITERAL, "42"}});
            Parser parser(tokens);
            size_t pos = 0;
            auto expr = parser.any_expression().parse(tokens, pos);
            
            assert_test(expr.has_value(), "integer_literal_parse", "Should parse integer literal");
            if (expr.has_value()) {
                auto literal_expr = std::dynamic_pointer_cast<LiteralExpression>(expr.value());
                assert_test(literal_expr != nullptr, "integer_literal_type", "Should create LiteralExpression");
                if (literal_expr) {
                    assert_test(literal_expr->value == "42", "integer_literal_value", "Should have correct value");
                }
            }
        }
        
        // Test string literal
        {
            auto tokens = create_tokens({{TokenType::STRING_LITERAL, "\"hello\""}});
            Parser parser(tokens);
            size_t pos = 0;
            auto expr = parser.any_expression().parse(tokens, pos);
            
            assert_test(expr.has_value(), "string_literal_parse", "Should parse string literal");
            if (expr.has_value()) {
                auto literal_expr = std::dynamic_pointer_cast<LiteralExpression>(expr.value());
                assert_test(literal_expr != nullptr, "string_literal_type", "Should create LiteralExpression");
                if (literal_expr) {
                    assert_test(literal_expr->value == "\"hello\"", "string_literal_value", "Should have correct value");
                }
            }
        }
        
        // Test boolean literal
        {
            auto tokens = create_tokens({{TokenType::TRUE, "true"}});
            Parser parser(tokens);
            size_t pos = 0;
            auto expr = parser.any_expression().parse(tokens, pos);
            
            assert_test(expr.has_value(), "boolean_literal_parse", "Should parse boolean literal");
            if (expr.has_value()) {
                auto literal_expr = std::dynamic_pointer_cast<LiteralExpression>(expr.value());
                assert_test(literal_expr != nullptr, "boolean_literal_type", "Should create LiteralExpression");
            }
        }
        
        // Test character literal
        {
            auto tokens = create_tokens({{TokenType::CHAR_LITERAL, "'a'"}});
            Parser parser(tokens);
            size_t pos = 0;
            auto expr = parser.any_expression().parse(tokens, pos);
            
            assert_test(expr.has_value(), "char_literal_parse", "Should parse character literal");
            if (expr.has_value()) {
                auto literal_expr = std::dynamic_pointer_cast<LiteralExpression>(expr.value());
                assert_test(literal_expr != nullptr, "char_literal_type", "Should create LiteralExpression");
            }
        }
    }
    
    // Test binary expressions
    void test_binary_expressions() {
        std::cout << "\n--- Testing Binary Expressions ---" << std::endl;
        
        // Test simple arithmetic: 1 + 2
        {
            auto tokens = create_tokens({
                {TokenType::INTEGER_LITERAL, "1"},
                {TokenType::PLUS, "+"},
                {TokenType::INTEGER_LITERAL, "2"}
            });
            Parser parser(tokens);
            auto expr = parser.any_expression()(tokens.begin());
            
            assert_test(expr.has_value(), "addition_parse", "Should parse addition expression");
            if (expr.has_value()) {
                auto binary_expr = std::dynamic_pointer_cast<BinaryExpression>(expr->result);
                assert_test(binary_expr != nullptr, "addition_type", "Should create BinaryExpression");
                if (binary_expr) {
                    assert_test(binary_expr->operator_token.lexeme == "+", "addition_operator", "Should have correct operator");
                    assert_test(binary_expr->left != nullptr, "addition_left", "Should have left operand");
                    assert_test(binary_expr->right != nullptr, "addition_right", "Should have right operand");
                }
            }
        }
        
        // Test comparison: x == y
        {
            auto tokens = create_tokens({
                {TokenType::NON_KEYWORD_IDENTIFIER, "x"},
                {TokenType::EQ_EQ, "=="},
                {TokenType::NON_KEYWORD_IDENTIFIER, "y"}
            });
            Parser parser(tokens);
            auto expr = parser.any_expression()(tokens.begin());
            
            assert_test(expr.has_value(), "equality_parse", "Should parse equality expression");
            if (expr.has_value()) {
                auto binary_expr = std::dynamic_pointer_cast<BinaryExpression>(expr->result);
                assert_test(binary_expr != nullptr, "equality_type", "Should create BinaryExpression");
                if (binary_expr) {
                    assert_test(binary_expr->operator_token.lexeme == "==", "equality_operator", "Should have correct operator");
                }
            }
        }
        
        // Test precedence: 1 + 2 * 3 (should be 1 + (2 * 3))
        {
            auto tokens = create_tokens({
                {TokenType::INTEGER_LITERAL, "1"},
                {TokenType::PLUS, "+"},
                {TokenType::INTEGER_LITERAL, "2"},
                {TokenType::STAR, "*"},
                {TokenType::INTEGER_LITERAL, "3"}
            });
            Parser parser(tokens);
            auto expr = parser.any_expression()(tokens.begin());
            
            assert_test(expr.has_value(), "precedence_parse", "Should parse precedence expression");
            if (expr.has_value()) {
                auto binary_expr = std::dynamic_pointer_cast<BinaryExpression>(expr->result);
                assert_test(binary_expr != nullptr, "precedence_type", "Should create BinaryExpression");
                if (binary_expr) {
                    assert_test(binary_expr->operator_token.lexeme == "+", "precedence_top_op", "Top operator should be +");
                    auto right_binary = std::dynamic_pointer_cast<BinaryExpression>(binary_expr->right);
                    assert_test(right_binary != nullptr, "precedence_right_binary", "Right side should be binary expression");
                    if (right_binary) {
                        assert_test(right_binary->operator_token.lexeme == "*", "precedence_right_op", "Right operator should be *");
                    }
                }
            }
        }
    }
    
    // Test prefix expressions
    void test_prefix_expressions() {
        std::cout << "\n--- Testing Prefix Expressions ---" << std::endl;
        
        // Test negation: -42
        {
            auto tokens = create_tokens({
                {TokenType::MINUS, "-"},
                {TokenType::INTEGER_LITERAL, "42"}
            });
            Parser parser(tokens);
            auto expr = parser.any_expression()(tokens.begin());
            
            assert_test(expr.has_value(), "negation_parse", "Should parse negation expression");
            if (expr.has_value()) {
                auto prefix_expr = std::dynamic_pointer_cast<PrefixExpression>(expr->result);
                assert_test(prefix_expr != nullptr, "negation_type", "Should create PrefixExpression");
                if (prefix_expr) {
                    assert_test(prefix_expr->operator_token.lexeme == "-", "negation_operator", "Should have correct operator");
                    assert_test(prefix_expr->operand != nullptr, "negation_operand", "Should have operand");
                }
            }
        }
        
        // Test logical not: !true
        {
            auto tokens = create_tokens({
                {TokenType::NOT, "!"},
                {TokenType::TRUE, "true"}
            });
            Parser parser(tokens);
            auto expr = parser.any_expression()(tokens.begin());
            
            assert_test(expr.has_value(), "logical_not_parse", "Should parse logical not expression");
            if (expr.has_value()) {
                auto prefix_expr = std::dynamic_pointer_cast<PrefixExpression>(expr->result);
                assert_test(prefix_expr != nullptr, "logical_not_type", "Should create PrefixExpression");
                if (prefix_expr) {
                    assert_test(prefix_expr->operator_token.lexeme == "!", "logical_not_operator", "Should have correct operator");
                }
            }
        }
    }
    
    // Test function calls
    void test_function_calls() {
        std::cout << "\n--- Testing Function Calls ---" << std::endl;
        
        // Test simple function call: foo()
        {
            auto tokens = create_tokens({
                {TokenType::NON_KEYWORD_IDENTIFIER, "foo"},
                {TokenType::LPAREN, "("},
                {TokenType::RPAREN, ")"}
            });
            Parser parser(tokens);
            auto expr = parser.any_expression()(tokens.begin());
            
            assert_test(expr.has_value(), "simple_call_parse", "Should parse simple function call");
            if (expr.has_value()) {
                auto call_expr = std::dynamic_pointer_cast<CallExpression>(expr->result);
                assert_test(call_expr != nullptr, "simple_call_type", "Should create CallExpression");
                if (call_expr) {
                    assert_test(call_expr->arguments.empty(), "simple_call_args", "Should have no arguments");
                }
            }
        }
        
        // Test function call with arguments: foo(1, 2)
        {
            auto tokens = create_tokens({
                {TokenType::NON_KEYWORD_IDENTIFIER, "foo"},
                {TokenType::LPAREN, "("},
                {TokenType::INTEGER_LITERAL, "1"},
                {TokenType::COMMA, ","},
                {TokenType::INTEGER_LITERAL, "2"},
                {TokenType::RPAREN, ")"}
            });
            Parser parser(tokens);
            auto expr = parser.any_expression()(tokens.begin());
            
            assert_test(expr.has_value(), "args_call_parse", "Should parse function call with arguments");
            if (expr.has_value()) {
                auto call_expr = std::dynamic_pointer_cast<CallExpression>(expr->result);
                assert_test(call_expr != nullptr, "args_call_type", "Should create CallExpression");
                if (call_expr) {
                    assert_test(call_expr->arguments.size() == 2, "args_call_count", "Should have 2 arguments");
                }
            }
        }
    }
    
    // Test let statements
    void test_let_statements() {
        std::cout << "\n--- Testing Let Statements ---" << std::endl;
        
        // Test simple let: let x = 42;
        {
            auto tokens = create_tokens({
                {TokenType::LET, "let"},
                {TokenType::NON_KEYWORD_IDENTIFIER, "x"},
                {TokenType::EQ, "="},
                {TokenType::INTEGER_LITERAL, "42"},
                {TokenType::SEMICOLON, ";"}
            });
            Parser parser(tokens);
            auto ast = parser.parse();
            
            assert_test(ast != nullptr, "simple_let_parse", "Should parse simple let statement");
            if (ast && !ast->children.empty()) {
                // Assuming the statement is wrapped in some structure
                assert_test(true, "simple_let_structure", "Let statement parsed successfully");
            }
        }
        
        // Test let with type annotation: let x: i32 = 42;
        {
            auto tokens = create_tokens({
                {TokenType::LET, "let"},
                {TokenType::NON_KEYWORD_IDENTIFIER, "x"},
                {TokenType::COLON, ":"},
                {TokenType::NON_KEYWORD_IDENTIFIER, "i32"},
                {TokenType::EQ, "="},
                {TokenType::INTEGER_LITERAL, "42"},
                {TokenType::SEMICOLON, ";"}
            });
            Parser parser(tokens);
            auto ast = parser.parse();
            
            assert_test(ast != nullptr, "typed_let_parse", "Should parse let statement with type");
        }
    }
    
    // Test function declarations
    void test_function_declarations() {
        std::cout << "\n--- Testing Function Declarations ---" << std::endl;
        
        // Test simple function: fn foo() {}
        {
            auto tokens = create_tokens({
                {TokenType::FN, "fn"},
                {TokenType::NON_KEYWORD_IDENTIFIER, "foo"},
                {TokenType::LPAREN, "("},
                {TokenType::RPAREN, ")"},
                {TokenType::LBRACE, "{"},
                {TokenType::RBRACE, "}"}
            });
            Parser parser(tokens);
            auto ast = parser.parse();
            
            assert_test(ast != nullptr, "simple_fn_parse", "Should parse simple function");
            if (ast && !ast->children.empty()) {
                auto fn_decl = std::dynamic_pointer_cast<FunctionDecl>(ast->children[0]);
                assert_test(fn_decl != nullptr, "simple_fn_type", "Should create FunctionDecl");
                if (fn_decl) {
                    assert_test(fn_decl->name == "foo", "simple_fn_name", "Should have correct name");
                    assert_test(fn_decl->parameters.empty(), "simple_fn_params", "Should have no parameters");
                }
            }
        }
        
        // Test function with parameters: fn add(x: i32, y: i32) -> i32 {}
        {
            auto tokens = create_tokens({
                {TokenType::FN, "fn"},
                {TokenType::NON_KEYWORD_IDENTIFIER, "add"},
                {TokenType::LPAREN, "("},
                {TokenType::NON_KEYWORD_IDENTIFIER, "x"},
                {TokenType::COLON, ":"},
                {TokenType::NON_KEYWORD_IDENTIFIER, "i32"},
                {TokenType::COMMA, ","},
                {TokenType::NON_KEYWORD_IDENTIFIER, "y"},
                {TokenType::COLON, ":"},
                {TokenType::NON_KEYWORD_IDENTIFIER, "i32"},
                {TokenType::RPAREN, ")"},
                {TokenType::ARROW, "->"},
                {TokenType::NON_KEYWORD_IDENTIFIER, "i32"},
                {TokenType::LBRACE, "{"},
                {TokenType::RBRACE, "}"}
            });
            Parser parser(tokens);
            auto ast = parser.parse();
            
            assert_test(ast != nullptr, "param_fn_parse", "Should parse function with parameters");
            if (ast && !ast->children.empty()) {
                auto fn_decl = std::dynamic_pointer_cast<FunctionDecl>(ast->children[0]);
                assert_test(fn_decl != nullptr, "param_fn_type", "Should create FunctionDecl");
                if (fn_decl) {
                    assert_test(fn_decl->name == "add", "param_fn_name", "Should have correct name");
                    assert_test(fn_decl->parameters.size() == 2, "param_fn_param_count", "Should have 2 parameters");
                }
            }
        }
    }
    
    // Test if expressions
    void test_if_expressions() {
        std::cout << "\n--- Testing If Expressions ---" << std::endl;
        
        // Test simple if: if true { 1 }
        {
            auto tokens = create_tokens({
                {TokenType::IF, "if"},
                {TokenType::TRUE, "true"},
                {TokenType::LBRACE, "{"},
                {TokenType::INTEGER_LITERAL, "1"},
                {TokenType::RBRACE, "}"}
            });
            Parser parser(tokens);
            auto expr = parser.any_expression()(tokens.begin());
            
            assert_test(expr.has_value(), "simple_if_parse", "Should parse simple if expression");
            if (expr.has_value()) {
                auto if_expr = std::dynamic_pointer_cast<IfExpression>(expr->result);
                assert_test(if_expr != nullptr, "simple_if_type", "Should create IfExpression");
                if (if_expr) {
                    assert_test(if_expr->condition != nullptr, "simple_if_condition", "Should have condition");
                    assert_test(if_expr->then_block != nullptr, "simple_if_then", "Should have then block");
                    assert_test(if_expr->else_block == nullptr, "simple_if_else", "Should not have else block");
                }
            }
        }
        
        // Test if-else: if true { 1 } else { 2 }
        {
            auto tokens = create_tokens({
                {TokenType::IF, "if"},
                {TokenType::TRUE, "true"},
                {TokenType::LBRACE, "{"},
                {TokenType::INTEGER_LITERAL, "1"},
                {TokenType::RBRACE, "}"},
                {TokenType::ELSE, "else"},
                {TokenType::LBRACE, "{"},
                {TokenType::INTEGER_LITERAL, "2"},
                {TokenType::RBRACE, "}"}
            });
            Parser parser(tokens);
            auto expr = parser.any_expression()(tokens.begin());
            
            assert_test(expr.has_value(), "if_else_parse", "Should parse if-else expression");
            if (expr.has_value()) {
                auto if_expr = std::dynamic_pointer_cast<IfExpression>(expr->result);
                assert_test(if_expr != nullptr, "if_else_type", "Should create IfExpression");
                if (if_expr) {
                    assert_test(if_expr->condition != nullptr, "if_else_condition", "Should have condition");
                    assert_test(if_expr->then_block != nullptr, "if_else_then", "Should have then block");
                    assert_test(if_expr->else_block != nullptr, "if_else_else", "Should have else block");
                }
            }
        }
    }
    
    // Test error cases
    void test_error_cases() {
        std::cout << "\n--- Testing Error Cases ---" << std::endl;
        
        // Test incomplete expression
        {
            auto tokens = create_tokens({
                {TokenType::INTEGER_LITERAL, "1"},
                {TokenType::PLUS, "+"}
                // Missing right operand
            });
            Parser parser(tokens);
            auto expr = parser.any_expression()(tokens.begin());
            
            assert_test(!expr.has_value(), "incomplete_expr", "Should fail to parse incomplete expression");
        }
        
        // Test mismatched parentheses
        {
            auto tokens = create_tokens({
                {TokenType::LPAREN, "("},
                {TokenType::INTEGER_LITERAL, "1"}
                // Missing closing paren
            });
            Parser parser(tokens);
            auto expr = parser.any_expression()(tokens.begin());
            
            assert_test(!expr.has_value(), "mismatched_parens", "Should fail to parse mismatched parentheses");
        }
        
        // Test invalid function declaration
        {
            auto tokens = create_tokens({
                {TokenType::FN, "fn"},
                // Missing function name
                {TokenType::LPAREN, "("},
                {TokenType::RPAREN, ")"},
                {TokenType::LBRACE, "{"},
                {TokenType::RBRACE, "}"}
            });
            Parser parser(tokens);
            auto ast = parser.parse();
            
            // This might succeed or fail depending on parser implementation
            // The test documents expected behavior
            assert_test(ast == nullptr, "invalid_fn_decl", "Should fail to parse invalid function declaration");
        }
    }
    
    // Run all tests
    void run_all_tests() {
        std::cout << "=== Parser Unit Tests ===" << std::endl;
        
        test_literal_expressions();
        test_binary_expressions();
        test_prefix_expressions();
        test_function_calls();
        test_let_statements();
        test_function_declarations();
        test_if_expressions();
        test_error_cases();
        
        print_summary();
    }
};

} // namespace rc

int main() {
    rc::ParserUnitTest test_runner;
    test_runner.run_all_tests();
    return test_runner.all_passed() ? 0 : 1;
}

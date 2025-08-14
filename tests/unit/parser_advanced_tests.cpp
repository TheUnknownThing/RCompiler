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
 * Advanced parser unit tests - testing complex parsing scenarios
 * and edge cases with controlled token input.
 */

namespace rc {

class AdvancedParserTest {
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
        std::cout << "\n=== Advanced Test Summary ===" << std::endl;
        std::cout << "Passed: " << passed_tests << "/" << total_tests << std::endl;
        if (passed_tests == total_tests) {
            std::cout << "All advanced tests passed! ✓" << std::endl;
        } else {
            std::cout << "Some advanced tests failed! ✗" << std::endl;
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
            token.line = 1;
            token.column = 1;
            tokens.push_back(token);
        }
        // Add EOF token
        Token eof_token;
        eof_token.type = TokenType::END_OF_FILE;
        eof_token.lexeme = "";
        eof_token.line = 1;
        eof_token.column = 1;
        tokens.push_back(eof_token);
        return tokens;
    }
    
    // Test complex expressions with multiple operators
    void test_complex_expressions() {
        std::cout << "\n--- Testing Complex Expressions ---" << std::endl;
        
        // Test complex arithmetic: (1 + 2) * 3 - 4 / 2
        {
            auto tokens = create_tokens({
                {TokenType::LPAREN, "("},
                {TokenType::INTEGER_LITERAL, "1"},
                {TokenType::PLUS, "+"},
                {TokenType::INTEGER_LITERAL, "2"},
                {TokenType::RPAREN, ")"},
                {TokenType::STAR, "*"},
                {TokenType::INTEGER_LITERAL, "3"},
                {TokenType::MINUS, "-"},
                {TokenType::INTEGER_LITERAL, "4"},
                {TokenType::SLASH, "/"},
                {TokenType::INTEGER_LITERAL, "2"}
            });
            Parser parser(tokens);
            auto expr = parser.any_expression()(tokens.begin());
            
            assert_test(expr.has_value(), "complex_arithmetic_parse", "Should parse complex arithmetic expression");
            if (expr.has_value()) {
                auto binary_expr = std::dynamic_pointer_cast<BinaryExpression>(expr->result);
                assert_test(binary_expr != nullptr, "complex_arithmetic_type", "Should create BinaryExpression");
                if (binary_expr) {
                    assert_test(binary_expr->operator_token.lexeme == "-", "complex_arithmetic_root_op", "Root operator should be -");
                }
            }
        }
        
        // Test chained comparisons: x < y && y < z
        {
            auto tokens = create_tokens({
                {TokenType::NON_KEYWORD_IDENTIFIER, "x"},
                {TokenType::LT, "<"},
                {TokenType::NON_KEYWORD_IDENTIFIER, "y"},
                {TokenType::AND_AND, "&&"},
                {TokenType::NON_KEYWORD_IDENTIFIER, "y"},
                {TokenType::LT, "<"},
                {TokenType::NON_KEYWORD_IDENTIFIER, "z"}
            });
            Parser parser(tokens);
            auto expr = parser.any_expression()(tokens.begin());
            
            assert_test(expr.has_value(), "chained_comparison_parse", "Should parse chained comparisons");
            if (expr.has_value()) {
                auto binary_expr = std::dynamic_pointer_cast<BinaryExpression>(expr->result);
                assert_test(binary_expr != nullptr, "chained_comparison_type", "Should create BinaryExpression");
                if (binary_expr) {
                    assert_test(binary_expr->operator_token.lexeme == "&&", "chained_comparison_op", "Top operator should be &&");
                }
            }
        }
        
        // Test nested function calls: foo(bar(1), baz(2, 3))
        {
            auto tokens = create_tokens({
                {TokenType::NON_KEYWORD_IDENTIFIER, "foo"},
                {TokenType::LPAREN, "("},
                {TokenType::NON_KEYWORD_IDENTIFIER, "bar"},
                {TokenType::LPAREN, "("},
                {TokenType::INTEGER_LITERAL, "1"},
                {TokenType::RPAREN, ")"},
                {TokenType::COMMA, ","},
                {TokenType::NON_KEYWORD_IDENTIFIER, "baz"},
                {TokenType::LPAREN, "("},
                {TokenType::INTEGER_LITERAL, "2"},
                {TokenType::COMMA, ","},
                {TokenType::INTEGER_LITERAL, "3"},
                {TokenType::RPAREN, ")"},
                {TokenType::RPAREN, ")"}
            });
            Parser parser(tokens);
            auto expr = parser.any_expression()(tokens.begin());
            
            assert_test(expr.has_value(), "nested_calls_parse", "Should parse nested function calls");
            if (expr.has_value()) {
                auto call_expr = std::dynamic_pointer_cast<CallExpression>(expr->result);
                assert_test(call_expr != nullptr, "nested_calls_type", "Should create CallExpression");
                if (call_expr) {
                    assert_test(call_expr->arguments.size() == 2, "nested_calls_arg_count", "Should have 2 arguments");
                    
                    // Check that first argument is a function call
                    auto first_arg_call = std::dynamic_pointer_cast<CallExpression>(call_expr->arguments[0]);
                    assert_test(first_arg_call != nullptr, "nested_calls_first_arg", "First argument should be a call");
                    
                    // Check that second argument is a function call
                    auto second_arg_call = std::dynamic_pointer_cast<CallExpression>(call_expr->arguments[1]);
                    assert_test(second_arg_call != nullptr, "nested_calls_second_arg", "Second argument should be a call");
                    if (second_arg_call) {
                        assert_test(second_arg_call->arguments.size() == 2, "nested_calls_second_arg_count", "Second call should have 2 arguments");
                    }
                }
            }
        }
    }
    
    // Test block expressions
    void test_block_expressions() {
        std::cout << "\n--- Testing Block Expressions ---" << std::endl;
        
        // Test simple block: { 42 }
        {
            auto tokens = create_tokens({
                {TokenType::LBRACE, "{"},
                {TokenType::INTEGER_LITERAL, "42"},
                {TokenType::RBRACE, "}"}
            });
            Parser parser(tokens);
            auto expr = parser.parse_block_expression()(tokens.begin());
            
            assert_test(expr.has_value(), "simple_block_parse", "Should parse simple block");
            if (expr.has_value()) {
                auto block_expr = std::dynamic_pointer_cast<BlockExpression>(expr->result);
                assert_test(block_expr != nullptr, "simple_block_type", "Should create BlockExpression");
                if (block_expr) {
                    assert_test(!block_expr->statements.empty(), "simple_block_statements", "Block should have statements");
                }
            }
        }
        
        // Test block with multiple statements: { let x = 1; x + 2 }
        {
            auto tokens = create_tokens({
                {TokenType::LBRACE, "{"},
                {TokenType::LET, "let"},
                {TokenType::NON_KEYWORD_IDENTIFIER, "x"},
                {TokenType::EQ, "="},
                {TokenType::INTEGER_LITERAL, "1"},
                {TokenType::SEMICOLON, ";"},
                {TokenType::NON_KEYWORD_IDENTIFIER, "x"},
                {TokenType::PLUS, "+"},
                {TokenType::INTEGER_LITERAL, "2"},
                {TokenType::RBRACE, "}"}
            });
            Parser parser(tokens);
            auto expr = parser.parse_block_expression()(tokens.begin());
            
            assert_test(expr.has_value(), "multi_stmt_block_parse", "Should parse block with multiple statements");
            if (expr.has_value()) {
                auto block_expr = std::dynamic_pointer_cast<BlockExpression>(expr->result);
                assert_test(block_expr != nullptr, "multi_stmt_block_type", "Should create BlockExpression");
                if (block_expr) {
                    assert_test(block_expr->statements.size() >= 1, "multi_stmt_block_count", "Block should have statements");
                    assert_test(block_expr->expression != nullptr, "multi_stmt_block_expr", "Block should have final expression");
                }
            }
        }
        
        // Test empty block: {}
        {
            auto tokens = create_tokens({
                {TokenType::LBRACE, "{"},
                {TokenType::RBRACE, "}"}
            });
            Parser parser(tokens);
            auto expr = parser.parse_block_expression()(tokens.begin());
            
            assert_test(expr.has_value(), "empty_block_parse", "Should parse empty block");
            if (expr.has_value()) {
                auto block_expr = std::dynamic_pointer_cast<BlockExpression>(expr->result);
                assert_test(block_expr != nullptr, "empty_block_type", "Should create BlockExpression");
                if (block_expr) {
                    assert_test(block_expr->statements.empty(), "empty_block_statements", "Empty block should have no statements");
                    assert_test(block_expr->expression == nullptr, "empty_block_expr", "Empty block should have no final expression");
                }
            }
        }
    }
    
    // Test match expressions
    void test_match_expressions() {
        std::cout << "\n--- Testing Match Expressions ---" << std::endl;
        
        // Test simple match: match x { 1 => 2, _ => 3 }
        {
            auto tokens = create_tokens({
                {TokenType::MATCH, "match"},
                {TokenType::NON_KEYWORD_IDENTIFIER, "x"},
                {TokenType::LBRACE, "{"},
                {TokenType::INTEGER_LITERAL, "1"},
                {TokenType::FAT_ARROW, "=>"},
                {TokenType::INTEGER_LITERAL, "2"},
                {TokenType::COMMA, ","},
                {TokenType::UNDERSCORE, "_"},
                {TokenType::FAT_ARROW, "=>"},
                {TokenType::INTEGER_LITERAL, "3"},
                {TokenType::RBRACE, "}"}
            });
            Parser parser(tokens);
            auto expr = parser.parse_match_expression()(tokens.begin());
            
            assert_test(expr.has_value(), "simple_match_parse", "Should parse simple match expression");
            if (expr.has_value()) {
                auto match_expr = std::dynamic_pointer_cast<MatchExpression>(expr->result);
                assert_test(match_expr != nullptr, "simple_match_type", "Should create MatchExpression");
                if (match_expr) {
                    assert_test(match_expr->expression != nullptr, "simple_match_expr", "Should have match expression");
                    assert_test(match_expr->arms.size() == 2, "simple_match_arms", "Should have 2 match arms");
                }
            }
        }
    }
    
    // Test loop expressions
    void test_loop_expressions() {
        std::cout << "\n--- Testing Loop Expressions ---" << std::endl;
        
        // Test infinite loop: loop { break; }
        {
            auto tokens = create_tokens({
                {TokenType::LOOP, "loop"},
                {TokenType::LBRACE, "{"},
                {TokenType::BREAK, "break"},
                {TokenType::SEMICOLON, ";"},
                {TokenType::RBRACE, "}"}
            });
            Parser parser(tokens);
            auto expr = parser.parse_loop_expression()(tokens.begin());
            
            assert_test(expr.has_value(), "loop_parse", "Should parse loop expression");
            if (expr.has_value()) {
                auto loop_expr = std::dynamic_pointer_cast<LoopExpression>(expr->result);
                assert_test(loop_expr != nullptr, "loop_type", "Should create LoopExpression");
                if (loop_expr) {
                    assert_test(loop_expr->body != nullptr, "loop_body", "Should have loop body");
                }
            }
        }
        
        // Test while loop: while true { continue; }
        {
            auto tokens = create_tokens({
                {TokenType::WHILE, "while"},
                {TokenType::TRUE, "true"},
                {TokenType::LBRACE, "{"},
                {TokenType::CONTINUE, "continue"},
                {TokenType::SEMICOLON, ";"},
                {TokenType::RBRACE, "}"}
            });
            Parser parser(tokens);
            auto expr = parser.parse_while_expression()(tokens.begin());
            
            assert_test(expr.has_value(), "while_parse", "Should parse while expression");
            if (expr.has_value()) {
                auto while_expr = std::dynamic_pointer_cast<WhileExpression>(expr->result);
                assert_test(while_expr != nullptr, "while_type", "Should create WhileExpression");
                if (while_expr) {
                    assert_test(while_expr->condition != nullptr, "while_condition", "Should have while condition");
                    assert_test(while_expr->body != nullptr, "while_body", "Should have while body");
                }
            }
        }
    }
    
    // Test array expressions
    void test_array_expressions() {
        std::cout << "\n--- Testing Array Expressions ---" << std::endl;
        
        // Test array literal: [1, 2, 3]
        {
            auto tokens = create_tokens({
                {TokenType::LBRACKET, "["},
                {TokenType::INTEGER_LITERAL, "1"},
                {TokenType::COMMA, ","},
                {TokenType::INTEGER_LITERAL, "2"},
                {TokenType::COMMA, ","},
                {TokenType::INTEGER_LITERAL, "3"},
                {TokenType::RBRACKET, "]"}
            });
            Parser parser(tokens);
            auto expr = parser.parse_array_expression()(tokens.begin());
            
            assert_test(expr.has_value(), "array_literal_parse", "Should parse array literal");
            if (expr.has_value()) {
                auto array_expr = std::dynamic_pointer_cast<ArrayExpression>(expr->result);
                assert_test(array_expr != nullptr, "array_literal_type", "Should create ArrayExpression");
                if (array_expr) {
                    assert_test(array_expr->elements.size() == 3, "array_literal_elements", "Should have 3 elements");
                }
            }
        }
        
        // Test empty array: []
        {
            auto tokens = create_tokens({
                {TokenType::LBRACKET, "["},
                {TokenType::RBRACKET, "]"}
            });
            Parser parser(tokens);
            auto expr = parser.parse_array_expression()(tokens.begin());
            
            assert_test(expr.has_value(), "empty_array_parse", "Should parse empty array");
            if (expr.has_value()) {
                auto array_expr = std::dynamic_pointer_cast<ArrayExpression>(expr->result);
                assert_test(array_expr != nullptr, "empty_array_type", "Should create ArrayExpression");
                if (array_expr) {
                    assert_test(array_expr->elements.empty(), "empty_array_elements", "Should have no elements");
                }
            }
        }
        
        // Test array indexing: arr[0]
        {
            auto tokens = create_tokens({
                {TokenType::NON_KEYWORD_IDENTIFIER, "arr"},
                {TokenType::LBRACKET, "["},
                {TokenType::INTEGER_LITERAL, "0"},
                {TokenType::RBRACKET, "]"}
            });
            Parser parser(tokens);
            auto expr = parser.any_expression()(tokens.begin());
            
            assert_test(expr.has_value(), "array_index_parse", "Should parse array indexing");
            if (expr.has_value()) {
                auto index_expr = std::dynamic_pointer_cast<IndexExpression>(expr->result);
                assert_test(index_expr != nullptr, "array_index_type", "Should create IndexExpression");
                if (index_expr) {
                    assert_test(index_expr->object != nullptr, "array_index_object", "Should have indexed object");
                    assert_test(index_expr->index != nullptr, "array_index_index", "Should have index expression");
                }
            }
        }
    }
    
    // Test tuple expressions
    void test_tuple_expressions() {
        std::cout << "\n--- Testing Tuple Expressions ---" << std::endl;
        
        // Test tuple: (1, 2, 3)
        {
            auto tokens = create_tokens({
                {TokenType::LPAREN, "("},
                {TokenType::INTEGER_LITERAL, "1"},
                {TokenType::COMMA, ","},
                {TokenType::INTEGER_LITERAL, "2"},
                {TokenType::COMMA, ","},
                {TokenType::INTEGER_LITERAL, "3"},
                {TokenType::RPAREN, ")"}
            });
            Parser parser(tokens);
            auto expr = parser.parse_tuple_or_group_expression()(tokens.begin());
            
            assert_test(expr.has_value(), "tuple_parse", "Should parse tuple expression");
            if (expr.has_value()) {
                auto tuple_expr = std::dynamic_pointer_cast<TupleExpression>(expr->result);
                assert_test(tuple_expr != nullptr, "tuple_type", "Should create TupleExpression");
                if (tuple_expr) {
                    assert_test(tuple_expr->elements.size() == 3, "tuple_elements", "Should have 3 elements");
                }
            }
        }
        
        // Test single element tuple: (42,)
        {
            auto tokens = create_tokens({
                {TokenType::LPAREN, "("},
                {TokenType::INTEGER_LITERAL, "42"},
                {TokenType::COMMA, ","},
                {TokenType::RPAREN, ")"}
            });
            Parser parser(tokens);
            auto expr = parser.parse_tuple_or_group_expression()(tokens.begin());
            
            assert_test(expr.has_value(), "single_tuple_parse", "Should parse single element tuple");
            if (expr.has_value()) {
                auto tuple_expr = std::dynamic_pointer_cast<TupleExpression>(expr->result);
                assert_test(tuple_expr != nullptr, "single_tuple_type", "Should create TupleExpression");
                if (tuple_expr) {
                    assert_test(tuple_expr->elements.size() == 1, "single_tuple_elements", "Should have 1 element");
                }
            }
        }
        
        // Test empty tuple (unit): ()
        {
            auto tokens = create_tokens({
                {TokenType::LPAREN, "("},
                {TokenType::RPAREN, ")"}
            });
            Parser parser(tokens);
            auto expr = parser.parse_tuple_or_group_expression()(tokens.begin());
            
            assert_test(expr.has_value(), "unit_tuple_parse", "Should parse unit tuple");
            if (expr.has_value()) {
                auto tuple_expr = std::dynamic_pointer_cast<TupleExpression>(expr->result);
                assert_test(tuple_expr != nullptr, "unit_tuple_type", "Should create TupleExpression");
                if (tuple_expr) {
                    assert_test(tuple_expr->elements.empty(), "unit_tuple_elements", "Should have no elements");
                }
            }
        }
    }
    
    // Test struct declarations
    void test_struct_declarations() {
        std::cout << "\n--- Testing Struct Declarations ---" << std::endl;
        
        // Test simple struct: struct Point { x: i32, y: i32 }
        {
            auto tokens = create_tokens({
                {TokenType::STRUCT, "struct"},
                {TokenType::NON_KEYWORD_IDENTIFIER, "Point"},
                {TokenType::LBRACE, "{"},
                {TokenType::NON_KEYWORD_IDENTIFIER, "x"},
                {TokenType::COLON, ":"},
                {TokenType::NON_KEYWORD_IDENTIFIER, "i32"},
                {TokenType::COMMA, ","},
                {TokenType::NON_KEYWORD_IDENTIFIER, "y"},
                {TokenType::COLON, ":"},
                {TokenType::NON_KEYWORD_IDENTIFIER, "i32"},
                {TokenType::RBRACE, "}"}
            });
            Parser parser(tokens);
            auto ast = parser.parse();
            
            assert_test(ast != nullptr, "struct_parse", "Should parse struct declaration");
            if (ast && !ast->children.empty()) {
                auto struct_decl = std::dynamic_pointer_cast<StructDecl>(ast->children[0]);
                assert_test(struct_decl != nullptr, "struct_type", "Should create StructDecl");
                if (struct_decl) {
                    assert_test(struct_decl->name == "Point", "struct_name", "Should have correct name");
                    assert_test(struct_decl->fields.size() == 2, "struct_fields", "Should have 2 fields");
                }
            }
        }
        
        // Test empty struct: struct Empty {}
        {
            auto tokens = create_tokens({
                {TokenType::STRUCT, "struct"},
                {TokenType::NON_KEYWORD_IDENTIFIER, "Empty"},
                {TokenType::LBRACE, "{"},
                {TokenType::RBRACE, "}"}
            });
            Parser parser(tokens);
            auto ast = parser.parse();
            
            assert_test(ast != nullptr, "empty_struct_parse", "Should parse empty struct");
            if (ast && !ast->children.empty()) {
                auto struct_decl = std::dynamic_pointer_cast<StructDecl>(ast->children[0]);
                assert_test(struct_decl != nullptr, "empty_struct_type", "Should create StructDecl");
                if (struct_decl) {
                    assert_test(struct_decl->name == "Empty", "empty_struct_name", "Should have correct name");
                    assert_test(struct_decl->fields.empty(), "empty_struct_fields", "Should have no fields");
                }
            }
        }
    }
    
    // Test precedence and associativity edge cases
    void test_precedence_edge_cases() {
        std::cout << "\n--- Testing Precedence Edge Cases ---" << std::endl;
        
        // Test right associativity: a = b = c (should be a = (b = c))
        {
            auto tokens = create_tokens({
                {TokenType::NON_KEYWORD_IDENTIFIER, "a"},
                {TokenType::EQ, "="},
                {TokenType::NON_KEYWORD_IDENTIFIER, "b"},
                {TokenType::EQ, "="},
                {TokenType::NON_KEYWORD_IDENTIFIER, "c"}
            });
            Parser parser(tokens);
            auto expr = parser.any_expression()(tokens.begin());
            
            assert_test(expr.has_value(), "right_assoc_parse", "Should parse right associative expression");
            if (expr.has_value()) {
                auto binary_expr = std::dynamic_pointer_cast<BinaryExpression>(expr->result);
                assert_test(binary_expr != nullptr, "right_assoc_type", "Should create BinaryExpression");
                if (binary_expr) {
                    assert_test(binary_expr->operator_token.lexeme == "=", "right_assoc_op", "Top operator should be =");
                    auto right_binary = std::dynamic_pointer_cast<BinaryExpression>(binary_expr->right);
                    assert_test(right_binary != nullptr, "right_assoc_right", "Right side should be binary expression");
                    if (right_binary) {
                        assert_test(right_binary->operator_token.lexeme == "=", "right_assoc_right_op", "Right operator should be =");
                    }
                }
            }
        }
        
        // Test mixed precedence: -a * b + c (should be ((-a) * b) + c)
        {
            auto tokens = create_tokens({
                {TokenType::MINUS, "-"},
                {TokenType::NON_KEYWORD_IDENTIFIER, "a"},
                {TokenType::STAR, "*"},
                {TokenType::NON_KEYWORD_IDENTIFIER, "b"},
                {TokenType::PLUS, "+"},
                {TokenType::NON_KEYWORD_IDENTIFIER, "c"}
            });
            Parser parser(tokens);
            auto expr = parser.any_expression()(tokens.begin());
            
            assert_test(expr.has_value(), "mixed_precedence_parse", "Should parse mixed precedence expression");
            if (expr.has_value()) {
                auto binary_expr = std::dynamic_pointer_cast<BinaryExpression>(expr->result);
                assert_test(binary_expr != nullptr, "mixed_precedence_type", "Should create BinaryExpression");
                if (binary_expr) {
                    assert_test(binary_expr->operator_token.lexeme == "+", "mixed_precedence_top_op", "Top operator should be +");
                    
                    auto left_binary = std::dynamic_pointer_cast<BinaryExpression>(binary_expr->left);
                    assert_test(left_binary != nullptr, "mixed_precedence_left", "Left side should be binary expression");
                    if (left_binary) {
                        assert_test(left_binary->operator_token.lexeme == "*", "mixed_precedence_left_op", "Left operator should be *");
                        
                        auto prefix_expr = std::dynamic_pointer_cast<PrefixExpression>(left_binary->left);
                        assert_test(prefix_expr != nullptr, "mixed_precedence_prefix", "Leftmost should be prefix expression");
                        if (prefix_expr) {
                            assert_test(prefix_expr->operator_token.lexeme == "-", "mixed_precedence_prefix_op", "Prefix operator should be -");
                        }
                    }
                }
            }
        }
    }
    
    // Run all advanced tests
    void run_all_tests() {
        std::cout << "=== Advanced Parser Unit Tests ===" << std::endl;
        
        test_complex_expressions();
        test_block_expressions();
        test_match_expressions();
        test_loop_expressions();
        test_array_expressions();
        test_tuple_expressions();
        test_struct_declarations();
        test_precedence_edge_cases();
        
        print_summary();
    }
};

} // namespace rc

int main() {
    rc::AdvancedParserTest test_runner;
    test_runner.run_all_tests();
    return test_runner.all_passed() ? 0 : 1;
}

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
 * Parser error handling unit tests.
 * Tests that the parser properly handles invalid input and error cases.
 */

namespace rc {

class ParserErrorTest {
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
        std::cout << "\n=== Error Test Summary ===" << std::endl;
        std::cout << "Passed: " << passed_tests << "/" << total_tests << std::endl;
        if (passed_tests == total_tests) {
            std::cout << "All error tests passed! ✓" << std::endl;
        } else {
            std::cout << "Some error tests failed! ✗" << std::endl;
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
    
    // Test syntax errors
    void test_syntax_errors() {
        std::cout << "\n--- Testing Syntax Errors ---" << std::endl;
        
        // Test incomplete binary expression: 1 +
        {
            auto tokens = create_tokens({
                {TokenType::INTEGER_LITERAL, "1"},
                {TokenType::PLUS, "+"}
            });
            Parser parser(tokens);
            auto expr = parser.any_expression()(tokens.begin());
            
            assert_test(!expr.has_value(), "incomplete_binary_expr", "Should reject incomplete binary expression");
        }
        
        // Test mismatched parentheses: (1 + 2
        {
            auto tokens = create_tokens({
                {TokenType::LPAREN, "("},
                {TokenType::INTEGER_LITERAL, "1"},
                {TokenType::PLUS, "+"},
                {TokenType::INTEGER_LITERAL, "2"}
            });
            Parser parser(tokens);
            auto expr = parser.any_expression()(tokens.begin());
            
            assert_test(!expr.has_value(), "mismatched_parens", "Should reject mismatched parentheses");
        }
        
        // Test incomplete function call: foo(1,
        {
            auto tokens = create_tokens({
                {TokenType::NON_KEYWORD_IDENTIFIER, "foo"},
                {TokenType::LPAREN, "("},
                {TokenType::INTEGER_LITERAL, "1"},
                {TokenType::COMMA, ","}
            });
            Parser parser(tokens);
            auto expr = parser.any_expression()(tokens.begin());
            
            assert_test(!expr.has_value(), "incomplete_function_call", "Should reject incomplete function call");
        }
        
        // Test invalid operator sequence: 1 + + 2
        {
            auto tokens = create_tokens({
                {TokenType::INTEGER_LITERAL, "1"},
                {TokenType::PLUS, "+"},
                {TokenType::PLUS, "+"},
                {TokenType::INTEGER_LITERAL, "2"}
            });
            Parser parser(tokens);
            auto expr = parser.any_expression()(tokens.begin());
            
            assert_test(!expr.has_value(), "invalid_operator_sequence", "Should reject invalid operator sequence");
        }
    }
    
    // Test expression errors
    void test_expression_errors() {
        std::cout << "\n--- Testing Expression Errors ---" << std::endl;
        
        // Test empty expression
        {
            auto tokens = create_tokens({});
            Parser parser(tokens);
            auto expr = parser.any_expression()(tokens.begin());
            
            assert_test(!expr.has_value(), "empty_expression", "Should reject empty expression");
        }
        
        // Test invalid prefix expression: + +
        {
            auto tokens = create_tokens({
                {TokenType::PLUS, "+"},
                {TokenType::PLUS, "+"}
            });
            Parser parser(tokens);
            auto expr = parser.any_expression()(tokens.begin());
            
            assert_test(!expr.has_value(), "invalid_prefix_expr", "Should reject invalid prefix expression");
        }
        
        // Test incomplete if expression: if
        {
            auto tokens = create_tokens({
                {TokenType::IF, "if"}
            });
            Parser parser(tokens);
            auto expr = parser.parse_if_expression()(tokens.begin());
            
            assert_test(!expr.has_value(), "incomplete_if_expr", "Should reject incomplete if expression");
        }
        
        // Test if without condition: if { }
        {
            auto tokens = create_tokens({
                {TokenType::IF, "if"},
                {TokenType::LBRACE, "{"},
                {TokenType::RBRACE, "}"}
            });
            Parser parser(tokens);
            auto expr = parser.parse_if_expression()(tokens.begin());
            
            assert_test(!expr.has_value(), "if_without_condition", "Should reject if without condition");
        }
        
        // Test incomplete block: {
        {
            auto tokens = create_tokens({
                {TokenType::LBRACE, "{"}
            });
            Parser parser(tokens);
            auto expr = parser.parse_block_expression()(tokens.begin());
            
            assert_test(!expr.has_value(), "incomplete_block", "Should reject incomplete block");
        }
    }
    
    // Test function declaration errors
    void test_function_errors() {
        std::cout << "\n--- Testing Function Declaration Errors ---" << std::endl;
        
        // Test function without name: fn () {}
        {
            auto tokens = create_tokens({
                {TokenType::FN, "fn"},
                {TokenType::LPAREN, "("},
                {TokenType::RPAREN, ")"},
                {TokenType::LBRACE, "{"},
                {TokenType::RBRACE, "}"}
            });
            Parser parser(tokens);
            auto ast = parser.parse();
            
            // This should fail as function needs a name
            assert_test(ast == nullptr, "function_without_name", "Should reject function without name");
        }
        
        // Test function with incomplete parameters: fn foo(x:
        {
            auto tokens = create_tokens({
                {TokenType::FN, "fn"},
                {TokenType::NON_KEYWORD_IDENTIFIER, "foo"},
                {TokenType::LPAREN, "("},
                {TokenType::NON_KEYWORD_IDENTIFIER, "x"},
                {TokenType::COLON, ":"}
            });
            Parser parser(tokens);
            auto ast = parser.parse();
            
            assert_test(ast == nullptr, "incomplete_function_param", "Should reject incomplete function parameter");
        }
        
        // Test function without body: fn foo()
        {
            auto tokens = create_tokens({
                {TokenType::FN, "fn"},
                {TokenType::NON_KEYWORD_IDENTIFIER, "foo"},
                {TokenType::LPAREN, "("},
                {TokenType::RPAREN, ")"}
            });
            Parser parser(tokens);
            auto ast = parser.parse();
            
            assert_test(ast == nullptr, "function_without_body", "Should reject function without body");
        }
        
        // Test function with mismatched braces: fn foo() { 
        {
            auto tokens = create_tokens({
                {TokenType::FN, "fn"},
                {TokenType::NON_KEYWORD_IDENTIFIER, "foo"},
                {TokenType::LPAREN, "("},
                {TokenType::RPAREN, ")"},
                {TokenType::LBRACE, "{"}
            });
            Parser parser(tokens);
            auto ast = parser.parse();
            
            assert_test(ast == nullptr, "function_mismatched_braces", "Should reject function with mismatched braces");
        }
    }
    
    // Test let statement errors
    void test_let_statement_errors() {
        std::cout << "\n--- Testing Let Statement Errors ---" << std::endl;
        
        // Test let without pattern: let = 42;
        {
            auto tokens = create_tokens({
                {TokenType::LET, "let"},
                {TokenType::EQ, "="},
                {TokenType::INTEGER_LITERAL, "42"},
                {TokenType::SEMICOLON, ";"}
            });
            Parser parser(tokens);
            auto ast = parser.parse();
            
            assert_test(ast == nullptr, "let_without_pattern", "Should reject let without pattern");
        }
        
        // Test let without semicolon: let x = 42
        {
            auto tokens = create_tokens({
                {TokenType::LET, "let"},
                {TokenType::NON_KEYWORD_IDENTIFIER, "x"},
                {TokenType::EQ, "="},
                {TokenType::INTEGER_LITERAL, "42"}
            });
            Parser parser(tokens);
            auto ast = parser.parse();
            
            assert_test(ast == nullptr, "let_without_semicolon", "Should reject let without semicolon");
        }
        
        // Test let with incomplete type annotation: let x:
        {
            auto tokens = create_tokens({
                {TokenType::LET, "let"},
                {TokenType::NON_KEYWORD_IDENTIFIER, "x"},
                {TokenType::COLON, ":"}
            });
            Parser parser(tokens);
            auto ast = parser.parse();
            
            assert_test(ast == nullptr, "let_incomplete_type", "Should reject let with incomplete type annotation");
        }
    }
    
    // Test match expression errors
    void test_match_errors() {
        std::cout << "\n--- Testing Match Expression Errors ---" << std::endl;
        
        // Test match without expression: match { }
        {
            auto tokens = create_tokens({
                {TokenType::MATCH, "match"},
                {TokenType::LBRACE, "{"},
                {TokenType::RBRACE, "}"}
            });
            Parser parser(tokens);
            auto expr = parser.parse_match_expression()(tokens.begin());
            
            assert_test(!expr.has_value(), "match_without_expr", "Should reject match without expression");
        }
        
        // Test match without braces: match x
        {
            auto tokens = create_tokens({
                {TokenType::MATCH, "match"},
                {TokenType::NON_KEYWORD_IDENTIFIER, "x"}
            });
            Parser parser(tokens);
            auto expr = parser.parse_match_expression()(tokens.begin());
            
            assert_test(!expr.has_value(), "match_without_braces", "Should reject match without braces");
        }
        
        // Test match arm without arrow: match x { 1 2 }
        {
            auto tokens = create_tokens({
                {TokenType::MATCH, "match"},
                {TokenType::NON_KEYWORD_IDENTIFIER, "x"},
                {TokenType::LBRACE, "{"},
                {TokenType::INTEGER_LITERAL, "1"},
                {TokenType::INTEGER_LITERAL, "2"},
                {TokenType::RBRACE, "}"}
            });
            Parser parser(tokens);
            auto expr = parser.parse_match_expression()(tokens.begin());
            
            assert_test(!expr.has_value(), "match_arm_without_arrow", "Should reject match arm without arrow");
        }
        
        // Test match with incomplete arm: match x { 1 =>
        {
            auto tokens = create_tokens({
                {TokenType::MATCH, "match"},
                {TokenType::NON_KEYWORD_IDENTIFIER, "x"},
                {TokenType::LBRACE, "{"},
                {TokenType::INTEGER_LITERAL, "1"},
                {TokenType::FAT_ARROW, "=>"}
            });
            Parser parser(tokens);
            auto expr = parser.parse_match_expression()(tokens.begin());
            
            assert_test(!expr.has_value(), "match_incomplete_arm", "Should reject match with incomplete arm");
        }
    }
    
    // Test loop expression errors
    void test_loop_errors() {
        std::cout << "\n--- Testing Loop Expression Errors ---" << std::endl;
        
        // Test loop without body: loop
        {
            auto tokens = create_tokens({
                {TokenType::LOOP, "loop"}
            });
            Parser parser(tokens);
            auto expr = parser.parse_loop_expression()(tokens.begin());
            
            assert_test(!expr.has_value(), "loop_without_body", "Should reject loop without body");
        }
        
        // Test while without condition: while { }
        {
            auto tokens = create_tokens({
                {TokenType::WHILE, "while"},
                {TokenType::LBRACE, "{"},
                {TokenType::RBRACE, "}"}
            });
            Parser parser(tokens);
            auto expr = parser.parse_while_expression()(tokens.begin());
            
            assert_test(!expr.has_value(), "while_without_condition", "Should reject while without condition");
        }
        
        // Test while with incomplete condition: while
        {
            auto tokens = create_tokens({
                {TokenType::WHILE, "while"}
            });
            Parser parser(tokens);
            auto expr = parser.parse_while_expression()(tokens.begin());
            
            assert_test(!expr.has_value(), "while_incomplete_condition", "Should reject while with incomplete condition");
        }
    }
    
    // Test array expression errors
    void test_array_errors() {
        std::cout << "\n--- Testing Array Expression Errors ---" << std::endl;
        
        // Test incomplete array: [1, 2,
        {
            auto tokens = create_tokens({
                {TokenType::LBRACKET, "["},
                {TokenType::INTEGER_LITERAL, "1"},
                {TokenType::COMMA, ","},
                {TokenType::INTEGER_LITERAL, "2"},
                {TokenType::COMMA, ","}
            });
            Parser parser(tokens);
            auto expr = parser.parse_array_expression()(tokens.begin());
            
            assert_test(!expr.has_value(), "incomplete_array", "Should reject incomplete array");
        }
        
        // Test array without closing bracket: [1, 2
        {
            auto tokens = create_tokens({
                {TokenType::LBRACKET, "["},
                {TokenType::INTEGER_LITERAL, "1"},
                {TokenType::COMMA, ","},
                {TokenType::INTEGER_LITERAL, "2"}
            });
            Parser parser(tokens);
            auto expr = parser.parse_array_expression()(tokens.begin());
            
            assert_test(!expr.has_value(), "array_without_closing_bracket", "Should reject array without closing bracket");
        }
    }
    
    // Test struct declaration errors
    void test_struct_errors() {
        std::cout << "\n--- Testing Struct Declaration Errors ---" << std::endl;
        
        // Test struct without name: struct { }
        {
            auto tokens = create_tokens({
                {TokenType::STRUCT, "struct"},
                {TokenType::LBRACE, "{"},
                {TokenType::RBRACE, "}"}
            });
            Parser parser(tokens);
            auto ast = parser.parse();
            
            assert_test(ast == nullptr, "struct_without_name", "Should reject struct without name");
        }
        
        // Test struct without body: struct Point
        {
            auto tokens = create_tokens({
                {TokenType::STRUCT, "struct"},
                {TokenType::NON_KEYWORD_IDENTIFIER, "Point"}
            });
            Parser parser(tokens);
            auto ast = parser.parse();
            
            assert_test(ast == nullptr, "struct_without_body", "Should reject struct without body");
        }
        
        // Test struct field without type: struct Point { x: }
        {
            auto tokens = create_tokens({
                {TokenType::STRUCT, "struct"},
                {TokenType::NON_KEYWORD_IDENTIFIER, "Point"},
                {TokenType::LBRACE, "{"},
                {TokenType::NON_KEYWORD_IDENTIFIER, "x"},
                {TokenType::COLON, ":"},
                {TokenType::RBRACE, "}"}
            });
            Parser parser(tokens);
            auto ast = parser.parse();
            
            assert_test(ast == nullptr, "struct_field_without_type", "Should reject struct field without type");
        }
        
        // Test struct with incomplete field: struct Point { x
        {
            auto tokens = create_tokens({
                {TokenType::STRUCT, "struct"},
                {TokenType::NON_KEYWORD_IDENTIFIER, "Point"},
                {TokenType::LBRACE, "{"},
                {TokenType::NON_KEYWORD_IDENTIFIER, "x"}
            });
            Parser parser(tokens);
            auto ast = parser.parse();
            
            assert_test(ast == nullptr, "struct_incomplete_field", "Should reject struct with incomplete field");
        }
    }
    
    // Test token stream errors
    void test_token_stream_errors() {
        std::cout << "\n--- Testing Token Stream Errors ---" << std::endl;
        
        // Test unexpected EOF
        {
            auto tokens = create_tokens({
                {TokenType::INTEGER_LITERAL, "42"},
                {TokenType::PLUS, "+"}
            });
            // Remove the EOF token to simulate unexpected end
            tokens.pop_back();
            Parser parser(tokens);
            auto expr = parser.any_expression()(tokens.begin());
            
            assert_test(!expr.has_value(), "unexpected_eof", "Should handle unexpected EOF gracefully");
        }
        
        // Test unknown token (if supported by lexer)
        {
            auto tokens = create_tokens({
                {TokenType::UNKNOWN, "@#$"}
            });
            Parser parser(tokens);
            auto expr = parser.any_expression()(tokens.begin());
            
            assert_test(!expr.has_value(), "unknown_token", "Should reject unknown tokens");
        }
    }
    
    // Run all error tests
    void run_all_tests() {
        std::cout << "=== Parser Error Handling Unit Tests ===" << std::endl;
        
        test_syntax_errors();
        test_expression_errors();
        test_function_errors();
        test_let_statement_errors();
        test_match_errors();
        test_loop_errors();
        test_array_errors();
        test_struct_errors();
        test_token_stream_errors();
        
        print_summary();
    }
};

} // namespace rc

int main() {
    rc::ParserErrorTest test_runner;
    test_runner.run_all_tests();
    return test_runner.all_passed() ? 0 : 1;
}

#include <cassert>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include "../../include/lexer/lexer.hpp"
#include "../../include/ast/parser.hpp"
#include "../../include/ast/parsers/pattern_parser.hpp"
#include "../../include/ast/nodes/base.hpp"

/**
 * Unit tests for pattern parsing functionality.
 * Tests pattern matching constructs used in let statements, match arms, etc.
 */

namespace rc {

class PatternParserTest {
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
        std::cout << "\n=== Pattern Test Summary ===" << std::endl;
        std::cout << "Passed: " << passed_tests << "/" << total_tests << std::endl;
        if (passed_tests == total_tests) {
            std::cout << "All pattern tests passed! ✓" << std::endl;
        } else {
            std::cout << "Some pattern tests failed! ✗" << std::endl;
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
        return tokens;
    }
    
    // Test identifier patterns
    void test_identifier_patterns() {
        std::cout << "\n--- Testing Identifier Patterns ---" << std::endl;
        
        // Test simple identifier pattern: x
        {
            auto tokens = create_tokens({
                {TokenType::NON_KEYWORD_IDENTIFIER, "x"}
            });
            PatternParser pattern_parser;
            auto result = pattern_parser.parse_pattern()(tokens.begin());
            
            assert_test(result.has_value(), "simple_identifier_pattern", "Should parse identifier pattern");
            if (result.has_value()) {
                auto id_pattern = std::dynamic_pointer_cast<IdentifierPattern>(result->result);
                assert_test(id_pattern != nullptr, "identifier_pattern_type", "Should create IdentifierPattern");
                if (id_pattern) {
                    assert_test(id_pattern->name == "x", "identifier_pattern_name", "Should have correct name");
                }
            }
        }
        
        // Test mutable identifier pattern: mut x
        {
            auto tokens = create_tokens({
                {TokenType::MUT, "mut"},
                {TokenType::NON_KEYWORD_IDENTIFIER, "x"}
            });
            PatternParser pattern_parser;
            auto result = pattern_parser.parse_pattern()(tokens.begin());
            
            assert_test(result.has_value(), "mut_identifier_pattern", "Should parse mutable identifier pattern");
            if (result.has_value()) {
                auto id_pattern = std::dynamic_pointer_cast<IdentifierPattern>(result->result);
                assert_test(id_pattern != nullptr, "mut_identifier_pattern_type", "Should create IdentifierPattern");
                if (id_pattern) {
                    assert_test(id_pattern->name == "x", "mut_identifier_pattern_name", "Should have correct name");
                    assert_test(id_pattern->is_mutable, "mut_identifier_pattern_mut", "Should be mutable");
                }
            }
        }
    }
    
    // Test literal patterns
    void test_literal_patterns() {
        std::cout << "\n--- Testing Literal Patterns ---" << std::endl;
        
        // Test integer literal pattern: 42
        {
            auto tokens = create_tokens({
                {TokenType::INTEGER_LITERAL, "42"}
            });
            PatternParser pattern_parser;
            auto result = pattern_parser.parse_pattern()(tokens.begin());
            
            assert_test(result.has_value(), "integer_literal_pattern", "Should parse integer literal pattern");
            if (result.has_value()) {
                auto literal_pattern = std::dynamic_pointer_cast<LiteralPattern>(result->result);
                assert_test(literal_pattern != nullptr, "integer_literal_pattern_type", "Should create LiteralPattern");
                if (literal_pattern) {
                    assert_test(literal_pattern->value == "42", "integer_literal_pattern_value", "Should have correct value");
                }
            }
        }
        
        // Test string literal pattern: "hello"
        {
            auto tokens = create_tokens({
                {TokenType::STRING_LITERAL, "\"hello\""}
            });
            PatternParser pattern_parser;
            auto result = pattern_parser.parse_pattern()(tokens.begin());
            
            assert_test(result.has_value(), "string_literal_pattern", "Should parse string literal pattern");
            if (result.has_value()) {
                auto literal_pattern = std::dynamic_pointer_cast<LiteralPattern>(result->result);
                assert_test(literal_pattern != nullptr, "string_literal_pattern_type", "Should create LiteralPattern");
                if (literal_pattern) {
                    assert_test(literal_pattern->value == "\"hello\"", "string_literal_pattern_value", "Should have correct value");
                }
            }
        }
        
        // Test boolean literal pattern: true
        {
            auto tokens = create_tokens({
                {TokenType::TRUE, "true"}
            });
            PatternParser pattern_parser;
            auto result = pattern_parser.parse_pattern()(tokens.begin());
            
            assert_test(result.has_value(), "boolean_literal_pattern", "Should parse boolean literal pattern");
            if (result.has_value()) {
                auto literal_pattern = std::dynamic_pointer_cast<LiteralPattern>(result->result);
                assert_test(literal_pattern != nullptr, "boolean_literal_pattern_type", "Should create LiteralPattern");
            }
        }
        
        // Test character literal pattern: 'a'
        {
            auto tokens = create_tokens({
                {TokenType::CHAR_LITERAL, "'a'"}
            });
            PatternParser pattern_parser;
            auto result = pattern_parser.parse_pattern()(tokens.begin());
            
            assert_test(result.has_value(), "char_literal_pattern", "Should parse character literal pattern");
            if (result.has_value()) {
                auto literal_pattern = std::dynamic_pointer_cast<LiteralPattern>(result->result);
                assert_test(literal_pattern != nullptr, "char_literal_pattern_type", "Should create LiteralPattern");
            }
        }
    }
    
    // Test wildcard patterns
    void test_wildcard_patterns() {
        std::cout << "\n--- Testing Wildcard Patterns ---" << std::endl;
        
        // Test simple wildcard: _
        {
            auto tokens = create_tokens({
                {TokenType::UNDERSCORE, "_"}
            });
            PatternParser pattern_parser;
            auto result = pattern_parser.parse_pattern()(tokens.begin());
            
            assert_test(result.has_value(), "wildcard_pattern", "Should parse wildcard pattern");
            if (result.has_value()) {
                auto wildcard_pattern = std::dynamic_pointer_cast<WildcardPattern>(result->result);
                assert_test(wildcard_pattern != nullptr, "wildcard_pattern_type", "Should create WildcardPattern");
            }
        }
    }
    
    // Test tuple patterns
    void test_tuple_patterns() {
        std::cout << "\n--- Testing Tuple Patterns ---" << std::endl;
        
        // Test simple tuple pattern: (x, y)
        {
            auto tokens = create_tokens({
                {TokenType::LPAREN, "("},
                {TokenType::NON_KEYWORD_IDENTIFIER, "x"},
                {TokenType::COMMA, ","},
                {TokenType::NON_KEYWORD_IDENTIFIER, "y"},
                {TokenType::RPAREN, ")"}
            });
            PatternParser pattern_parser;
            auto result = pattern_parser.parse_pattern()(tokens.begin());
            
            assert_test(result.has_value(), "tuple_pattern", "Should parse tuple pattern");
            if (result.has_value()) {
                auto tuple_pattern = std::dynamic_pointer_cast<TuplePattern>(result->result);
                assert_test(tuple_pattern != nullptr, "tuple_pattern_type", "Should create TuplePattern");
                if (tuple_pattern) {
                    assert_test(tuple_pattern->elements.size() == 2, "tuple_pattern_elements", "Should have 2 elements");
                }
            }
        }
        
        // Test nested tuple pattern: (x, (y, z))
        {
            auto tokens = create_tokens({
                {TokenType::LPAREN, "("},
                {TokenType::NON_KEYWORD_IDENTIFIER, "x"},
                {TokenType::COMMA, ","},
                {TokenType::LPAREN, "("},
                {TokenType::NON_KEYWORD_IDENTIFIER, "y"},
                {TokenType::COMMA, ","},
                {TokenType::NON_KEYWORD_IDENTIFIER, "z"},
                {TokenType::RPAREN, ")"},
                {TokenType::RPAREN, ")"}
            });
            PatternParser pattern_parser;
            auto result = pattern_parser.parse_pattern()(tokens.begin());
            
            assert_test(result.has_value(), "nested_tuple_pattern", "Should parse nested tuple pattern");
            if (result.has_value()) {
                auto tuple_pattern = std::dynamic_pointer_cast<TuplePattern>(result->result);
                assert_test(tuple_pattern != nullptr, "nested_tuple_pattern_type", "Should create TuplePattern");
                if (tuple_pattern) {
                    assert_test(tuple_pattern->elements.size() == 2, "nested_tuple_pattern_elements", "Should have 2 elements");
                    
                    // Check that second element is also a tuple pattern
                    auto nested_tuple = std::dynamic_pointer_cast<TuplePattern>(tuple_pattern->elements[1]);
                    assert_test(nested_tuple != nullptr, "nested_tuple_pattern_nested", "Second element should be tuple pattern");
                    if (nested_tuple) {
                        assert_test(nested_tuple->elements.size() == 2, "nested_tuple_pattern_nested_elements", "Nested tuple should have 2 elements");
                    }
                }
            }
        }
        
        // Test single element tuple pattern: (x,)
        {
            auto tokens = create_tokens({
                {TokenType::LPAREN, "("},
                {TokenType::NON_KEYWORD_IDENTIFIER, "x"},
                {TokenType::COMMA, ","},
                {TokenType::RPAREN, ")"}
            });
            PatternParser pattern_parser;
            auto result = pattern_parser.parse_pattern()(tokens.begin());
            
            assert_test(result.has_value(), "single_tuple_pattern", "Should parse single element tuple pattern");
            if (result.has_value()) {
                auto tuple_pattern = std::dynamic_pointer_cast<TuplePattern>(result->result);
                assert_test(tuple_pattern != nullptr, "single_tuple_pattern_type", "Should create TuplePattern");
                if (tuple_pattern) {
                    assert_test(tuple_pattern->elements.size() == 1, "single_tuple_pattern_elements", "Should have 1 element");
                }
            }
        }
    }
    
    // Test struct patterns
    void test_struct_patterns() {
        std::cout << "\n--- Testing Struct Patterns ---" << std::endl;
        
        // Test simple struct pattern: Point { x, y }
        {
            auto tokens = create_tokens({
                {TokenType::NON_KEYWORD_IDENTIFIER, "Point"},
                {TokenType::LBRACE, "{"},
                {TokenType::NON_KEYWORD_IDENTIFIER, "x"},
                {TokenType::COMMA, ","},
                {TokenType::NON_KEYWORD_IDENTIFIER, "y"},
                {TokenType::RBRACE, "}"}
            });
            PatternParser pattern_parser;
            auto result = pattern_parser.parse_pattern()(tokens.begin());
            
            assert_test(result.has_value(), "struct_pattern", "Should parse struct pattern");
            if (result.has_value()) {
                auto struct_pattern = std::dynamic_pointer_cast<StructPattern>(result->result);
                assert_test(struct_pattern != nullptr, "struct_pattern_type", "Should create StructPattern");
                if (struct_pattern) {
                    assert_test(struct_pattern->path == "Point", "struct_pattern_path", "Should have correct path");
                    assert_test(struct_pattern->fields.size() == 2, "struct_pattern_fields", "Should have 2 fields");
                }
            }
        }
        
        // Test struct pattern with field patterns: Point { x: a, y: b }
        {
            auto tokens = create_tokens({
                {TokenType::NON_KEYWORD_IDENTIFIER, "Point"},
                {TokenType::LBRACE, "{"},
                {TokenType::NON_KEYWORD_IDENTIFIER, "x"},
                {TokenType::COLON, ":"},
                {TokenType::NON_KEYWORD_IDENTIFIER, "a"},
                {TokenType::COMMA, ","},
                {TokenType::NON_KEYWORD_IDENTIFIER, "y"},
                {TokenType::COLON, ":"},
                {TokenType::NON_KEYWORD_IDENTIFIER, "b"},
                {TokenType::RBRACE, "}"}
            });
            PatternParser pattern_parser;
            auto result = pattern_parser.parse_pattern()(tokens.begin());
            
            assert_test(result.has_value(), "struct_field_pattern", "Should parse struct pattern with field patterns");
            if (result.has_value()) {
                auto struct_pattern = std::dynamic_pointer_cast<StructPattern>(result->result);
                assert_test(struct_pattern != nullptr, "struct_field_pattern_type", "Should create StructPattern");
                if (struct_pattern) {
                    assert_test(struct_pattern->fields.size() == 2, "struct_field_pattern_fields", "Should have 2 fields");
                }
            }
        }
        
        // Test struct pattern with rest: Point { x, .. }
        {
            auto tokens = create_tokens({
                {TokenType::NON_KEYWORD_IDENTIFIER, "Point"},
                {TokenType::LBRACE, "{"},
                {TokenType::NON_KEYWORD_IDENTIFIER, "x"},
                {TokenType::COMMA, ","},
                {TokenType::DOT_DOT, ".."},
                {TokenType::RBRACE, "}"}
            });
            PatternParser pattern_parser;
            auto result = pattern_parser.parse_pattern()(tokens.begin());
            
            assert_test(result.has_value(), "struct_rest_pattern", "Should parse struct pattern with rest");
            if (result.has_value()) {
                auto struct_pattern = std::dynamic_pointer_cast<StructPattern>(result->result);
                assert_test(struct_pattern != nullptr, "struct_rest_pattern_type", "Should create StructPattern");
                if (struct_pattern) {
                    assert_test(struct_pattern->fields.size() >= 1, "struct_rest_pattern_fields", "Should have at least 1 field");
                    assert_test(struct_pattern->has_rest, "struct_rest_pattern_rest", "Should have rest indicator");
                }
            }
        }
    }
    
    // Test slice patterns
    void test_slice_patterns() {
        std::cout << "\n--- Testing Slice Patterns ---" << std::endl;
        
        // Test simple slice pattern: [x, y, z]
        {
            auto tokens = create_tokens({
                {TokenType::LBRACKET, "["},
                {TokenType::NON_KEYWORD_IDENTIFIER, "x"},
                {TokenType::COMMA, ","},
                {TokenType::NON_KEYWORD_IDENTIFIER, "y"},
                {TokenType::COMMA, ","},
                {TokenType::NON_KEYWORD_IDENTIFIER, "z"},
                {TokenType::RBRACKET, "]"}
            });
            PatternParser pattern_parser;
            auto result = pattern_parser.parse_pattern()(tokens.begin());
            
            assert_test(result.has_value(), "slice_pattern", "Should parse slice pattern");
            if (result.has_value()) {
                auto slice_pattern = std::dynamic_pointer_cast<SlicePattern>(result->result);
                assert_test(slice_pattern != nullptr, "slice_pattern_type", "Should create SlicePattern");
                if (slice_pattern) {
                    assert_test(slice_pattern->elements.size() == 3, "slice_pattern_elements", "Should have 3 elements");
                }
            }
        }
        
        // Test slice pattern with rest: [x, .., z]
        {
            auto tokens = create_tokens({
                {TokenType::LBRACKET, "["},
                {TokenType::NON_KEYWORD_IDENTIFIER, "x"},
                {TokenType::COMMA, ","},
                {TokenType::DOT_DOT, ".."},
                {TokenType::COMMA, ","},
                {TokenType::NON_KEYWORD_IDENTIFIER, "z"},
                {TokenType::RBRACKET, "]"}
            });
            PatternParser pattern_parser;
            auto result = pattern_parser.parse_pattern()(tokens.begin());
            
            assert_test(result.has_value(), "slice_rest_pattern", "Should parse slice pattern with rest");
            if (result.has_value()) {
                auto slice_pattern = std::dynamic_pointer_cast<SlicePattern>(result->result);
                assert_test(slice_pattern != nullptr, "slice_rest_pattern_type", "Should create SlicePattern");
                if (slice_pattern) {
                    // Check for rest element
                    bool has_rest = false;
                    for (const auto& element : slice_pattern->elements) {
                        if (std::dynamic_pointer_cast<RestPattern>(element)) {
                            has_rest = true;
                            break;
                        }
                    }
                    assert_test(has_rest, "slice_rest_pattern_rest", "Should have rest element");
                }
            }
        }
        
        // Test empty slice pattern: []
        {
            auto tokens = create_tokens({
                {TokenType::LBRACKET, "["},
                {TokenType::RBRACKET, "]"}
            });
            PatternParser pattern_parser;
            auto result = pattern_parser.parse_pattern()(tokens.begin());
            
            assert_test(result.has_value(), "empty_slice_pattern", "Should parse empty slice pattern");
            if (result.has_value()) {
                auto slice_pattern = std::dynamic_pointer_cast<SlicePattern>(result->result);
                assert_test(slice_pattern != nullptr, "empty_slice_pattern_type", "Should create SlicePattern");
                if (slice_pattern) {
                    assert_test(slice_pattern->elements.empty(), "empty_slice_pattern_elements", "Should have no elements");
                }
            }
        }
    }
    
    // Test reference patterns
    void test_reference_patterns() {
        std::cout << "\n--- Testing Reference Patterns ---" << std::endl;
        
        // Test reference pattern: &x
        {
            auto tokens = create_tokens({
                {TokenType::AND, "&"},
                {TokenType::NON_KEYWORD_IDENTIFIER, "x"}
            });
            PatternParser pattern_parser;
            auto result = pattern_parser.parse_pattern()(tokens.begin());
            
            assert_test(result.has_value(), "reference_pattern", "Should parse reference pattern");
            if (result.has_value()) {
                auto ref_pattern = std::dynamic_pointer_cast<ReferencePattern>(result->result);
                assert_test(ref_pattern != nullptr, "reference_pattern_type", "Should create ReferencePattern");
                if (ref_pattern) {
                    assert_test(ref_pattern->pattern != nullptr, "reference_pattern_inner", "Should have inner pattern");
                    assert_test(!ref_pattern->is_mutable, "reference_pattern_immutable", "Should be immutable reference");
                }
            }
        }
        
        // Test mutable reference pattern: &mut x
        {
            auto tokens = create_tokens({
                {TokenType::AND, "&"},
                {TokenType::MUT, "mut"},
                {TokenType::NON_KEYWORD_IDENTIFIER, "x"}
            });
            PatternParser pattern_parser;
            auto result = pattern_parser.parse_pattern()(tokens.begin());
            
            assert_test(result.has_value(), "mut_reference_pattern", "Should parse mutable reference pattern");
            if (result.has_value()) {
                auto ref_pattern = std::dynamic_pointer_cast<ReferencePattern>(result->result);
                assert_test(ref_pattern != nullptr, "mut_reference_pattern_type", "Should create ReferencePattern");
                if (ref_pattern) {
                    assert_test(ref_pattern->pattern != nullptr, "mut_reference_pattern_inner", "Should have inner pattern");
                    assert_test(ref_pattern->is_mutable, "mut_reference_pattern_mutable", "Should be mutable reference");
                }
            }
        }
    }
    
    // Test complex pattern combinations
    void test_complex_patterns() {
        std::cout << "\n--- Testing Complex Pattern Combinations ---" << std::endl;
        
        // Test nested patterns: (Point { x, y }, &z)
        {
            auto tokens = create_tokens({
                {TokenType::LPAREN, "("},
                {TokenType::NON_KEYWORD_IDENTIFIER, "Point"},
                {TokenType::LBRACE, "{"},
                {TokenType::NON_KEYWORD_IDENTIFIER, "x"},
                {TokenType::COMMA, ","},
                {TokenType::NON_KEYWORD_IDENTIFIER, "y"},
                {TokenType::RBRACE, "}"},
                {TokenType::COMMA, ","},
                {TokenType::AND, "&"},
                {TokenType::NON_KEYWORD_IDENTIFIER, "z"},
                {TokenType::RPAREN, ")"}
            });
            PatternParser pattern_parser;
            auto result = pattern_parser.parse_pattern()(tokens.begin());
            
            assert_test(result.has_value(), "complex_nested_pattern", "Should parse complex nested pattern");
            if (result.has_value()) {
                auto tuple_pattern = std::dynamic_pointer_cast<TuplePattern>(result->result);
                assert_test(tuple_pattern != nullptr, "complex_nested_pattern_type", "Should create TuplePattern");
                if (tuple_pattern) {
                    assert_test(tuple_pattern->elements.size() == 2, "complex_nested_pattern_elements", "Should have 2 elements");
                    
                    // First element should be struct pattern
                    auto struct_pattern = std::dynamic_pointer_cast<StructPattern>(tuple_pattern->elements[0]);
                    assert_test(struct_pattern != nullptr, "complex_nested_pattern_struct", "First element should be struct pattern");
                    
                    // Second element should be reference pattern
                    auto ref_pattern = std::dynamic_pointer_cast<ReferencePattern>(tuple_pattern->elements[1]);
                    assert_test(ref_pattern != nullptr, "complex_nested_pattern_ref", "Second element should be reference pattern");
                }
            }
        }
    }
    
    // Run all pattern tests
    void run_all_tests() {
        std::cout << "=== Pattern Parser Unit Tests ===" << std::endl;
        
        test_identifier_patterns();
        test_literal_patterns();
        test_wildcard_patterns();
        test_tuple_patterns();
        test_struct_patterns();
        test_slice_patterns();
        test_reference_patterns();
        test_complex_patterns();
        
        print_summary();
    }
};

} // namespace rc

int main() {
    rc::PatternParserTest test_runner;
    test_runner.run_all_tests();
    return test_runner.all_passed() ? 0 : 1;
}

#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <memory>
#include <map>
#include <cassert>

#include "../../include/preprocessor/preprocessor.hpp"
#include "../../include/lexer/lexer.hpp"
#include "../../include/ast/parser.hpp"
#include "../../include/ast/visitors/pretty_print.hpp"

/**
 * Comprehensive parser test runner that validates AST structure.
 * Unlike the smoke runner, this validates the actual parsed structure.
 */

// Test result structure
struct TestResult {
    bool success;
    std::string message;
    std::string actual_output;
    std::string expected_output;
};

// Read file contents
std::string read_file_contents(const std::string& filepath) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot open file: " + filepath);
    }
    std::ostringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

// Normalize whitespace for comparison
std::string normalize_whitespace(const std::string& str) {
    std::ostringstream result;
    bool prev_space = false;
    
    for (char c : str) {
        if (std::isspace(c)) {
            if (!prev_space) {
                result << ' ';
                prev_space = true;
            }
        } else {
            result << c;
            prev_space = false;
        }
    }
    
    std::string normalized = result.str();
    // Trim leading and trailing spaces
    size_t start = normalized.find_first_not_of(' ');
    size_t end = normalized.find_last_not_of(' ');
    
    if (start == std::string::npos) return "";
    return normalized.substr(start, end - start + 1);
}

// Test parser functionality
TestResult test_parser(const std::string& input_file, const std::string& expected_file) {
    TestResult result;
    result.success = false;
    
    try {
        // Parse the input file
        rc::Preprocessor pre(input_file);
        auto preprocessed = pre.preprocess();
        
        rc::Lexer lexer(preprocessed);
        auto tokens = lexer.tokenize();
        
        rc::Parser parser(tokens);
        auto ast = parser.parse();
        
        if (!ast) {
            result.message = "Parser returned null AST";
            return result;
        }
        
        // Generate pretty-printed output
        rc::PrettyPrintVisitor visitor;
        ast->accept(visitor);
        result.actual_output = visitor.get_result();
        
        // Read expected output
        if (!expected_file.empty()) {
            result.expected_output = read_file_contents(expected_file);
            
            // Normalize both outputs for comparison
            std::string norm_actual = normalize_whitespace(result.actual_output);
            std::string norm_expected = normalize_whitespace(result.expected_output);
            
            if (norm_actual == norm_expected) {
                result.success = true;
                result.message = "AST structure matches expected output";
            } else {
                result.message = "AST structure does not match expected output";
            }
        } else {
            // No expected file - just verify parsing succeeds and AST is valid
            result.success = true;
            result.message = "Parsing succeeded, AST has " + std::to_string(ast->children.size()) + " top-level items";
        }
        
    } catch (const std::exception& e) {
        result.message = "Exception: " + std::string(e.what());
    }
    
    return result;
}

// Detailed AST validation
bool validate_ast_structure(std::shared_ptr<rc::RootNode> ast, const std::string& input_file) {
    if (!ast) {
        std::cerr << "ERROR: AST is null for " << input_file << std::endl;
        return false;
    }
    
    // Basic structural validation
    std::cout << "AST Validation for " << input_file << ":" << std::endl;
    std::cout << "  - Top-level items: " << ast->children.size() << std::endl;
    
    // Validate each top-level item
    for (size_t i = 0; i < ast->children.size(); ++i) {
        auto& item = ast->children[i];
        if (!item) {
            std::cerr << "ERROR: Item " << i << " is null" << std::endl;
            return false;
        }
        
        // Try to identify item type (would need proper visitor pattern for full validation)
        std::cout << "  - Item " << i << ": " << typeid(*item).name() << std::endl;
    }
    
    return true;
}

int main(int argc, char** argv) {
    if (argc < 2 || argc > 3) {
        std::cerr << "Usage: " << argv[0] << " <input.rs> [expected.ast]" << std::endl;
        std::cerr << "  input.rs     - Rust source file to parse" << std::endl;
        std::cerr << "  expected.ast - Optional expected AST pretty-print output" << std::endl;
        return 2;
    }
    
    std::string input_file = argv[1];
    std::string expected_file = (argc == 3) ? argv[2] : "";
    
    // Run the test
    TestResult result = test_parser(input_file, expected_file);
    
    std::cout << "=== Parser Test Results ===" << std::endl;
    std::cout << "Input file: " << input_file << std::endl;
    std::cout << "Expected file: " << (expected_file.empty() ? "(none)" : expected_file) << std::endl;
    std::cout << "Status: " << (result.success ? "PASS" : "FAIL") << std::endl;
    std::cout << "Message: " << result.message << std::endl;
    
    if (!result.success && !expected_file.empty()) {
        std::cout << "\n=== Expected Output ===" << std::endl;
        std::cout << result.expected_output << std::endl;
        std::cout << "\n=== Actual Output ===" << std::endl;
        std::cout << result.actual_output << std::endl;
    } else if (result.success && !result.actual_output.empty()) {
        std::cout << "\n=== Generated AST ===" << std::endl;
        std::cout << result.actual_output << std::endl;
    }
    
    return result.success ? 0 : 1;
}

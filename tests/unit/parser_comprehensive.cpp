#include <filesystem>
#include <iostream>
#include <string>
#include <vector>
#include <optional>
#include <algorithm>

#include "../../include/preprocessor/preprocessor.hpp"
#include "../../include/lexer/lexer.hpp"
#include "../../include/ast/parser.hpp"
#include "../../include/ast/visitors/pretty_print.hpp"

namespace fs = std::filesystem;

struct CaseResult { std::string path; bool expected_valid; bool passed; std::string detail; };

static bool parse_ok_with_ast(const std::string &path, std::string &ast_out) {
    try {
        rc::Preprocessor pre(path);
        auto preprocessed = pre.preprocess();
        rc::Lexer lexer(preprocessed);
        auto tokens = lexer.tokenize();
        rc::Parser parser(tokens);
        auto ast = parser.parse();
        if(!ast) return false;
        rc::PrettyPrintVisitor pp; ast->accept(pp); ast_out = pp.get_result();
        return true;
    } catch(const std::exception &e) {
        ast_out.clear();
        return false;
    }
}

static void collect_rs_files_recursive(const fs::path &dir, std::vector<fs::path> &out) {
    if(!fs::exists(dir)) return;
    for(auto &p : fs::recursive_directory_iterator(dir)) {
        if(p.is_regular_file() && p.path().extension()==".rs") out.push_back(p.path());
    }
}

int main() {
    const fs::path base = "tests/data/parser/comprehensive";
    const fs::path valid_dir = base / "valid";
    const fs::path invalid_dir = base / "invalid";

    if(!fs::exists(valid_dir) || !fs::exists(invalid_dir)) {
        std::cerr << "Comprehensive parser test directories missing" << std::endl;
        return 2;
    }

    std::vector<fs::path> valid_files, invalid_files;
    collect_rs_files_recursive(valid_dir, valid_files);
    collect_rs_files_recursive(invalid_dir, invalid_files);

    // Also include existing AST examples as part of comprehensive valid set for breadth if present.
    collect_rs_files_recursive("examples/ast", valid_files);
    // de-duplicate
    std::sort(valid_files.begin(), valid_files.end());
    valid_files.erase(std::unique(valid_files.begin(), valid_files.end()), valid_files.end());

    if(valid_files.empty()) { std::cerr << "No comprehensive valid files" << std::endl; return 2; }
    if(invalid_files.empty()) { std::cerr << "No comprehensive invalid files" << std::endl; return 2; }

    std::vector<CaseResult> results;
    for(const auto &f : valid_files) {
        std::string ast_pp; bool ok = parse_ok_with_ast(f.string(), ast_pp);
        bool good = ok && !ast_pp.empty();
        results.push_back({f.string(), true, good, good?"":"parse or AST gen failed"});
    }
    for(const auto &f : invalid_files) {
        std::string ast_pp; bool ok = parse_ok_with_ast(f.string(), ast_pp);
        bool good = !ok; // expect failure
        results.push_back({f.string(), false, good, good?"":"unexpectedly parsed"});
    }

    size_t pass = 0; for(auto &r: results) if(r.passed) pass++;
    std::cout << "=== Comprehensive Parser Tests ===\n";
    for(auto &r: results) {
        std::cout << (r.passed?"PASS ":"FAIL ") << (r.expected_valid?"valid ":"invalid ") << r.path;
        if(!r.passed && !r.detail.empty()) std::cout << " - " << r.detail;
        std::cout << "\n";
    }
    std::cout << "Summary: " << pass << "/" << results.size() << " passed" << std::endl;

    bool all_valid_ok = true, all_invalid_ok = true;
    for(auto &r: results) {
        if(r.expected_valid && !r.passed) all_valid_ok = false;
        if(!r.expected_valid && !r.passed) all_invalid_ok = false; // already inverted
    }

    if(all_valid_ok && all_invalid_ok) { std::cout << "COMPREHENSIVE PARSER RESULT: SUCCESS" << std::endl; return 0; }
    std::cout << "COMPREHENSIVE PARSER RESULT: FAILURE" << std::endl; return 1;
}

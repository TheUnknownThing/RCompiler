#include <filesystem>
#include <iostream>
#include <string>
#include <vector>
#include <optional>

#include "../../include/preprocessor/preprocessor.hpp"
#include "../../include/lexer/lexer.hpp"
#include "../../include/ast/parser.hpp"

namespace fs = std::filesystem;

struct FileResult { std::string path; bool expected_valid; bool passed; std::string message; };

static bool parse_source(const std::string &path) {
    try {
        rc::Preprocessor pre(path);
        auto preprocessed = pre.preprocess();
        rc::Lexer lexer(preprocessed);
        auto tokens = lexer.tokenize();
        rc::Parser parser(tokens);
        auto ast = parser.parse();
        return ast != nullptr; // parse() throws on hard failure
    } catch(const std::exception &e) {
        return false; // treated as parse failure
    }
}

static void collect_rs_files(const fs::path &dir, std::vector<fs::path> &out) {
    if(!fs::exists(dir)) return;
    for (auto &p : fs::directory_iterator(dir)) {
        if (p.is_regular_file() && p.path().extension() == ".rs") out.push_back(p.path());
    }
}

int main() {
    std::vector<FileResult> results;

    const fs::path base = "tests/data/parser/basic";
    const fs::path valid_dir = base / "valid";
    const fs::path invalid_dir = base / "invalid";

    if(!fs::exists(valid_dir) || !fs::exists(invalid_dir)) {
        std::cerr << "Basic parser test directories missing" << std::endl;
        return 2;
    }

    std::vector<fs::path> valid_files, invalid_files;
    collect_rs_files(valid_dir, valid_files);
    collect_rs_files(invalid_dir, invalid_files);

    if(valid_files.empty() || invalid_files.empty()) {
        std::cerr << "Basic parser requires at least one valid and one invalid file" << std::endl;
        return 2;
    }

    // Basic success criteria: every valid parses, every invalid fails.
    for(const auto &f : valid_files) {
        bool ok = parse_source(f.string());
        results.push_back({f.string(), true, ok, ok?"":"parse failed"});
    }
    for(const auto &f : invalid_files) {
        bool ok = parse_source(f.string());
        results.push_back({f.string(), false, !ok, ok?"unexpectedly parsed":""});
    }

    size_t pass = 0; for(auto &r: results) if (r.passed) pass++;
    std::cout << "=== Basic Parser Tests ===\n";
    for(auto &r: results) {
        std::cout << (r.passed?"PASS ":"FAIL ") << (r.expected_valid?"valid ":"invalid ") << r.path;
        if(!r.passed && !r.message.empty()) std::cout << " - " << r.message;
        std::cout << "\n";
    }
    std::cout << "Summary: " << pass << "/" << results.size() << " passed" << std::endl;

    bool all_valid_ok = true, all_invalid_ok = true;
    for(auto &r: results) {
        if(r.expected_valid && !r.passed) all_valid_ok = false;
        if(!r.expected_valid && !r.passed) all_invalid_ok = false; // r.passed already inverted for invalid
    }

    if(all_valid_ok && all_invalid_ok) {
        std::cout << "BASIC PARSER RESULT: SUCCESS" << std::endl;
        return 0;
    }
    std::cout << "BASIC PARSER RESULT: FAILURE" << std::endl;
    return 1;
}

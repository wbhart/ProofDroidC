#include <iostream>
#include <string>
#include "../src/grammar.h"

// Function to test parsing and checking the resulting AST's repr format
bool test_parser(const std::string& input, int test_number) {
    manager_t mgr;
    mgr.input = nullptr; // Initialize manager input
    mgr.pos = 0;

    parser_context_t *ctx = parser_create(&mgr);
    node* ast = nullptr;

    std::string modified_input = input;  // Copy the input string
    modified_input.push_back('\n');  // Add newline to the input, just like in proof_droid

    // Set the input buffer and reset position
    mgr.input = modified_input.c_str();
    mgr.pos = 0;

    // Parse the input
    parser_parse(ctx, &ast);

    if (!ast) {
        std::cerr << "Syntax error near position " << mgr.pos << ":\n";
        std::cerr << "Failed to parse expected string at position: " << mgr.pos << std::endl;
        std::cerr << "Remaining input: '" << &mgr.input[mgr.pos] << "'" << std::endl;
        parser_destroy(ctx);
        return false;
    }

    // Compare the REPR output of the AST with the input
    std::string repr_output = ast->to_string(OutputFormat::REPR);
    delete ast;  // Clean up the AST
    parser_destroy(ctx);

    if (repr_output != input) {
        std::cerr << "Test case #" << test_number << " mismatch:\n";
        std::cerr << "Expected: [" << input << "]\n";
        std::cerr << "Got:      [" << repr_output << "]\n";
        return false;
    }

    return true;
}

int main() {
    std::vector<std::string> test_cases = {
        "a = b",
        "f(a) = b",
        "\\mathcal{P}(S) = T"
    };

    std::cout << "Running tests..." << std::endl;

    bool all_tests_passed = true;
    for (size_t i = 0; i < test_cases.size(); ++i) {
        if (!test_parser(test_cases[i], i + 1)) {
            all_tests_passed = false;
        }
    }

    if (all_tests_passed) {
        std::cout << "All tests passed!" << std::endl;
    } else {
        std::cout << "Some tests failed." << std::endl;
    }

    return all_tests_passed ? 0 : 1;
}

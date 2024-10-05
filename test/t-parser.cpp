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
        std::cerr << "Syntax error near position " << mgr.pos << ":" << std::endl;
        std::cerr << "Input string: " << input << std::endl;
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
        "\\mathcal{P}(S) = T",
        "f(g(t)) = (a, f(t), \\emptyset)",
        "A = \\emptyset",
        "() = f(g(\\emptyset))",
        "S \\cup T = \\emptyset",
        "S \\cup T \\times (A \\cap B) = \\emptyset",
        "(S \\cup T) \\times (A \\cap B) = \\emptyset",
        "A \\setminus B = f(U)",
        "A \\subseteq B",
        "A \\subset B",
        "A \\cap B \\subseteq \\emptyset",
        "A \\cap (B \\cup C) \\subset f(T) \\cup \\mathcal{P}(S)",
        "P(x)",
        "Q(A \\cup B)",
        "P(f(x))",
        "\\neg P(x)",
        "\\top",
        "\\bot",
        "\\neg \\top",
        "\\neg P(x)",
        "\\neg (A \\subseteq B)",
        "A \\cup B \\neq C",
        "(P(x) \\vee Q(y)) \\wedge R(z)",
        "P(x) \\vee (Q(y) \\wedge R(z))",
        "P(x) = \\emptyset \\vee \\mathcal{P}(S) \\subseteq T",
        "x \\neq y",
        "A \\cup B \\neq \\emptyset",
        "A \\cup (B \\cap C) \\neq \\mathcal{P}(\\emptyset)",
        "A = B \\implies P(x)",
        "(P(x) \\implies Q(y)) \\iff Q(x)",
        "\\forall x (x = y)",
        "\\exists x P(x)",
        "\\forall x (P(x) \\vee Q(x))",
        "\\forall x \\forall y P(x, y)",
        "\\forall x \\forall y (P(x) \\vee Q(y))"  
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

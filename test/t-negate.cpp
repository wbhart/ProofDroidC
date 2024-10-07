// t-negate.cpp

#include <iostream>
#include <string>
#include <vector>
#include "../src/grammar.h"
#include "../src/node.h"

// Function to test double negation of a formula
bool test_double_negation(const std::string& input, int test_number) {
    manager_t mgr;
    mgr.input = nullptr; // Initialize manager input
    mgr.pos = 0;

    parser_context_t *ctx = parser_create(&mgr);
    node* ast_original = nullptr;

    std::string modified_input = input;  // Copy the input string
    modified_input.push_back('\n');  // Add newline to the input, just like in proof_droid

    // Set the input buffer and reset position
    mgr.input = modified_input.c_str();
    mgr.pos = 0;

    // Parse the input
    parser_parse(ctx, &ast_original);

    if (!ast_original) {
        std::cerr << "Test #" << test_number << " failed: Syntax error near position " << mgr.pos << ":\n";
        std::cerr << "Input string: " << input << std::endl;
        parser_destroy(ctx);
        return false;
    }

    // Create a deep copy of the original AST
    node* ast_copy = deep_copy(ast_original);
    if (!ast_copy) {
        std::cerr << "Test #" << test_number << " failed: Failed to deep copy the AST." << std::endl;
        delete ast_original;
        parser_destroy(ctx);
        return false;
    }

    // Apply negate twice
    node* ast_negated_once = negate_node(ast_copy);
    if (!ast_negated_once) {
        std::cerr << "Test #" << test_number << " failed: First negation returned nullptr." << std::endl;
        delete ast_copy;
        delete ast_original;
        parser_destroy(ctx);
        return false;
    }

    node* ast_negated_twice = negate_node(ast_negated_once);
    if (!ast_negated_twice) {
        std::cerr << "Test #" << test_number << " failed: Second negation returned nullptr." << std::endl;
        delete ast_negated_once;
        delete ast_copy;
        delete ast_original;
        parser_destroy(ctx);
        return false;
    }

    // Generate repr strings for comparison
    std::string repr_original = ast_original->to_string(REPR);
    std::string repr_double_negated = ast_negated_twice->to_string(REPR);

    // Compare the original and double-negated repr strings
    bool pass = (repr_original == repr_double_negated);

    if (!pass) {
        std::cerr << "Test #" << test_number << " failed:\n";
        std::cerr << "Original Formula:      [" << repr_original << "]\n";
        std::cerr << "Double-Negated Formula:[" << repr_double_negated << "]\n";
    }

    // Clean up memory
    delete ast_negated_twice; // This deletes ast_negated_once and ast_copy if ownership is correctly managed
    delete ast_original;
    parser_destroy(ctx);

    return pass;
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
        "\\neg P(x)",
        "\\neg (A \\subseteq B)",
        "A \\cup B \\neq C",
        "(P(x) \\vee Q(y)) \\wedge R(z)",
        "P(x) \\vee (Q(y) \\wedge R(z))",
        "P(x) = \\emptyset \\vee \\mathcal{P}(S) \\subseteq T",
        "x \\neq y",
        "A \\cup B \\neq \\emptyset",
        "A \\cup (B \\cap C) \\neq \\mathcal{P}(\\emptyset)",
        "A \\neq B \\vee P(x)",
        "\\forall x (x = y)",
        "\\exists x P(x)",
        "\\forall x (P(x) \\vee Q(x))",
        "\\forall x \\forall y P(x, y)",
        "\\forall x \\forall y (P(x) \\vee Q(y))"  
    };

    std::cout << "Running tests..." << std::endl;

    bool all_tests_passed = true;
    for (size_t i = 0; i < test_cases.size(); ++i) {
        bool result = test_double_negation(test_cases[i], i + 1);
        if (!result) {
            all_tests_passed = false;
        }
    }

    if (all_tests_passed) {
        std::cout << "All tests passed!" << std::endl;
        return 0;
    }
}


// t-skolemize.cpp

#include <iostream>
#include <string>
#include <vector>
#include <memory> // For std::make_unique
#include "../src/moves.h"        // For skolem_form
#include "../src/grammar.h"      // For parser functions
#include "../src/node.h"         // For node and related functions
#include "../src/context.h"      // For context_t
#include "../src/substitute.h"   // For substitute function and Substitution

// Function to perform a single skolemization test
bool test_skolem_form(const std::string& input, const std::string& expected_output, int test_number) {
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

    context_t context = context_t();

    // Perform skolemization
    // Assuming that parser_context_t has a member 'ctx_var' of type context_t
    node* skolemized_ast = skolem_form(context, deep_copy(ast_original));

    if (!skolemized_ast) {
        std::cerr << "Test #" << test_number << " failed: Skolemization returned nullptr.\n";
        delete ast_original;
        parser_destroy(ctx);
        return false;
    }

    // Parse the expected output formula
    node* ast_expected = nullptr;
    std::string expected_modified = expected_output;
    expected_modified.push_back('\n');

    mgr.input = expected_modified.c_str();
    mgr.pos = 0;

    parser_parse(ctx, &ast_expected);

    if (!ast_expected) {
        std::cerr << "Test #" << test_number << " failed: Syntax error in expected output near position " << mgr.pos << ":\n";
        std::cerr << "Expected output string: " << expected_output << std::endl;
        delete skolemized_ast;
        delete ast_original;
        parser_destroy(ctx);
        return false;
    }

    // Convert both ASTs to their string representations for comparison
    std::string skolemized_str = skolemized_ast->to_string(REPR);
    std::string expected_str = ast_expected->to_string(REPR);

    // Compare the skolemized formula with the expected formula
    bool pass = (skolemized_str == expected_str);

    if (!pass) {
        std::cerr << "Test #" << test_number << " failed:\n";
        std::cerr << "Input Formula:          [" << input << "]\n";
        std::cerr << "Expected Skolemized:    [" << expected_output << "]\n";
        std::cerr << "Actual Skolemized:      [" << skolemized_str << "]\n";
    }

    // Clean up memory
    delete ast_expected;
    delete skolemized_ast;
    delete ast_original;
    parser_destroy(ctx);
    
    return pass;
}

int main() {
    // Define test cases: pair of input formula and expected Skolemized formula
    struct TestCase {
        std::string input;
        std::string expected_output;
    };

    std::vector<TestCase> test_cases = {
        // Test 1: Simple existential quantifier with no universals
        {"\\exists x P(x)", "P(x())"},

        // Test 2: Existential quantifier with one universal
        {"\\forall y \\exists x P(x, y)", "P(x(y), y)"},

        // Test 3: Nested quantifiers
        {"\\forall y \\forall z \\exists x P(x, y, z)", "P(x(y, z), y, z)"},

        // Test 4: Multiple existential quantifiers
        {"\\forall y \\exists x \\exists w P(x, w, y)", "P(x(y), w(y), y)"},

        // Test 5: Existential quantifier with unused universal
        {"\\forall y \\forall z \\exists x P(x, y)", "P(x(y), y)"},

        // Test 6: No quantifiers
        {"P(x, y)", "P(x, y)"},

        // Test 7: Multiple quantifiers mixed
        {"\\forall y \\exists x \\forall z \\exists w P(x, y, z, w)", "P(x(y), y, z, w(y, z))"},

        // Test 8: Existential quantifier inside universal
        {"\\forall y \\exists x (P(x, y) \\vee Q(y))", "P(x(y), y) \\vee Q(y)"},

        // Test 9: Multiple universals before existential
        {"\\forall y \\forall z \\exists x \\exists w P(x, w, y, z)", "P(x(y, z), w(y, z), y, z)"},

        // Test 10: Existential quantifier with function arguments
        {"\\forall y \\exists x P(f(x), y)", "P(f(x(y)), y)"},

        // Additional Test Cases
        // Test 11: Multiple universals, some unused
        {"\\forall y \\forall z \\forall w \\exists x P(x, y)", "P(x(y), y)"},

        // Test 12: Nested existential quantifiers
        {"\\forall y \\exists x \\forall z \\exists w \\exists v Q(x, w, v, y, z)", "Q(x(y), w(y, z), v(y, z), y, z)"},

        // Test 13: Existential quantifier with multiple universals
        {"\\forall y \\forall z \\exists x P(x, y, z)", "P(x(y, z), y, z)"},

        // Test 14: Complex formula with multiple quantifiers
        {"\\forall y \\exists x \\forall z (P(x, y) \\implies Q(z))", "(P(x(y), y) \\implies Q(z))"}
    };

    std::cout << "Running tests..." << std::endl;

    bool all_tests_passed = true;
    for (size_t i = 0; i < test_cases.size(); ++i) {
        bool result = test_skolem_form(test_cases[i].input, test_cases[i].expected_output, i + 1);
        if (!result) {
            all_tests_passed = false;
        }
    }

    if (all_tests_passed) {
        std::cout << "All tests passed!" << std::endl;
        return 0;
    }
}

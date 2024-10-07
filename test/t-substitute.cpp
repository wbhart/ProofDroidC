// t-substitute.cpp

#include "../src/node.h"
#include "../src/substitute.h"
#include "../src/grammar.h"
#include <iostream>
#include <unordered_map>
#include <string>
#include <vector>
#include <optional>

// Function declarations
std::optional<Substitution> unify(node* node1, node* node2, Substitution& subst);
void print_substitution(const Substitution& subst);
bool run_test_case(const std::string& formula, const Substitution& substitution, const std::string& expected_formula);

// Function to parse a formula using the parser
node* parse_formula(const std::string& formula) {
    manager_t mgr;
    mgr.input = nullptr; // Initialize manager input
    mgr.pos = 0;

    parser_context_t *ctx = parser_create(&mgr);
    node* ast = nullptr;

    std::string modified_input = formula;  // Copy the formula string
    modified_input.push_back('\n');  // Add newline to the input, as per the example

    // Set the input buffer and reset position
    mgr.input = modified_input.c_str();
    mgr.pos = 0;

    // Parse the input
    parser_parse(ctx, &ast);

    if (!ast) {
        std::cerr << "Failed to parse formula: " << formula << "\n";
        parser_destroy(ctx);
        return nullptr;
    }

    parser_destroy(ctx);
    return ast;
}

node* parse_term(const std::string& formula) {
    node *f = parse_formula("P(" + formula + ")");
    return f->children[1];
}

// Function to print the substitution map
void print_substitution(const Substitution& subst) {
    for (const auto& [key, value] : subst) {
        std::cout << key << " -> " << value->to_string(OutputFormat::REPR) << "\n";
    }
}

// Function to run a single test case
bool run_test_case(const std::string& formula, const Substitution& substitution, const std::string& expected_formula) {
    // Parse the original formula
    node* parsed_formula = parse_formula(formula);
    if (!parsed_formula) {
        std::cerr << "Test failed: Unable to parse formula: " << formula << "\n";
        return false;
    }

    // Create a deep copy of the original formula to preserve it for comparison
    node* formula_copy = deep_copy(parsed_formula);
    if (!formula_copy) {
        std::cerr << "Test failed: Unable to deep copy the formula: " << formula << "\n";
        delete parsed_formula;
        return false;
    }

    // Apply the substitution to the formula copy
    node* substituted_formula = substitute(formula_copy, substitution);
    if (!substituted_formula) {
        std::cerr << "Test failed: Substitution returned nullptr for formula: " << formula << "\n";
        delete formula_copy;
        delete parsed_formula;
        return false;
    }

    // Parse the expected formula
    node* parsed_expected = parse_formula(expected_formula);
    if (!parsed_expected) {
        std::cerr << "Test failed: Unable to parse expected formula: " << expected_formula << "\n";
        delete substituted_formula;
        delete formula_copy;
        delete parsed_formula;
        return false;
    }

    // Generate REPR strings for comparison
    std::string repr_substituted = substituted_formula->to_string(OutputFormat::REPR);
    std::string repr_expected = parsed_expected->to_string(OutputFormat::REPR);

    // Compare the substituted formula with the expected formula
    bool pass = (repr_substituted == repr_expected);

    if (!pass) {
        std::cerr << "Test failed:\n";
        std::cerr << "Original Formula:    [" << formula << "]\n";
        std::cerr << "Substitution:\n";
        print_substitution(substitution);
        std::cerr << "Expected Formula:    [" << expected_formula << "]\n";
        std::cerr << "Substituted Formula: [" << repr_substituted << "]\n";
        std::cerr << "Expected Formula REPR:[" << repr_expected << "]\n";
    }

    // Clean up memory
    delete parsed_formula;
    delete substituted_formula;
    delete parsed_expected;

    return pass;
}

int main() {
    // Define test cases
    struct TestCase {
        std::string formula;
        Substitution substitution;
        std::string expected_formula;
    };

    std::vector<TestCase> test_cases = {
        // Test 1: Simple substitution
        {
            "P(x)",
            { {"x", parse_term("\\emptyset")} },
            "P(\\emptyset)"
        },
        // Test 2: Multiple substitutions
        {
            "P(x) \\wedge Q(y)",
            { {"x", parse_term("f(z)")}, {"y", parse_term("a")} },
            "P(f(z)) \\wedge Q(a)"
        },
        // Test 3: No substitution
        {
            "P(x) \\vee Q(y)",
            { }, // Empty substitution
            "P(x) \\vee Q(y)"
        },
        // Test 4: Substitution with function applications
        {
            "R(x, y)",
            { {"x", parse_term("g(z)")}, {"y", parse_term("h(w)")}},
            "R(g(z), h(w))"
        },
        // Test 5: Substitution with constants
        {
            "S(x) \\iff T(y)",
            { {"x", parse_term("\\emptyset")}, {"y", parse_term("b")} },
            "S(\\emptyset) \\iff T(b)"
        },
        // Test 6: Substitution with nested terms
        {
            "U(x) \\vee V(y)",
            { {"x", parse_term("f(g(z))")}, {"y", parse_term("h(k(l))")} },
            "U(f(g(z))) \\vee V(h(k(l)))"
        },
        // Test 7: Substitution with tuple terms
        {
            "W(x)",
            { {"x", parse_term("(a, b, c)")} },
            "W((a, b, c))"
        },
        // Test 8: Substitution where substitution variable is bound (should not substitute)
        {
            "\\forall x P(x, y)",
            { {"y", parse_term("a")} },
            "\\forall x P(x, a)"
        },
        // Test 9: Substitution in presence of multiple quantifiers
        {
            "\\forall x \\exists y (P(x) \\wedge Q(y) \\wedge R(z))",
            { {"z", parse_term("g(x)")}, {"w", parse_term("h")} }, // 'w' not in formula
            "\\forall x \\exists y (P(x) \\wedge Q(y) \\wedge R(g(x)))"
        },
        // Test 10: Substitution with terms containing variables to be substituted
        {
            "F(x)",
            { {"x", parse_term("G(y)")}, {"y", parse_term("H(z)")}},
            "F(G(y))" // Note: Only x is substituted with G(y), y remains free
        }
    };

    std::cout << "Running tests..." << std::endl;

    bool all_passed = true;
    for (size_t i = 0; i < test_cases.size(); ++i) {
        const auto& test = test_cases[i];
        bool result = run_test_case(test.formula, test.substitution, test.expected_formula);
        if (!result) {
            all_passed = false;
        }
    }

    if (all_passed) {
        std::cout << "All tests passed!" << std::endl;
        return 0;
    }
}


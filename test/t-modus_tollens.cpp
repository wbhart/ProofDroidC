// t-modus_tollens.cpp

#include "../src/node.h"
#include "../src/context.h"
#include "../src/moves.h"
#include "../src/grammar.h"
#include "../src/unify.h"
#include "../src/substitute.h"
#include <iostream>
#include <string>
#include <vector>
#include <optional>
#include <set>
#include <utility>
#include <algorithm>

// Structure representing a test case
struct TestCase {
    std::string implication;                  // Contrapositive implication formula (e.g., "\\neg B \\implies \\neg A")
    std::vector<std::string> unit_clauses;    // List of unit clause formulas (e.g., {"A"})
    std::string expected_result;              // Expected result formula after applying modus tollens (e.g., "B")
    bool expect_failure;                      // Indicates if the test is expected to fail
};

// Function to parse a formula using the parser
node* parse_formula(const std::string& formula) {
    manager_t mgr;
    mgr.input = nullptr; // Initialize manager input
    mgr.pos = 0;

    parser_context_t *ctx = parser_create(&mgr);
    if (ctx == nullptr) {
        std::cerr << "Failed to create parser context." << std::endl;
        return nullptr;
    }

    std::string modified_input = formula;
    modified_input.push_back('\n'); // Add newline to the input as required

    // Set the input buffer and reset position
    mgr.input = modified_input.c_str();
    mgr.pos = 0;

    // Parse the input
    node* ast = nullptr;
    parser_parse(ctx, &ast);

    if (!ast) {
        std::cerr << "Failed to parse formula: " << formula << "\n";
    }

    parser_destroy(ctx);
    return ast;
}

// Function to compare two nodes by their string representations
bool compare_nodes(const node* n1, const node* n2) {
    if (n1 == nullptr && n2 == nullptr) return true;
    if (n1 == nullptr || n2 == nullptr) return false;
    return n1->to_string(REPR) == n2->to_string(REPR);
}

// Function to run a single test case
bool run_test_case(int test_number, context_t& ctx_var, const TestCase& test) {
    // Parse the contrapositive implication
    node* parsed_implication = parse_formula(test.implication);

    if (parsed_implication == nullptr) {
        std::cerr << "Parsing error for implication in Test Case " << test_number << ". Skipping...\n";
        return false;
    }

    // Parse the unit clauses
    std::vector<node*> parsed_units;
    bool unit_parse_failed = false;
    for (const auto& unit_str : test.unit_clauses) {
        node* parsed_unit = parse_formula(unit_str);
        if (parsed_unit == nullptr) {
            std::cerr << "Parsing error for unit clause '" << unit_str << "' in Test Case " << test_number << ". Skipping...\n";
            unit_parse_failed = true;
            break;
        }
        parsed_units.push_back(parsed_unit);
    }

    if (unit_parse_failed) {
        // Clean up any parsed nodes
        delete parsed_implication;
        for (auto& unit : parsed_units) {
            delete unit;
        }
        return false;
    }

    // Apply modus tollens
    node* result = modus_tollens(ctx_var, parsed_implication, parsed_units);

    // Parse the expected result
    node* parsed_expected = parse_formula(test.expected_result);
    if (parsed_expected == nullptr && !test.expect_failure) {
        std::cerr << "Parsing error for expected result in Test Case " << test_number << ". Skipping...\n";
        // Clean up allocated nodes
        delete parsed_implication;
        for (auto& unit : parsed_units) {
            delete unit;
        }
        delete result;
        return false;
    }

    // Determine if the test passed
    bool passed = false;
    if (test.expect_failure) {
        // If a failure was expected, pass the test if result is nullptr
        passed = (result == nullptr);
    }
    else {
        // If no failure was expected, compare the result with the expected formula
        passed = compare_nodes(result, parsed_expected);
    }

    if (!passed) {
        std::cout << "Test Case " << test_number << " FAILED.\n";
        std::cout << "Implication: " << (parsed_implication ? parsed_implication->to_string(REPR) : "null") << "\n";
        std::cout << "Unit Clauses: ";
        for (const auto& unit : parsed_units) {
            std::cout << (unit ? unit->to_string(REPR) : "null") << " ";
        }
        std::cout << "\nExpected Result: " << (parsed_expected ? parsed_expected->to_string(REPR) : "null") << "\n";
        std::cout << "Actual Result: " << (result ? result->to_string(REPR) : "null") << "\n";
    }

    // Clean up allocated nodes
    delete parsed_implication;
    for (auto& unit : parsed_units) {
        delete unit;
    }
    delete parsed_expected;
    delete result;

    return passed;
}

int main() {
    std::cout << "Running tests...\n";

    // Initialize a context for variable renaming
    context_t ctx_var;

    // Define test cases with contrapositives
    std::vector<TestCase> test_cases = {
        // Test Case 1: Single implication
        {
            "\\neg Q(a) \\implies \\neg P(a)", // Contrapositive of "P(a) \\implies Q(a)"
            {"P(a)"},
            "Q(a)",
            false
        },
        // Test Case 2: Multiple conjuncts and De Morgan's applied
        {
            "\\neg R(x) \\implies (\\neg P(x) \\vee \\neg Q(y))", // Contrapositive of "P(x) \\wedge Q(y) \\implies R(x)"
            {"P(a)", "Q(b)"},
            "R(a)",
            false
        },
        // Test Case 3: Multiple conjuncts with variables and De Morgan's applied
        {
            "\\neg R(z) \\implies (\\neg P(x) \\vee \\neg Q(y))", // Contrapositive of "P(x) \\wedge Q(y) \\implies R(z)"
            {"P(a)", "Q(b)"},
            "R(z)",
            false
        },
        // Test Case 4: Multiple conjuncts with functions and De Morgan's applied
        {
            "\\neg R(h(z)) \\implies (\\neg P(f(x)) \\vee \\neg Q(g(y)))", // Contrapositive of "P(f(x)) \\wedge Q(g(y)) \\implies R(h(z))"
            {"P(f(a))", "Q(g(b))"},
            "R(h(z))",
            false
        },
        // Test Case 5: Single implication with variable renaming
        {
            "\\neg Q(x) \\implies \\neg P(x)", // Contrapositive of "P(x) \\implies Q(x)"
            {"P(y)"},
            "Q(y)",
            false
        },
        // Test Case 6: Nested conjunctions and De Morgan's applied
        {
            "\\neg S(x) \\implies (\\neg P(x) \\vee \\neg Q(x) \\vee \\neg R(x))", // Contrapositive of "P(x) \\wedge Q(x) \\wedge R(x) \\implies S(x)"
            {"P(x)", "Q(x)", "R(x)"},
            "S(x)",
            false
        },
        // Test Case 7: Implication with nested functions and De Morgan's applied
        {
            "\\neg R(h(z)) \\implies (\\neg P(f(x)) \\vee \\neg Q(g(y)))", // Contrapositive of "P(f(x)) \\wedge Q(g(y)) \\implies R(h(z))"
            {"P(f(a))", "Q(g(b))"},
            "R(h(z))",
            false
        },
        // Test Case 8: Implication with multiple variables and De Morgan's applied
        {
            "\\neg R(a, c) \\implies (\\neg P(a, b) \\vee \\neg Q(b, c))", // Contrapositive of "P(x, y) \\wedge Q(y, z) \\implies R(x, z)"
            {"P(a, b)", "Q(b, c)"},
            "R(a, c)",
            false
        }
    };

    int total_tests = test_cases.size();
    int passed_tests = 0;

    // Execute each test case
    for (size_t i = 0; i < test_cases.size(); ++i) {
        bool passed = run_test_case(static_cast<int>(i + 1), ctx_var, test_cases[i]);
        if (passed) {
            passed_tests++;
        }
    }

    if (passed_tests == total_tests) {
        std::cout << "All tests passed!\n";
        return 0;
    }
}

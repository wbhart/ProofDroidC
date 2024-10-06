#include "../src/node.h"
#include "../src/symbol_enum.h"
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
bool run_test_case(const std::string& formula1, const std::string& formula2, const Substitution& expected_subst);


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

// Function to print the substitutions
void print_substitution(const Substitution& subst) {
    for (const auto& [key, value] : subst) {
        std::cout << key << " -> " << value->to_string(OutputFormat::REPR) << "\n";
    }
}

// Function to run a single test case, returns true if the test passes, false otherwise
bool run_test_case(const std::string& formula1, const std::string& formula2, const Substitution& expected_subst) {
    node* parsed_formula1 = parse_formula(formula1);
    node* parsed_formula2 = parse_formula(formula2);

    if (parsed_formula1 == nullptr || parsed_formula2 == nullptr) {
        std::cerr << "Parsing error occurred for formulas: " << formula1 << " or " << formula2 << "\n";
        return false;
    }

    Substitution subst;
    auto result = unify(parsed_formula1, parsed_formula2, subst);

    bool passed = true;
    if (result.has_value()) {
        // Verify against expected substitution
        for (const auto& [key, value] : expected_subst) {
            if (result.value().find(key) == result.value().end() || result.value().at(key)->to_string() != value->to_string()) {
                passed = false;
                break;
            }
        }
    } else {
        if (!expected_subst.empty()) {
            passed = false;
        }
    }

    if (!passed) {
        std::cout << "Test failed for: " << formula1 << " and " << formula2 << "\n";
        std::cout << "Expected substitution:\n";
        print_substitution(expected_subst);
        std::cout << "Actual substitution:\n";
        if (result.has_value()) {
            print_substitution(result.value());
        } else {
            std::cout << "Unification failed.\n";
        }
    }

    // Clean up memory
    delete parsed_formula1;
    delete parsed_formula2;
    
    for (const auto& [key, value] : expected_subst) {
        delete value;
    }

    return passed;
}

int main() {
    // Test cases: each contains a pair of formulas and the expected substitution
    struct TestCase {
        std::string formula1;
        std::string formula2;
        Substitution expected_subst;
    };

    std::vector<TestCase> test_cases = {
        {"P(x)", "P(\\emptyset)", { {"x", parse_term("\\emptyset")} }},
        {"P(x) = T", "P(\\emptyset) = T", { {"x", parse_term("\\emptyset")} }},
        {"f(x, y)", "f(\\emptyset, \\emptyset)", { {"x", parse_term("\\emptyset")}, {"y", parse_term("\\emptyset")} }},
        {"\\forall x P(x)", "\\forall y P(y)", {}}, // Expected empty substitution since they match structurally
        {"P(f(x))", "P(f(\\emptyset))", { {"x", parse_term("\\emptyset")} }},
        {"P(f(x, \\emptyset))", "P(f(g(y), z))", { {"x", parse_term("g(y)")}, {"z", parse_term("\\emptyset")} }},
        {"P(x)", "P((y, z))", { {"x", parse_term("(y, z)")} }}
    };

    std::cout << "Running tests..." << std::endl;

    bool all_passed = true;
    for (const auto& test : test_cases) {
        if (!run_test_case(test.formula1, test.formula2, test.expected_subst)) {
            all_passed = false;
        }
    }

    if (all_passed) {
        std::cout << "All tests passed!\n";
    }

    return all_passed ? 0 : 1;
}

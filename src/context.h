// context.h

#ifndef CONTEXT_H
#define CONTEXT_H

#include "node.h"
#include <unordered_map>
#include <string>
#include <iostream>
#include <vector>
#include <set>
#include <utility>
#include <unordered_set>
#include <algorithm>

// Enumeration representing reasons for justifications in tableau lines
enum class Reason {
    ModusPonens,
    ModusTollens,
    Target,
    Hypothesis,
    ConjunctiveIdempotence,
    DisjunctiveIdempotence,
    SplitConjunction,
    SplitDisjunctiveImplication,
    SplitConjunctiveImplication,
    NegatedImplication,
    ConditionalPremise
};

// Represents a single line in the tableau
class tabline_t {
public:
    bool target = false;                           // Indicates if this line is a target
    bool active = true;                            // Indicates if this line is active
    std::vector<int> proved;                       // Indices of targets this is proved for
    std::vector<int> assumptions;                  // Indices of assumptions
    std::vector<int> restrictions;                 // Indices of restrictions
    std::pair<Reason, std::vector<int>> justification; // How this was proved and from which lines
    node* formula = nullptr;                       // Pointer to the associated formula
    node* negation = nullptr;                      // Pointer to the negation of the formula

    tabline_t(node* form) : target(false), formula(form) {}
    tabline_t(node* form, node* neg) : target(true), formula(form), negation(neg) {}
};

class context_t {
public:
    context_t();

    // Whether free variables have already been made into parameters
    bool parameterized = false;

    // Retrieves and increments the next available index for a variable
    int get_next_index(const std::string& var_name);

    // Retrieves the current index of a variable without incrementing
    // Returns -1 if the variable does not exist
    int get_current_index(const std::string& var_name) const;

    // Resets the index of a variable to zero
    // Initializes the variable if it does not exist
    void reset_index(const std::string& var_name);

    // Checks if a variable exists in the context
    bool has_variable(const std::string& var_name) const;

    // Prints the current state of variable indices for debugging
    void print_context() const;

    // Array of tableau lines
    std::vector<tabline_t> tableau;

private:
    // Maps variable base names to their latest index
    std::unordered_map<std::string, int> var_indices;
};

// Generates renaming pairs for common variables based on the context
std::vector<std::pair<std::string, std::string>> vars_rename_list(context_t& ctx, const std::set<std::string>& common_vars);

// Applies renaming pairs to a formula's AST to avoid variable capture
void rename_vars(node* root, const std::vector<std::pair<std::string, std::string>>& renaming_pairs);

void print_reason(const context_t& context, int index);

// Combine a pair of restrictions into a single restriction
std::vector<int> combine_restrictions(const std::vector<int>& res1, const std::vector<int>& res2);

// Check if restrictions are compatible
bool check_restrictions(const std::vector<int>& res1, const std::vector<int>& res2);

// Combine a pair of assumptions into a single set of assumptions
std::vector<int> combine_assumptions(const std::vector<int>& assm1, const std::vector<int>& assm2);

// Check is assumptions are compatible
bool check_assumptions(const std::vector<int>& assm1, const std::vector<int>& assm2);

#endif // CONTEXT_H


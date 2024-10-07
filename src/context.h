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

// Enumeration representing reasons for justifications in tableau lines
enum class Reason {
    ModusPonens,
    Target,
    Hypothesis
};

// Represents a single line in the tableau
class tabline_t {
public:
    bool target = false;                           // Indicates if this line is a target
    bool active = true;                            // Indicates if this line is active
    std::vector<int> proved;                       // Indices of targets this is proved for
    std::vector<int> assumptions;                  // Indices of assumptions
    std::pair<Reason, std::vector<int>> justification; // How this was proved and from which lines
    node* formula = nullptr;                       // Pointer to the associated formula
    node* negation = nullptr;                      // Pointer to the negation of the formula

    tabline_t(node* form) : formula(form) {}
};

class context_t {
public:
    context_t();

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

#endif // CONTEXT_H


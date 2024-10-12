// context.cpp

#include "context.h"

// Constructor: Initializes the context (empty var_indices)
context_t::context_t() : var_indices() {}

// Retrieves and increments the next index for the given variable
int context_t::get_next_index(const std::string& var_name) {
    // If the variable is not present, initialize its index to 0
    if (var_indices.find(var_name) == var_indices.end()) {
        var_indices[var_name] = 0;
    } else {
        // Increment the index
        var_indices[var_name] += 1;
    }
    return var_indices[var_name];
}

// Retrieves the current index for the given variable without incrementing
int context_t::get_current_index(const std::string& var_name) const {
    auto it = var_indices.find(var_name);
    if (it != var_indices.end()) {
        return it->second;
    }
    return -1; // Indicates that the variable was not found
}

// Resets the index for the given variable to 0
void context_t::reset_index(const std::string& var_name) {
    var_indices[var_name] = 0;
}

// Checks if the variable exists in the context
bool context_t::has_variable(const std::string& var_name) const {
    return var_indices.find(var_name) != var_indices.end();
}

// Prints the current state of var_indices for debugging
void context_t::print_context() const {
    std::cout << "Current Context State:\n";
    for (const auto& pair : var_indices) {
        std::cout << "Variable: " << pair.first << ", Latest Index: " << pair.second << "\n";
    }
    std::cout << "--------------------------\n";
}

std::vector<std::pair<std::string, std::string>> vars_rename_list(context_t& ctx, const std::set<std::string>& common_vars) {
    std::vector<std::pair<std::string, std::string>> renaming_pairs;

    for (const auto& var : common_vars) {
        std::string base = remove_subscript(var);
        std::string new_var;

        if (base != var) {
            // Variable has a subscript, e.g., "x_1" -> "x"
            if (ctx.has_variable(base)) {
                // Base variable exists in context; get next index
                int new_index = ctx.get_next_index(base); // Increments and retrieves
                new_var = append_subscript(base, new_index); // Use append_index
            } else {
                // Base variable does not exist; initialize it with index 0
                new_var = append_subscript(base, 0); // Use append_index
                ctx.reset_index(base); // Sets "x" -> 0
            }
        } else {
            // Variable does not have a subscript, e.g., "x"
            if (ctx.has_variable(base)) {
                // Base variable exists in context; get next index
                int new_index = ctx.get_next_index(base); // Increments and retrieves
                new_var = append_subscript(base, new_index); // Use append_index
            } else {
                // Base variable does not exist; initialize it with index 0
                new_var = append_subscript(base, 0); // Use append_index
                ctx.reset_index(base); // Sets "x" -> 0
            }
        }

        // Add the renaming pair to the list
        renaming_pairs.emplace_back(var, new_var);
    }

    return renaming_pairs;
}

// Function to print the reason for a given tableau line index
// context.cpp

#include "context.h"
#include <iostream>
#include <vector>

// Function to print the reason for a given tableau line index
void print_reason(const context_t& context, int index) {
    // Check if the index is within the bounds of the tableau
    if (index < 0 || static_cast<size_t>(index) >= context.tableau.size()) {
        std::cerr << "Error: Line index " << index << " is out of bounds." << std::endl;
        return;
    }

    // Retrieve the specified tableau line
    const tabline_t& tabline = context.tableau[index];

    // Extract the justification reason and associated line numbers
    Reason reason = tabline.justification.first;
    const std::vector<int>& associated_lines = tabline.justification.second;

    // Determine the output based on the reason
    switch (reason) {
        case Reason::Target:
            std::cout << "Tar";
            break;

        case Reason::Hypothesis:
            std::cout << "Hyp";
            break;

        case Reason::ModusPonens: {
            std::cout << "MP[";
            for (size_t i = 0; i < associated_lines.size(); ++i) {
                std::cout << associated_lines[i] + 1;
                if (i != associated_lines.size() - 1) {
                    std::cout << ", ";
                }
            }
            std::cout << "]";
            break;
        }

        case Reason::ModusTollens: {
            std::cout << "MT[";
            for (size_t i = 0; i < associated_lines.size(); ++i) {
                std::cout << associated_lines[i] + 1;
                if (i != associated_lines.size() - 1) {
                    std::cout << ", ";
                }
            }
            std::cout << "]";
            break;
        }

        case Reason::SplitConjunction: {
            std::cout << "SC[";
            std::cout << associated_lines[0] + 1;
            std::cout << "]";
            break;
        }

        case Reason::SplitConjunctiveImplication: {
            std::cout << "SCI[";
            std::cout << associated_lines[0] + 1;
            std::cout << "]";
            break;
        }

        case Reason::SplitDisjunctiveImplication: {
            std::cout << "SDI[";
            std::cout << associated_lines[0] + 1;
            std::cout << "]";
            break;
        }

        default:
            std::cout << "Unknown";
            break;
    }
}

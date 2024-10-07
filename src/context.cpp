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


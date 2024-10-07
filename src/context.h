// context.h

#ifndef CONTEXT_H
#define CONTEXT_H

#include "node.h"
#include <unordered_map>
#include <string>
#include <iostream>
#include <vector>
#include <set>

class context_t {
public:
    // Constructor
    context_t();

    /**
     * @brief Retrieves the next available index for the given variable name.
     *        If the variable does not exist in the context, it initializes its index to 0.
     * 
     * @param var_name The name of the variable.
     * @return int The next index for the variable.
     */
    int get_next_index(const std::string& var_name);

    /**
     * @brief Retrieves the current index for the given variable name without incrementing.
     *        Returns -1 if the variable does not exist.
     * 
     * @param var_name The name of the variable.
     * @return int The current index, or -1 if not found.
     */
    int get_current_index(const std::string& var_name) const;

    /**
     * @brief Resets the index for the given variable name to zero.
     *        If the variable does not exist, it initializes its index to 0.
     * 
     * @param var_name The name of the variable.
     */
    void reset_index(const std::string& var_name);

    /**
     * @brief Checks if a variable exists in the context.
     * 
     * @param var_name The name of the variable.
     * @return true If the variable exists.
     * @return false Otherwise.
     */
    bool has_variable(const std::string& var_name) const;

    /**
     * @brief Prints the current state of var_indices for debugging purposes.
     */
    void print_context() const;

private:
    // Dictionary mapping variable names to their latest index
    std::unordered_map<std::string, int> var_indices;
};

std::vector<std::pair<std::string, std::string>> vars_rename_list(context_t& ctx, const std::set<std::string>& common_vars);

#endif // CONTEXT_H


// substitute.cpp

#include "substitute.h"
#include "node.h"
#include <iostream>

// Function to apply substitution to a formula node
node* substitute(node* formula, const Substitution& subst) {
    // Check if the current node is a variable that needs to be substituted
    if (formula->type == VARIABLE) {
        std::string var_name = formula->name();
        auto it = subst.find(var_name);
        if (it != subst.end()) {
            // Replace the entire node with the substitution
            return deep_copy(it->second);
        }
    }

    // For all other node types, apply substitution to their children
    // Iterate through the children and apply substitution recursively
    for (size_t i = 0; i < formula->children.size(); ++i) {
        node* child = formula->children[i];
        node* substituted_child = substitute(child, subst);
        if (substituted_child != child) {
            formula->children[i] = substituted_child;
            delete child;
        }
    }

    // Functions are not substituted for now

    // Return the modified formula node
    return deep_copy(formula);
}


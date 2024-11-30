#include "symbol_enum.h"
#include "node.h"
#include "unify.h"
#include "substitute.h"
#include <iostream>
#include <unordered_map>
#include <optional>
#include <string>
#include <vector>

// Function to check if a variable occurs in a node (occurs check)
bool occurs_check(node* var, node* node) {
    if (node->type == VARIABLE && node->name() == var->name()) {
        return true;
    }

    for (auto& child : node->children) {
        if (occurs_check(var, child)) {
            return true;
        }
    }
    return false;
}

// Function to unify a variable with a node
std::optional<Substitution> unify_variable(node* var, node* term, Substitution& subst, bool smgu) {
    std::string var_name = var->name();

    // If the variable is already bound in the substitution map, unify the mapped value with the term
    if (subst.find(var_name) != subst.end()) {
        return unify(subst[var_name], term, subst, smgu);
    }

    // If the term is already a variable mapped in the substitution, unify them
    if (term->is_variable()) {
        std::string term_name = term->name();
        if (subst.find(term_name) != subst.end()) {
            return unify(var, subst[term_name], subst, smgu);
        }
    }

    // Variable unifies with itself
    if (term->type == VARIABLE && term->name() == var->name()) {
        return subst;
    }
    
    // If the variable occurs in the term (occurs check), fail to avoid infinite loops
    if (occurs_check(var, term)) {
        return std::nullopt;
    }

    // Add the substitution of the variable to the term only if the term is a valid type (variable or constant)
    if (term->type == VARIABLE || term->type == CONSTANT ||
        term->type == APPLICATION || term->type == TUPLE) {
            subst[var_name] = term;
    } else {
        return std::nullopt;
    }
    
    return subst;
}

// Function to unify two nodes
std::optional<Substitution> unify(node* node1, node* node2, Substitution& subst, bool smgu) {
    // If node1 is a variable, ensure it is a true variable before trying to unify
    if (node1->is_free_variable() && (smgu || !node1->is_shared_variable())) {
        return unify_variable(node1, node2, subst, smgu);
    }

    // If node2 is a variable, ensure it is a true variable before trying to unify
    if (node2->is_free_variable() && (smgu || !node2->is_shared_variable())) {
        return unify_variable(node2, node1, subst, smgu);
    }

    // If both nodes are PARAMETERS
    if (node1->type == VARIABLE && node2->type == VARIABLE)
    {
        if (node1->vdata->var_kind != node2->vdata->var_kind) {
            return std::nullopt; // Not the same kind of variable
        }

        if (node1->vdata->name != node2->vdata->name) {
            return std::nullopt; // Not the same name
        }

        return subst;
    }
    
    // If both nodes are applications, check if they can be unified
    if (node1->type == APPLICATION && node2->type == APPLICATION) {
        // First children are the symbols, which should be the same, for now
        if (node1->children[0]->type != node2->children[0]->type) {
            return std::nullopt; // We don't allow unification with functions for now
        }
        
        switch (node1->children[0]->type) {
        case VARIABLE:
            if (node1->children[0]->vdata->var_kind != node2->children[0]->vdata->var_kind ||
                node1->children[0]->name() != node2->children[0]->name()) {
                return std::nullopt; // Functions/predicates must have the same name
            }
            break;
        case BINARY_OP:
        case UNARY_OP:
        case BINARY_PRED:
        case UNARY_PRED:
            if (node1->children[0]->symbol != node2->children[0]->symbol) {
                return std::nullopt; // Functions must have the same name
            }
            break;
        default:
            return std::nullopt; // Not dealt with currently
        }

        // Check if they have the same number of arguments
        if (node1->children.size() != node2->children.size()) {
            return std::nullopt; // Different arities
        }

        // Unify the arguments of the applications
        for (size_t i = 1; i < node1->children.size(); ++i) {
            auto result = unify(node1->children[i], node2->children[i], subst, smgu);
            if (!result.has_value()) {
                return std::nullopt;
            }
        }
        return subst;
    }

    // If both nodes are tuples, check if they can be unified
    if (node1->type == TUPLE && node2->type == TUPLE) {
        // Check if they have the same number of elements
        if (node1->children.size() != node2->children.size()) {
            return std::nullopt; // Different arities
        }

        // Unify the elements of the tuples
        for (size_t i = 0; i < node1->children.size(); ++i) {
            auto result = unify(node1->children[i], node2->children[i], subst, smgu);
            if (!result.has_value()) {
                return std::nullopt;
            }
        }
        return subst;
    }

    // If both nodes are constants, check if they can be unified
    if (node1->type == CONSTANT && node2->type == CONSTANT) {
        // Check if they have the same symbol
        if (node1->symbol == node2->symbol) {
            return subst;
        } else {
            return std::nullopt; // Different constants
        }
    }

    // If both nodes are logical unary operations, check if they can be unified
    if (node1->type == LOGICAL_UNARY && node2->type == LOGICAL_UNARY) {
        // Both must be SYMBOL_NOT to unify
        if (node1->symbol == node2->symbol) {
            return unify(node1->children[0], node2->children[0], subst, smgu);
        } else {
            return std::nullopt; // Different logical unary operations
        }
    }

    // If both nodes are logical binary operations, check if they can be unified
    if (node1->type == LOGICAL_BINARY && node2->type == LOGICAL_BINARY) {
        // The symbols must match (IFF, IMPLIES, AND, OR)
        if (node1->symbol == node2->symbol) {
            // Unify both left and right children
            auto left_result = unify(node1->children[0], node2->children[0], subst, smgu);
            if (!left_result.has_value()) {
                return std::nullopt;
            }
            auto right_result = unify(node1->children[1], node2->children[1], subst, smgu);
            if (!right_result.has_value()) {
                return std::nullopt;
            }
            return subst;
        } else {
            return std::nullopt; // Different logical binary operations
        }
    }

    // If both nodes are quantifiers, check if they can be unified
    if (node1->type == QUANTIFIER && node2->type == QUANTIFIER) {
        // The symbols must match (FORALL or EXISTS)
        if (node1->symbol == node2->symbol &&
            (node1->symbol == SYMBOL_FORALL || node1->symbol == SYMBOL_EXISTS)) {
            // Create a local substitution for bound variables
            Substitution local_subst = subst;

            // Unify the bound variables
            node* bound_var1 = node1->children[0];
            node* bound_var2 = node2->children[0];

            auto bound_result = unify_variable(bound_var1, bound_var2, local_subst, smgu);
            if (!bound_result.has_value()) {
                return std::nullopt; // Bound variables must match or unification failed
            }

            // Apply the local substitution to the inner formulas and unify them
            node* inner_formula1 = node1->children[1];
            node* inner_formula2 = node2->children[1];
            auto inner_result = unify(inner_formula1, inner_formula2, local_subst, smgu);
            if (!inner_result.has_value()) {
                return std::nullopt; // Inner formulas cannot be unified
            }

            // If successful, update the global substitution
            subst = local_subst;
            return subst;
        } else {
            return std::nullopt; // Different quantifiers
        }
    }

    // If the nodes cannot be unified, return nullopt
    return std::nullopt;
}
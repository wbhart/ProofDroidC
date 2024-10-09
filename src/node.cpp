// node.cpp

#include "node.h"
#include <stdexcept>
#include <iostream>

variable_data* deep_copy(variable_data* vdata) {
    return new variable_data{vdata->var_kind, vdata->bound, vdata->arity, vdata->name};
}

// Helper function to deep copy a node
node* deep_copy(const node* n) {
    std::vector<node*> copied_children;
    for (const auto& child : n->children) {
        copied_children.push_back(deep_copy(child));
    }

    node* copied_node = nullptr;

    switch (n->type) {
        case VARIABLE:
            copied_node = new node(VARIABLE, n->name());
            copied_node->vdata = deep_copy(n->vdata);
            break;
        case CONSTANT:
            copied_node = new node(CONSTANT, n->symbol);
            break;
        case QUANTIFIER:
            copied_node = new node(QUANTIFIER, n->symbol, copied_children);
            break;
        case LOGICAL_UNARY:
            copied_node = new node(LOGICAL_UNARY, n->symbol, copied_children);
            break;
        case LOGICAL_BINARY:
            copied_node = new node(LOGICAL_BINARY, n->symbol, copied_children);
            break;
        case UNARY_OP:
            copied_node = new node(UNARY_OP, n->symbol, copied_children);
            break;
        case BINARY_OP:
            copied_node = new node(BINARY_OP, n->symbol, copied_children);
            break;
        case UNARY_PRED:
            copied_node = new node(UNARY_PRED, n->symbol, copied_children);
            break;
        case BINARY_PRED:
            copied_node = new node(BINARY_PRED, n->symbol, copied_children);
            break;
        case APPLICATION:
            copied_node = new node(APPLICATION, n->symbol, copied_children);
            break;
        case TUPLE:
            copied_node = new node(TUPLE, copied_children);
            break;
        default:
            throw std::logic_error("Unsupported node type for deep copy");
    }

    return copied_node;
}

// Helper function to create a negation node
node* create_negation(node* child) {
    std::vector<node*> children = { child };
    return new node(LOGICAL_UNARY, SYMBOL_NOT, children);
}

// The negate_node function implementation
node* negate_node(node* n) {
    if (n == nullptr) {
        throw std::invalid_argument("Cannot negate a null node");
    }

    switch (n->type) {
        case UNARY_PRED:
        case BINARY_PRED: {
            // Create a NOT node wrapping the predicate
            return create_negation(n);
        }

        case LOGICAL_UNARY:
            if (n->symbol == SYMBOL_NOT) {
                // Eliminate double negation: ¬¬φ ≡ φ
                // Assume n->children[0] is φ
                node* phi = n->children[0];
                // Clear the children to prevent deletion when n is deleted
                n->children.clear();
                // Delete the NOT node
                delete n;
                // Return φ as the negated result
                return phi;
            } else {
                // For other unary logical operators, wrap with NOT
                return create_negation(n);
            }

        case LOGICAL_BINARY: {
            // Apply De Morgan's laws and other logical negations
            if (n->children.size() != 2) {
                throw std::logic_error("Logical binary node must have exactly two children");
            }

            if (n->symbol == SYMBOL_AND) {
                // ¬(φ ∧ ψ) ≡ ¬φ ∨ ¬ψ
                node* left_neg = negate_node(n->children[0]);
                node* right_neg = negate_node(n->children[1]);
                std::vector<node*> children;
                children.push_back(left_neg);
                children.push_back(right_neg);
                return new node(LOGICAL_BINARY, SYMBOL_OR, children);
            }
            else if (n->symbol == SYMBOL_OR) {
                // ¬(φ ∨ ψ) ≡ ¬φ ∧ ¬ψ
                node* left_neg = negate_node(n->children[0]);
                node* right_neg = negate_node(n->children[1]);
                std::vector<node*> children;
                children.push_back(left_neg);
                children.push_back(right_neg);
                return new node(LOGICAL_BINARY, SYMBOL_AND, children);
            }
            else if (n->symbol == SYMBOL_IMPLIES) {
                // ¬(φ → ψ) ≡ φ ∧ ¬ψ
                node* phi = n->children[0];
                node* psi = n->children[1];
                node* negated_psi = negate_node(psi);
                // Clear children to prevent deletion
                n->children.clear();
                // Create AND node
                std::vector<node*> children;
                children.push_back(phi);
                children.push_back(negated_psi);
                return new node(LOGICAL_BINARY, SYMBOL_AND, children);
            }
            else if (n->symbol == SYMBOL_IFF) {
                // ¬(φ ↔ ψ) ≡ (φ ∧ ¬ψ) ∨ (¬φ ∧ ψ)
                node* phi = n->children[0];
                node* psi = n->children[1];
                node* neg_phi = negate_node(deep_copy(phi));
                node* neg_psi = negate_node(deep_copy(psi));
                // Clear children to prevent deletion
                n->children.clear();
                // Create (φ ∧ ¬ψ)
                std::vector<node*> left_children;
                left_children.push_back(phi);
                left_children.push_back(neg_psi);
                node* left_clause = new node(LOGICAL_BINARY, SYMBOL_AND, left_children);
                // Create (¬φ ∧ ψ)
                std::vector<node*> right_children;
                right_children.push_back(neg_phi);
                right_children.push_back(psi);
                node* right_clause = new node(LOGICAL_BINARY, SYMBOL_AND, right_children);
                // Create OR node
                std::vector<node*> children;
                children.push_back(left_clause);
                children.push_back(right_clause);
                return new node(LOGICAL_BINARY, SYMBOL_OR, children);
            }
            else {
                // For other binary operators, wrap with NOT
                return create_negation(n);
            }
        }

        case QUANTIFIER: {
            // Negate quantifiers: ¬∀x φ ≡ ∃x ¬φ and ¬∃x φ ≡ ∀x ¬φ
            symbol_enum new_quantifier = (n->symbol == SYMBOL_FORALL) ? SYMBOL_EXISTS : SYMBOL_FORALL;

            if (n->children.size() != 2) {
                throw std::logic_error("Quantifier node must have exactly two children");
            }

            node* variable = n->children[0];
            node* formula = n->children[1];

            // Clear children to prevent deletion
            n->children.clear();

            // Negate the formula
            node* negated_formula = negate_node(formula);

            return new node(QUANTIFIER, new_quantifier, { variable, negated_formula });
        }

        case APPLICATION: {
            if (n->children[0]->is_predicate()) {
                // Negate the APPLICATION node by wrapping it with NOT
                return create_negation(n);
            } else {
                // For other APPLICATION nodes, do not allow negation
                throw std::logic_error("Cannot negate an APPLICATION node unless its first child is a PREDICATE");
            }
        }

        case CONSTANT: {
            if (n->symbol == SYMBOL_TOP)
                return new node(CONSTANT, SYMBOL_BOT);
            else if (n->symbol == SYMBOL_BOT)
                return new node(CONSTANT, SYMBOL_TOP);
            else
                throw std::logic_error("Cannot negate a term. Only predicates and logical formulas can be negated.");
        }

        case UNARY_OP:
        case BINARY_OP:
        case VARIABLE:
        case TUPLE:
            // For all term node types, do not negate and throw an exception
            throw std::logic_error("Cannot negate a term. Only predicates and logical formulas can be negated.");

        default:
            throw std::logic_error("Unsupported node type for negation");
    }
}

void bind_var(node* current, const std::string& var_name) {
    // Check if the current node is a VARIABLE with the given name
    if (current->type == VARIABLE && current->name() == var_name) {
        current->vdata->bound = true;
    }

    // Recursively traverse all child nodes
    for (auto& child : current->children) {
        bind_var(child, var_name);
    }
}

void unbind_var(node* current, const std::string& var_name) {
    // Check if the current node is a VARIABLE with the given name
    if (current->type == VARIABLE && current->name() == var_name) {
        current->vdata->bound = false;
    }

    // Recursively traverse all child nodes
    for (auto& child : current->children) {
        unbind_var(child, var_name);
    }
}

void vars_used(std::set<std::string>& variables, const node* root) {
    // If the current node is a VARIABLE, add its name to the set
    if (root->type == VARIABLE) {
        variables.insert(root->name());
    }

    // Recursively traverse all child nodes
    for (const auto& child : root->children) {
        vars_used(variables, child);
    }
}

// Function to find common variables between two formulas
std::set<std::string> find_common_variables(const node* formula1, const node* formula2) {
    std::set<std::string> vars1;
    std::set<std::string> vars2;

    // Collect variables from both formulas
    vars_used(vars1, formula1);
    vars_used(vars2, formula2);

    // For debugging: Print collected variables
    std::cout << "Variables in Formula 1:\n";
    for (const auto& var : vars1) {
        std::cout << var << " ";
    }
    std::cout << "\nVariables in Formula 2:\n";
    for (const auto& var : vars2) {
        std::cout << var << " ";
    }
    std::cout << "\n";

    // Find the intersection of vars1 and vars2
    std::set<std::string> common_vars;
    std::set_intersection(
        vars1.begin(), vars1.end(),
        vars2.begin(), vars2.end(),
        std::inserter(common_vars, common_vars.begin())
    );

    return common_vars;
}

std::string remove_subscript(const std::string& var_name) {
    size_t pos = var_name.rfind('_');
    if (pos == std::string::npos) {
        // No underscore found; return the original name
        return var_name;
    }

    // Extract the substring after the last '_'
    std::string suffix = var_name.substr(pos + 1);

    // Check if the suffix is entirely composed of digits
    bool all_digits = !suffix.empty() && std::all_of(suffix.begin(), suffix.end(), ::isdigit);

    if (all_digits) {
        // Subscript detected; remove it
        return var_name.substr(0, pos);
    } else {
        // '_' is part of the base name; do not remove
        return var_name;
    }
}

std::string append_subscript(const std::string& base, int index) {
    return base + "_" + std::to_string(index);
}

int get_subscript(const std::string& var_name) {
    size_t pos = var_name.rfind('_');
    if (pos == std::string::npos) {
        return -1; // No subscript
    }

    std::string suffix = var_name.substr(pos + 1);
    if (std::all_of(suffix.begin(), suffix.end(), ::isdigit)) {
        return std::stoi(suffix);
    }
    return -1; // Not a valid subscript
}

std::string append_unicode_subscript(const std::string& base, int index) {
    // Append Unicode subscript for 0-9
    if (index < 0 || index > 9) {
        // If index is not between 0-9, fall back to regular subscript
        return base + "_" + std::to_string(index);
    }
    
    std::string subscript;
    subscript += static_cast<char>(0xE2);
    subscript += static_cast<char>(0x82);
    subscript += static_cast<char>(0x80 + index);
    return base + subscript;
}

// Implementing rename_vars
void rename_vars(node* root, const std::vector<std::pair<std::string, std::string>>& renaming_pairs) {
    // Check if the current node is a VARIABLE and needs renaming
    if (root->type == VARIABLE) {
        for (const auto& pair : renaming_pairs) {
            if (root->name() == pair.first) {
                // Rename the variable
                root->set_name(pair.second);
                
                break; // Assuming no duplicate original names
            }
        }
    }

    // Recursively traverse and rename all child nodes
    for (auto& child : root->children) {
        rename_vars(child, renaming_pairs);
    }
}

// turn disjunction into implication
node* disjunction_to_implication(node* formula) {
   if (formula->is_disjunction()) {
       node* antecedent = formula->children[0];
       node* negated = negate_node(antecedent);

       std::vector<node*> children;
       children.push_back(negated);
       children.push_back(formula->children[1]);

       node* impl = new node(LOGICAL_BINARY, SYMBOL_IMPLIES, children);

       formula->children.clear();

       return impl;
   } else
       return formula;
}
// node.cpp

#include "node.h"
#include <stdexcept>
#include <iostream>

variable_data* deep_copy(variable_data* vdata) {
    return new variable_data{vdata->var_kind, vdata->bound, vdata->shared, vdata->structure, vdata->arity, vdata->name};
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
            copied_node = new node(VARIABLE);
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
            std::cout << n->type << std::endl;
            throw std::logic_error("Unsupported node type for deep_copy");
    }

    return copied_node;
}

// Helper function to create a negation node
node* create_negation(node* child) {
    std::vector<node*> children = { child };
    return new node(LOGICAL_UNARY, SYMBOL_NOT, children);
}

// The negate_node function implementation
node* negate_node(node* n, bool rewrite_disj) {
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
                return rewrite_disj ? disjunction_to_implication(phi) : phi;
            } else {
                // For other unary logical operators, wrap with NOT
                return create_negation(n);
            }

        case LOGICAL_BINARY: {
            // Apply De Morgan's laws and other logical negations
            if (n->symbol == SYMBOL_AND) {
                // ¬(φ ∧ ψ) ≡ ¬φ ∨ ¬ψ
                node* left_neg = negate_node(n->children[0]);
                node* right_neg = negate_node(n->children[1]);
                std::vector<node*> children;
                children.push_back(left_neg);
                children.push_back(right_neg);
                node* res = new node(LOGICAL_BINARY, SYMBOL_OR, children);
                n->children.clear();
                delete n;
                return rewrite_disj ? disjunction_to_implication(res) : res;
            }
            else if (n->symbol == SYMBOL_OR) {
                // ¬(φ ∨ ψ) ≡ ¬φ ∧ ¬ψ
                node* left_neg = negate_node(n->children[0]);
                node* right_neg = negate_node(n->children[1]);
                std::vector<node*> children;
                children.push_back(left_neg);
                children.push_back(right_neg);
                n->children.clear();
                delete n;
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
                n->children.clear();
                delete n;
                return new node(LOGICAL_BINARY, SYMBOL_AND, children);
            }
            else if (n->symbol == SYMBOL_IFF) {
                // ¬(φ ↔ ψ) ≡ (φ ∧ ¬ψ) ∨ (ψ ∧ ¬φ)
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
                // Create (ψ ∧ ¬φ)
                std::vector<node*> right_children;
                right_children.push_back(psi);
                right_children.push_back(neg_phi);
                node* right_clause = new node(LOGICAL_BINARY, SYMBOL_AND, right_children);
                // Create OR node
                std::vector<node*> children;
                children.push_back(left_clause);
                children.push_back(right_clause);
                node* res = new node(LOGICAL_BINARY, SYMBOL_OR, children);
                n->children.clear();
                delete n;
                return rewrite_disj ? disjunction_to_implication(res) : res;
            }
            else {
                // For other binary operators, wrap with NOT
                return create_negation(n);
            }
        }

        case QUANTIFIER: {
            // Negate quantifiers: ¬∀x φ ≡ ∃x ¬φ and ¬∃x φ ≡ ∀x ¬φ
            symbol_enum new_quantifier = (n->symbol == SYMBOL_FORALL) ? SYMBOL_EXISTS : SYMBOL_FORALL;

            node* variable = n->children[0];
            node* formula = n->children[1];

            // Clear children to prevent deletion
            n->children.clear();
            
            // Clean up original node
            delete n;

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
            if (n->symbol == SYMBOL_TOP) {
                delete n;
                return new node(CONSTANT, SYMBOL_BOT);
            }
            else if (n->symbol == SYMBOL_BOT) {
                delete n;
                return new node(CONSTANT, SYMBOL_TOP);
            }
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

void mark_shared(node* current, const std::set<std::string>& var_names) {
    // Check if the current node is a VARIABLE and its name is in the set
    if (current->type == VARIABLE) {
        const std::string& current_name = current->name();
        if (var_names.find(current_name) != var_names.end()) {
            current->vdata->shared = true; // Mark as shared
        }
    }

    // Recursively traverse all child nodes
    for (auto& child : current->children) {
        mark_shared(child, var_names);
    }
}

void vars_used(std::set<std::string>& variables, const node* root, bool include_params, bool include_bound) {
    // If the current node is a VARIABLE, add its name to the set
    if (root->type == VARIABLE && (root->vdata->var_kind == INDIVIDUAL || root->vdata->var_kind == PARAMETER) &&
            (include_params || (root->vdata->var_kind != PARAMETER && root->vdata->var_kind != FUNCTION)) &&
            (include_bound || !root->vdata->bound)) {
        variables.insert(root->name());
    }

    // Recursively traverse all child nodes
    for (const auto& child : root->children) {
        vars_used(variables, child, include_params, include_bound);
    }
}

// Function to find common variables between two formulas
std::set<std::string> find_common_variables(const node* formula1, const node* formula2) {
    std::set<std::string> vars1;
    std::set<std::string> vars2;

    // Collect variables from both formulas
    vars_used(vars1, formula1, false);
    vars_used(vars2, formula2);

    // Find the intersection of vars1 and vars2
    std::set<std::string> common_vars;
    std::set_intersection(
        vars1.begin(), vars1.end(),
        vars2.begin(), vars2.end(),
        std::inserter(common_vars, common_vars.begin())
    );

    return common_vars;
}

bool node::has_shared_vars() const {
    if (is_shared_variable()) {
        return true;
    }

    for (node* n : children) {
        if (n->has_shared_vars()) {
            return true;
        }
    }

    return false;
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

// Rename all variables according to a list of renames
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

       delete formula;

       return impl;
   } else
       return formula;
}

// Function to convert a conjunction into a list of its conjuncts
std::vector<node*> conjunction_to_list(node* conjunction) {
    std::vector<node*> conjuncts;

    if (!conjunction->is_conjunction()) {
        // Not a disjunction; return the node itself as a deep copy
        conjuncts.push_back(deep_copy(conjunction));
        return conjuncts;
    }

    // Traverse the left children to collect all conjuncts in reverse order
    node* current = conjunction;
    while (current->is_conjunction()) {
        // Push the right child (current->children[1]) first
        conjuncts.push_back(deep_copy(current->children[1]));
        // Move to the left child (current->children[0])
        current = current->children[0];
    }

    // Add the last conjunct (leftmost child) as a deep copy
    conjuncts.push_back(deep_copy(current));

    // Reverse the conjuncts to maintain left-to-right order
    std::reverse(conjuncts.begin(), conjuncts.end());

    return conjuncts;
}

// Take the contrapositive of an implication
node* contrapositive(node* implication) {
    if (!implication->is_implication()) {
        std::cerr << "Error: The node to negate is not an implication." << std::endl;
        return nullptr;
    }

    node* antecedent = implication->children[0];
    node* consequent = implication->children[1];

    // Negate the consequent: ¬B
    node* not_consequent = negate_node(deep_copy(consequent));
    if (!not_consequent) {
        std::cerr << "Error: Failed to negate the consequent." << std::endl;
        return nullptr;
    }

    // Negate the antecedent: ¬A
    node* not_antecedent = negate_node(deep_copy(antecedent));
    if (!not_antecedent) {
        std::cerr << "Error: Failed to negate the antecedent." << std::endl;
        delete not_consequent;
        return nullptr;
    }

    // Create the new implication: ¬B -> ¬A
    std::vector<node*> children;
    children.push_back(not_consequent);  // Push the EQUALS operator node
    children.push_back(not_antecedent);  // Push the first term
    node* new_implication = new node(LOGICAL_BINARY, SYMBOL_IMPLIES, children);

    return new_implication;
}

// Function to compare two nodes for equality up to variable mapping
bool equal_helper(const node* a, const node* b, std::unordered_map<std::string, std::string>& var_map) {
    // Compare node types
    if (a->type != b->type)
        return false;

    switch (a->type) {
        case VARIABLE:
            // Handle variable comparison without mapping here
            if (a->vdata->var_kind == INDIVIDUAL) {
                const std::string& var_a = a->vdata->name;
                const std::string& var_b = b->vdata->name;

                auto it = var_map.find(var_a);
                if (it != var_map.end()) {
                    // Variable has been mapped in a quantifier, check consistency
                    if (it->second != var_b)
                        return false;
                } else {
                    // Free variables must match exactly
                    if (var_a != var_b)
                        return false;
                }
            } else {
                // For other VariableKind types, names must match exactly
                if (a->vdata->name != b->vdata->name)
                    return false;
            }
            break;

        case CONSTANT:
            // Compare symbols for CONSTANT nodes
            if (a->symbol != b->symbol)
                return false;
            break;

        case QUANTIFIER:
            // Compare quantifier symbols (e.g., ∀, ∃)
            if (a->symbol != b->symbol)
                return false;
            
            // Map the bound variable from 'a' to 'b'
            {
                const node* a_var = a->children[0];
                const node* b_var = b->children[0];
                
                // Add mapping for bound variables
                var_map[a_var->vdata->name] = b_var->vdata->name;
            }

            // Recursively compare the formulas under the quantifiers
            if (!equal_helper(a->children[1], b->children[1], var_map))
                return false;
            break;

        case LOGICAL_UNARY:
            // Compare symbols and recursively compare single child
            if (a->symbol != b->symbol)
                return false;
            // Assuming LOGICAL_UNARY nodes have exactly one child
            if (!equal_helper(a->children[0], b->children[0], var_map))
                return false;
            break;

        case LOGICAL_BINARY:
            // Compare symbols and recursively compare both children
            if (a->symbol != b->symbol)
                return false;
            if (!equal_helper(a->children[0], b->children[0], var_map))
                return false;
            if (!equal_helper(a->children[1], b->children[1], var_map))
                return false;
            break;

        case UNARY_OP:
        case BINARY_OP:
            return (a->symbol == b->symbol);
            break;

        case UNARY_PRED:
        case BINARY_PRED:
            if (a->symbol != b->symbol)
                return false;
            for (size_t i = 0; i < a->children.size(); ++i) {
                if (!equal_helper(a->children[i], b->children[i], var_map))
                    return false;
            }
            break;
        
        case APPLICATION:
            // Compare the operator node (first child)
            if (!equal_helper(a->children[0], b->children[0], var_map))
                return false;
            // Compare the arguments (remaining children)
            if (a->children.size() != b->children.size())
                return false; // APPLICATION nodes should have the same number of arguments
            for (size_t i = 1; i < a->children.size(); ++i) {
                if (!equal_helper(a->children[i], b->children[i], var_map))
                    return false;
            }
            break;

        case TUPLE:
            // Compare all children recursively
            // Assuming TUPLE nodes can have a variable number of children
            if (a->children.size() != b->children.size())
                return false;
            for (size_t i = 0; i < a->children.size(); ++i) {
                if (!equal_helper(a->children[i], b->children[i], var_map))
                    return false;
            }
            break;

        default:
            // For unhandled types, assume not equal
            return false;
    }

    return true;
}

// Compares formulas up to renaming of variables bound in expressions
bool equal(const node* a, const node* b) {
    std::unordered_map<std::string, std::string> var_map;
    return equal_helper(a, b, var_map);
}

// Traverse a formula and get all constants
void node_get_constants(std::vector<std::string>& constants, const node* formula) {
    // Define the node types that can contain constants
    const std::vector<node_type> constant_node_types = {
        UNARY_OP,
        BINARY_OP,
        UNARY_PRED,
        BINARY_PRED,
        CONSTANT
    };

    // Check if the current node is of a type that can contain a constant
    if (std::find(constant_node_types.begin(), constant_node_types.end(), formula->type) != constant_node_types.end()) {
        symbol_enum sym = formula->symbol;
        if (sym >= SYMBOL_EQUALS) { // Assuming constants are ordered after SYMBOL_EQUALS
            // Find the Unicode representation from precedenceTable
            auto it = precedenceTable.find(sym);
            if (it != precedenceTable.end()) {
                const std::string& unicode_str = it->second.unicode;
                // Add the constant if it's not already in the list
                if (std::find(constants.begin(), constants.end(), unicode_str) == constants.end()) {
                    constants.push_back(unicode_str);
                }
            }
            else {
                std::cerr << "Warning: Symbol not found in precedenceTable." << std::endl;
            }
        }
    }

    // Recursively traverse child nodes
    for (const auto& child : formula->children) {
        node_get_constants(constants, child);
    }
}

// Return true if all variables on right side of implication are found on the left side
// and max_term_size of right side is at most that of the left side
void left_to_right(bool& ltor, bool& rtol, bool& ltor_safe, bool& rtol_safe, const node* implication) {
    // Extract left (premise) and right (conclusion) sub-nodes
    const node* premise = implication->children[0];
    const node* conclusion = implication->children[1];
    
    // Retrieve variables used in premise and conclusion
    std::set<std::string> premise_vars;
    vars_used(premise_vars, premise, false, false);

    std::set<std::string> conclusion_vars;
    vars_used(conclusion_vars, conclusion, false, false);

    size_t left_term_depth = max_term_depth(premise);
    size_t right_term_depth = max_term_depth(conclusion);

    ltor_safe = right_term_depth <= left_term_depth;
    
    ltor = true;
    
    // Check if all variables in conclusion are present in premise
    for (const auto& var : conclusion_vars) {
        if (premise_vars.find(var) == premise_vars.end()) {
            // Variable in conclusion not found in premise
            ltor = false;
            break;
        }
    }

    rtol_safe = left_term_depth <= right_term_depth;
    
    rtol = true;

    // Check if all variables in conclusion are present in premise
    for (const auto& var : premise_vars) {
        if (conclusion_vars.find(var) == conclusion_vars.end()) {
            // Variable in conclusion not found in premise
            rtol = false;
            break;
        }
    }
}

// Given a formula which is wrapped in special implications, remove the
// special implications and return the matrix. Does not deep copy.
node* unwrap_special(node* formula) {
    node* matrix = formula;

    while (matrix->is_special_implication()) {
         matrix = matrix->children[1]; // unpeel one special implication
    }

    return matrix;
}

// Given a formula which is wrapped in special implications, return the matrix
// of the formula and add the special predicates to the list
node* split_special(std::vector<node*>& specials, node * formula) {
    std::vector<node*> special_list; // list of specials
    node* matrix = formula;

    while (matrix->is_special_implication()) {
        specials.push_back(matrix->children[0]); // save special predicate
        matrix = matrix->children[1]; // unpeel one special implication
    }

    return matrix;
}

// Prepend a list of given special predicates to a formula as implications
// In each case, the variable should actually be used in the formula
// The special predicates are deep copied internally. The user is responsible
// for cleanup of the originals and the new formula
node* reapply_special(std::vector<node*>& special_predicates, node* formula) {
    std::set<std::string> vars;
    vars_used(vars, formula, false, false); // Get all unbound variables used
    
    // Apply special predicates in reverse list order
    for (int i = special_predicates.size() - 1; i >= 0; --i) {
        node* special = special_predicates[i];

        // Check this variable is actually used
        if (vars.find(special->children[1]->vdata->name) == vars.end()) {
            continue;
        }

        // Make copy of special
        special = deep_copy(special);

        // Add special implication to formula
        std::vector<node*> children;
        children.push_back(special);
        children.push_back(formula);
        formula = new node(LOGICAL_BINARY, SYMBOL_IMPLIES, children);
    }

    return formula;
}

// Return the expression depth of a formula
size_t formula_depth(const node* formula) {
    size_t max_depth = 0;

    for (node* child : formula->children) {
        size_t depth = formula_depth(child);
        if (depth > max_depth) {
            max_depth = depth;
        }
    }

    return max_depth + 1;
}

// Return the maximum term depth of a formula
size_t max_term_depth(const node* formula) {
    if (formula->is_term()) {
        return formula_depth(formula);
    }

    size_t max_depth = 0;

    for (node* child : formula->children) {
        size_t depth = max_term_depth(child);
        if (depth > max_depth) {
            max_depth = depth;
        }
    }

    return max_depth;
}
// node.cpp

#include "node.h"
#include <stdexcept>

// Helper function to deep copy a node
node* deep_copy(const node* n) {
    if (n == nullptr) {
        throw std::invalid_argument("Cannot copy a null node");
    }

    std::vector<node*> copied_children;
    for (const auto& child : n->children) {
        copied_children.push_back(deep_copy(child));
    }

    node* copied_node = nullptr;

    switch (n->type) {
        case node::VARIABLE:
            copied_node = new node(node::VARIABLE, n->name());
            break;
        case node::CONSTANT:
            copied_node = new node(node::CONSTANT, n->symbol);
            break;
        case node::QUANTIFIER:
            // Assuming QUANTIFIER nodes have exactly two children: variable and formula
            if (n->children.size() != 2) {
                throw std::logic_error("Quantifier node must have exactly two children");
            }
            copied_node = new node(node::QUANTIFIER, n->symbol, { deep_copy(n->children[0]), deep_copy(n->children[1]) });
            break;
        case node::LOGICAL_UNARY:
            copied_node = new node(node::LOGICAL_UNARY, n->symbol, copied_children);
            break;
        case node::LOGICAL_BINARY:
            copied_node = new node(node::LOGICAL_BINARY, n->symbol, copied_children);
            break;
        case node::UNARY_OP:
            copied_node = new node(node::UNARY_OP, n->symbol, copied_children);
            break;
        case node::BINARY_OP:
            copied_node = new node(node::BINARY_OP, n->symbol, copied_children);
            break;
        case node::UNARY_PRED:
            copied_node = new node(node::UNARY_PRED, n->symbol, copied_children);
            break;
        case node::BINARY_PRED:
            copied_node = new node(node::BINARY_PRED, n->symbol, copied_children);
            break;
        case node::APPLICATION:
            copied_node = new node(node::APPLICATION, n->symbol, copied_children);
            break;
        case node::TUPLE:
            copied_node = new node(node::TUPLE, copied_children);
            break;
        default:
            throw std::logic_error("Unsupported node type for deep copy");
    }

    return copied_node;
}

// Helper function to create a negation node
node* create_negation(node* child) {
    std::vector<node*> children = { child };
    return new node(node::LOGICAL_UNARY, SYMBOL_NOT, children);
}

// The negate_node function implementation
node* negate_node(node* n) {
    if (n == nullptr) {
        throw std::invalid_argument("Cannot negate a null node");
    }

    switch (n->type) {
        case node::UNARY_PRED:
        case node::BINARY_PRED: {
            // Create a NOT node wrapping the predicate
            return create_negation(n);
        }

        case node::LOGICAL_UNARY:
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

        case node::LOGICAL_BINARY: {
            // Apply De Morgan's laws and other logical negations
            if (n->children.size() != 2) {
                throw std::logic_error("Logical binary node must have exactly two children");
            }

            if (n->symbol == SYMBOL_AND) {
                // ¬(φ ∧ ψ) ≡ ¬φ ∨ ¬ψ
                node* left_neg = negate_node(n->children[0]);
                node* right_neg = negate_node(n->children[1]);
                return new node(node::LOGICAL_BINARY, SYMBOL_OR, { left_neg, right_neg });
            }
            else if (n->symbol == SYMBOL_OR) {
                // ¬(φ ∨ ψ) ≡ ¬φ ∧ ¬ψ
                node* left_neg = negate_node(n->children[0]);
                node* right_neg = negate_node(n->children[1]);
                return new node(node::LOGICAL_BINARY, SYMBOL_AND, { left_neg, right_neg });
            }
            else if (n->symbol == SYMBOL_IMPLIES) {
                // ¬(φ → ψ) ≡ φ ∧ ¬ψ
                node* phi = n->children[0];
                node* psi = n->children[1];
                node* negated_psi = negate_node(psi);
                // Clear children to prevent deletion
                n->children.clear();
                // Create AND node
                return new node(node::LOGICAL_BINARY, SYMBOL_AND, { phi, negated_psi });
            }
            else if (n->symbol == SYMBOL_IFF) {
                // ¬(φ ↔ ψ) ≡ (φ ∧ ¬ψ) ∨ (¬φ ∧ ψ)
                node* phi = n->children[0];
                node* psi = n->children[1];
                node* neg_phi = negate_node(phi);
                node* neg_psi = negate_node(psi);
                // Clear children to prevent deletion
                n->children.clear();
                // Create (φ ∧ ¬ψ)
                node* left_clause = new node(node::LOGICAL_BINARY, SYMBOL_AND, { phi, neg_psi });
                // Create (¬φ ∧ ψ)
                node* right_clause = new node(node::LOGICAL_BINARY, SYMBOL_AND, { neg_phi, psi });
                // Create OR node
                return new node(node::LOGICAL_BINARY, SYMBOL_OR, { left_clause, right_clause });
            }
            else {
                // For other binary operators, wrap with NOT
                return create_negation(n);
            }
        }

        case node::QUANTIFIER: {
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

            return new node(node::QUANTIFIER, new_quantifier, { variable, negated_formula });
        }

        case node::APPLICATION: {
            if (n->children[0]->is_predicate()) {
                // Negate the APPLICATION node by wrapping it with NOT
                return create_negation(n);
            }
            else {
                // For other APPLICATION nodes, do not allow negation
                throw std::logic_error("Cannot negate an APPLICATION node unless its first child is a PREDICATE");
            }
        }

        case node::UNARY_OP:
        case node::BINARY_OP:
        case node::VARIABLE:
        case node::CONSTANT:
        case node::TUPLE:
            // For all term node types, do not negate and throw an exception
            throw std::logic_error("Cannot negate a term. Only predicates and logical formulas can be negated.");

        default:
            throw std::logic_error("Unsupported node type for negation");
    }
}

#ifndef NODE_H
#define NODE_H

#include "symbol_enum.h"
#include "precedence.h"
#include <vector>
#include <string>
#include <sstream>
#include <iostream>
#include <set>
#include <algorithm>

enum OutputFormat {
    REPR,    // Re-parsable string format
    UNICODE  // Unicode format for user display
};

enum VariableKind {
    INDIVIDUAL,
    FUNCTION,
    PREDICATE,
    METAVAR, // accepts formulas
    PARAMETER // constant variable
};

enum node_type {
    VARIABLE, // node(VARIABLE, "x")
    CONSTANT, // node(CONSTANT, SYMBOL_EMPTYSET)
    QUANTIFIER, // node(QUANTIFIER, [node(VARIABLE, "x"), formula])
    LOGICAL_UNARY, // node(LOGICAL_UNARY, SYMBOL_NOT, [formula])
    LOGICAL_BINARY, // node(LOGICAL_BINARY, SYMBOL_AND, [formula1, formula2])
    UNARY_OP, // node(UNARY_OP, SYMBOL_POWERSET)
    BINARY_OP, // node(BINARY_OP, SYMBOL_CAP)
    UNARY_PRED, // node(UNARY_PRED, ??)
    BINARY_PRED, // node(BINARY_PRED, SYMBOL_EQUALS)
    APPLICATION, // node(APPLICATION, [node(VARIABLE, "f"), term1, term2, ...])
                    // node(APPLICATION, [node(UNARY_OP, SYMBOL_POWERSET), term])
                    // node(APPLICATION, [node(BINARY_OP, SYMBOL_CAP), term1, term2])
    TUPLE // node(TUPLE, [term1, term2, ...])
};

std::string remove_subscript(const std::string& var_name);

std::string append_subscript(const std::string& base, int index);

int get_subscript(const std::string& var_name);

std::string append_unicode_subscript(const std::string& base, int index);

struct variable_data {
    VariableKind var_kind;
    bool bound;
    bool shared;
    bool structure;
    int arity;
    std::string name;
};

class node {
public:
    node_type type;
    symbol_enum symbol;
    variable_data* vdata; // Pointer to vdata for VARIABLE nodes
    std::vector<node*> children;

    node(node_type t, const std::string& name)
        : type(VARIABLE), symbol(SYMBOL_NONE), vdata(new variable_data{INDIVIDUAL, false, false, false, 0, name}), children() {}

    node(node_type t)
        : type(t), symbol(SYMBOL_NONE), vdata(nullptr), children() {}

    node(node_type t, symbol_enum sym)
        : type(t), symbol(sym), vdata(nullptr), children() {}

    node(node_type t, symbol_enum sym, const std::vector<node*>& children)
        : type(t), symbol(sym), vdata(nullptr), children(children) {}

    node(node_type t, const std::vector<node*>& children)
        : type(t), symbol(SYMBOL_NONE), vdata(nullptr), children(children) {}

    ~node() {
        if (vdata) {
            delete vdata;
        }
        for (auto child : children) {
            delete child;
        }
        children.clear();
    }

    bool is_predicate() const {
        return (type == BINARY_PRED || type == UNARY_PRED ||
                (type == VARIABLE && vdata->var_kind == PREDICATE) ||
                (type == CONSTANT && (symbol == SYMBOL_TOP || symbol == SYMBOL_BOT)));
    }

    bool is_variable() const {
        return (type == VARIABLE && vdata->var_kind == INDIVIDUAL);
    }

    bool is_free_variable() const {
        return (type == VARIABLE && vdata->var_kind == INDIVIDUAL && !vdata->bound);
    }

    bool is_shared_variable() const {
        return (type == VARIABLE && vdata->var_kind == INDIVIDUAL && vdata->shared);
    }

    bool has_shared_vars() const;

    bool is_negation() const {
        return (type == LOGICAL_UNARY && symbol == SYMBOL_NOT);
    }
    
    bool is_disjunction() const {
        return (type == LOGICAL_BINARY && symbol == SYMBOL_OR);
    }
    
    bool is_conjunction() const {
        return (type == LOGICAL_BINARY && symbol == SYMBOL_AND);
    }
    
    bool is_implication() const {
        return (type == LOGICAL_BINARY && symbol == SYMBOL_IMPLIES);
    }
    
    bool is_equivalence() const {
        return (type == LOGICAL_BINARY && symbol == SYMBOL_IFF);
    }
    
    bool is_application() const {
        return (type == APPLICATION);
    }
    
    bool is_special_predicate() const {
        return (is_application() && children[0]->type == VARIABLE &&
                 children[0]->vdata->var_kind == PREDICATE &&
                 children[0]->vdata->structure);
    }

    bool is_special_implication() const {
        return (is_implication() && children[0]->is_special_predicate());
    }

    bool is_special_binder() const {
        return (type == QUANTIFIER && children[1]->is_special_implication());
    }
    
    bool is_element_quantifier() const {
        return (type == QUANTIFIER && children[1]->is_implication() &&
                children[1]->children[0]->type == APPLICATION &&
                children[1]->children[0]->children[0]->type == BINARY_PRED &&
                children[1]->children[0]->children[0]->symbol == SYMBOL_ELEM &&
                children[1]->children[0]->children[1]->type == VARIABLE &&
                children[0]->name() == children[1]->children[0]->children[1]->name());
    }
    
    bool is_term() const {
        return ((type == VARIABLE && vdata->var_kind != PREDICATE && vdata->var_kind != METAVAR)
             || (type == APPLICATION && children[0]->is_term())
             || (type == CONSTANT) || (type == UNARY_OP)
             || (type == BINARY_OP) || (type == TUPLE));
    }
    
    // Function to get the variable name if the node is of type VARIABLE
    std::string name() const {
        if (type == VARIABLE && vdata) {
            return vdata->name;
        }
        throw std::logic_error("Node is not of type VARIABLE");
    }

    void set_name(std::string name) const {
        if (type != VARIABLE) {
           throw std::logic_error("Node is not of type VARIABLE");
        }

        vdata->name = name;
    }
    
    // Print function that accepts an OutputFormat enum
    void print(OutputFormat format = REPR) const {
        std::cout << to_string(format) << std::endl;
    }

    // Helper to generate string based on format type ("repr" or "unicode")
    std::string to_string(OutputFormat format = REPR) const {
        std::ostringstream oss;
        PrecedenceInfo precInfo = getPrecedenceInfo(symbol);

        switch (type) {
            case VARIABLE:
                if (format == UNICODE) {
                    int subscript = get_subscript(name());
                    if (subscript >= 0 && subscript <= 9) {
                        std::string base = remove_subscript(name());
                        std::string subscript_unicode = append_unicode_subscript(base, subscript);
                        oss << subscript_unicode;
                    } else {
                        oss << name();
                    }
                    if (vdata->var_kind == INDIVIDUAL && !vdata->bound) {
                         oss << "'";
                    }
                } else { // REPR
                    oss << name();
                }
                break;
            case CONSTANT:
            case UNARY_OP:
            case BINARY_OP:
            case UNARY_PRED:
            case BINARY_PRED:
                oss << (format == REPR ? precInfo.repr : precInfo.unicode);
                break;
            case LOGICAL_UNARY:
                if (symbol == SYMBOL_NOT && children[0]->type == APPLICATION &&
                    children[0]->children[0]->type == BINARY_PRED &&
                    children[0]->children[0]->symbol == SYMBOL_EQUALS) { // neq
                        oss << children[0]->children[1]->to_string(format) <<
                           (format == REPR ? " \\neq " : " ≠ ") <<
                           children[0]->children[2]->to_string(format);
                } else {
                    oss << (format == REPR ? precInfo.repr + " " : precInfo.unicode);
                    oss << parenthesize(children[0], format, "left");
                }
                break;
            case LOGICAL_BINARY:
                if (is_special_implication()) {
                    // print special constraints in square brackets
                    node* special = children[0];
                    node* formula = children[1];
                    oss << "[";
                    oss << special->children[1]->to_string(format) << ":" << special->children[0]->to_string(format);
                    while (formula->is_special_implication()) {
                        special = formula->children[0];
                        formula = formula->children[1];
                        oss << ", ";
                        oss << special->children[1]->to_string(format) << ":" << special->children[0]->to_string(format);
                    }
                    oss << "] ";
                    oss << formula->to_string(format);
                } else {
                    oss << parenthesize(children[0], format, "left") + " ";
                    oss << (format == REPR ? precInfo.repr : precInfo.unicode);
                    oss << " " << parenthesize(children[1], format, "right");
                }
                break;
            case APPLICATION:
                if (children[0]->type == BINARY_OP || children[0]->type == UNARY_OP ||
                    children[0]->type == BINARY_PRED || children[0]->type == UNARY_PRED) {
                    PrecedenceInfo childPrecInfo = getPrecedenceInfo(children[0]->symbol);

                    // Handle binary operators
                    if (childPrecInfo.fixity == Fixity::INFIX && children.size() == 3) {
                        oss << parenthesize(children[1], format, "left") << " ";
                        oss << (format == REPR ? childPrecInfo.repr : childPrecInfo.unicode) << " "; // Print the operator
                        oss << parenthesize(children[2], format, "right");
                    }
                    // Handle unary operators
                    else if (childPrecInfo.fixity == Fixity::FUNCTIONAL || children.size() == 2) {
                        oss << (format == REPR ? childPrecInfo.repr : childPrecInfo.unicode) << "(";
                        oss << children[1]->to_string(format);  // Print the argument
                        oss << ")";
                    }
                } else if (children[0]->is_predicate() && children[0]->vdata->structure) {
                    oss << children[1]->to_string(format) << ":" << children[0]->to_string(format);
                } else {
                    // If the operator is a variable or application
                    oss << children[0]->to_string(format) << "("; // Print the operator
                    for (size_t i = 1; i < children.size(); ++i) {
                        if (i > 1) oss << ", ";
                        oss << children[i]->to_string(format);  // Print the arguments
                    }
                    oss << ")";
                }
                break;
            case TUPLE:
                oss << "(";
                for (size_t i = 0; i < children.size(); ++i) {
                    if (i > 0) oss << ", ";
                    oss << children[i]->to_string(format);
                }
                oss << ")";
                break;
            case QUANTIFIER:
                oss << (format == REPR ? precInfo.repr + " " : precInfo.unicode);
                if (is_special_binder()) {
                    node* special = children[1]->children[0];
                    oss << special->children[1]->to_string(format) << ":" << special->children[0]->to_string(format) << " ";
                    oss << parenthesize(children[1]->children[1], format, "true");
                } else if (is_element_quantifier()) {
                    oss << children[1]->children[0]->to_string(format) << " ";
                    oss << parenthesize(children[1]->children[1], format, "true");
                } else {
                    oss << children[0]->to_string(format) << " ";
                    oss << parenthesize(children[1], format, "true");
                }
                break;
            default:
                break;
        }
        return oss.str();
    }

private:

    // Helper function to parenthesize based on precedence and associativity
    std::string parenthesize(const node *child, OutputFormat format, const std::string& childPosition) const {
        symbol_enum parent_symbol = type == APPLICATION ? children[0]->symbol : symbol;
        symbol_enum child_symbol = child->type == APPLICATION ? child->children[0]->symbol : child->symbol;

        PrecedenceInfo parentPrecInfo = getPrecedenceInfo(parent_symbol);
        PrecedenceInfo childPrecInfo = getPrecedenceInfo(child_symbol);

        // If the child is a simple variable or constant, return it as is
        if (child->type == VARIABLE || child->type == CONSTANT ||
            child->type == TUPLE || child->type == QUANTIFIER ||
             (child->type == APPLICATION && childPrecInfo.fixity == Fixity::FUNCTIONAL)) {
            return child->to_string(format);
        }

        // Handle parentheses based on precedence and associativity
        if (childPrecInfo.precedence < parentPrecInfo.precedence) {
            return child->to_string(format);
        }

        if (childPrecInfo.precedence == parentPrecInfo.precedence) {
            if ((parent_symbol != child_symbol) ||
                (parentPrecInfo.associativity == Associativity::LEFT && childPosition == "right") ||
                (parentPrecInfo.associativity == Associativity::RIGHT && childPosition == "left")) {
                return "(" + child->to_string(format) + ")";
            } else {
                return child->to_string(format);
            }
        }

        return "(" + child->to_string(format) + ")";
    }
};

node* deep_copy(const node* n);

node* negate_node(node *n, bool rewrite_disj = false);

void bind_var(node* current, const std::string& var_name);

void unbind_var(node* current, const std::string& var_name);

void mark_shared(node* current, const std::set<std::string>& var_names);

void vars_used(std::set<std::string>& variables, const node* root, bool include_params=true, bool include_bound=true);

std::set<std::string> find_common_variables(const node* formula1, const node* formula2);

void rename_vars(node* root, const std::vector<std::pair<std::string, std::string>>& renaming_pairs);

node* disjunction_to_implication(node* formula);

std::vector<node*> conjunction_to_list(node* conjunction);

node* contrapositive(node* implication);

bool equal(const node* a, const node* b);

void node_get_constants(std::vector<std::string>& constants, const node* formula);

void left_to_right(bool& ltor, bool& rtol, const node* implication);

node* unwrap_special(node* formula);

node* split_special(std::vector<node*>& specials, node * formula);

node* reapply_special(std::vector<node*>& special_predicates, node* formula);

size_t formula_depth(const node* formula);

size_t max_term_depth(const node* formula);

#endif // NODE_H
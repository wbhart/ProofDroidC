#ifndef NODE_H
#define NODE_H

#include "symbol_enum.h"
#include <vector>
#include <string>
#include <iostream>

class node {
public:
    enum node_type {
        VARIABLE,
        CONSTANT,
        QUANTIFIER,
        BINARY_OP,
        UNARY_OP,
        APPLICATION,
        TUPLE
    };

    // Node properties
    node_type type;
    symbol_enum symbol;   // For operators/quantifiers/constants
    std::string var_name; // Only used for VARIABLE nodes
    std::vector<node*> children; // Children nodes (for binary/unary/applications)

    // Constructor for a variable node
    node(const std::string& var_name) 
        : type(VARIABLE), symbol(SYMBOL_VARIABLE), var_name(var_name) {}

    // Constructor for a constant or operator node (without children)
    node(symbol_enum sym, node_type t) 
        : type(t), symbol(sym) {}

    // Constructor for operator nodes (with children)
    node(symbol_enum sym, node_type t, const std::vector<node*>& children)
        : type(t), symbol(sym), children(children) {}

    // Debug print function
    void print(int indent = 0) const {
        std::string indent_str(indent, ' ');
        if (type == VARIABLE) {
            std::cout << indent_str << "Variable: " << var_name << "\n";
        } else if (type == CONSTANT) {
            std::cout << indent_str << "Constant: " << symbol << "\n";
        } else if (type == QUANTIFIER) {
            std::cout << indent_str << "Quantifier: " << symbol << "\n";
        } else if (type == BINARY_OP) {
            std::cout << indent_str << "Binary Op: " << symbol << "\n";
        } else if (type == UNARY_OP) {
            std::cout << indent_str << "Unary Op: " << symbol << "\n";
        } else if (type == APPLICATION) {
            std::cout << indent_str << "Application\n";
        } else if (type == TUPLE) {
            std::cout << indent_str << "Tuple\n";
        }

        for (const auto& child : children) {
            child->print(indent + 2);
        }
    }
};

// Factory functions for creating nodes
inline node* node_create_variable(const char* name) {
    return new node(std::string(name));
}

inline node* node_create_const(symbol_enum sym) {
    return new node(sym, node::CONSTANT);
}

inline node* node_create_binary_op(symbol_enum sym) {
    return new node(sym, node::BINARY_OP);
}

inline node* node_create_unary_op(symbol_enum sym) {
    return new node(sym, node::UNARY_OP);
}

inline node* node_create_quantifier(symbol_enum sym, node* variable, node* formula) {
    return new node(sym, node::QUANTIFIER, { variable, formula });
}

inline node* node_create_application(node* symbol, node** args, int count) {
    std::vector<node*> children(args, args + count);
    return new node(symbol->symbol, node::APPLICATION, children);
}

inline node* node_create_tuple(node** terms, int count) {
    std::vector<node*> children(terms, terms + count);
    return new node(SYMBOL_VARIABLE, node::TUPLE, children); // Assuming a tuple has a default symbol
}

// List creation helpers for arguments (used in application)
inline node** node_list_create0() {
    return nullptr;
}

inline node** node_list_create1(node* node1) {
    node** list = new node*[1];
    list[0] = node1;
    return list;
}

inline node** node_list_create2(node* node1, node* node2) {
    node** list = new node*[2];
    list[0] = node1;
    list[1] = node2;
    return list;
}

#endif // NODE_H


#ifndef NODE_H
#define NODE_H

#include "symbol_enum.h"
#include <vector>
#include <string>
#include <iostream>
#include <memory>  // For unique_ptr

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
    symbol_enum symbol;   
    std::string var_name;  
    std::vector<std::unique_ptr<node>> children;  // Children as unique_ptr for automatic memory management

    // Constructor for a variable node
    node(const std::string& var_name) 
        : type(VARIABLE), var_name(var_name) {}

    // Constructor for a constant or operator node (without children)
    node(symbol_enum sym, node_type t) 
        : type(t), symbol(sym) {}

    // Constructor for operator nodes (with children)
    node(symbol_enum sym, node_type t, std::vector<std::unique_ptr<node>> children)
        : type(t), symbol(sym), children(std::move(children)) {}

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

#endif // NODE_H

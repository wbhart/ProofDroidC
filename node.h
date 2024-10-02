#include <vector>
#include <string>
#include <iostream>

class node {
public:
    // Enum for node types
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
    std::vector<node*> children;

    // Constructor for a variable node
    node(node_type t, const std::string& var_name) 
        : type(t), var_name(var_name) {}

    // Constructor for a constant or operator node (without children)
    node(node_type t, symbol_enum sym) 
        : type(t), symbol(sym) {}

    // Constructor for operator nodes (with children)
    node(node_type t, symbol_enum sym, const std::vector<node*>& children)
        : type(t), symbol(sym), children(children) {}

    // Constructor for application or tuple nodes (without symbol_enum)
    node(node_type t, const std::vector<node*>& children)
        : type(t), children(children) {}

    // Constructor for unary operators (with operator as a node and children)
    node(node_type t, node* op, const std::vector<node*>& children)
        : type(t), symbol(op->symbol), children(children) {}

    // Destructor to free child nodes
    ~node() {
        for (auto child : children) {
            delete child;
        }
    }

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

#ifndef NODE_H
#define NODE_H

#include "symbol_enum.h"
#include "precedence.h"
#include <vector>
#include <string>
#include <sstream>
#include <iostream>

enum class OutputFormat {
    REPR,    // Re-parsable string format
    UNICODE  // Unicode format for user display
};

class node {
public:
    enum node_type {
        VARIABLE,
        CONSTANT,
        QUANTIFIER,
        LOGICAL_UNARY,
        LOGICAL_BINARY,
        BINARY_OP,
        UNARY_OP,
        APPLICATION,
        TUPLE
    };

    node_type type;
    symbol_enum symbol;
    bool bound; // for variables
    std::string var_name;
    std::vector<node*> children;

    node(node_type t, const std::string& var_name) 
        : type(VARIABLE), symbol(SYMBOL_NONE), var_name(var_name) {}

    node(node_type t, symbol_enum sym) 
        : type(t), symbol(sym) {}

    node(node_type t, symbol_enum sym, const std::vector<node*>& children) 
        : type(t), symbol(sym), children(children) {}

    node(node_type t, const std::vector<node*>& children) 
        : type(t), symbol(SYMBOL_NONE), children(children) {}

    // Print function that accepts an OutputFormat enum
    void print(OutputFormat format = OutputFormat::REPR) const {
        std::cout << to_string_format(format) << std::endl;
    }

    // String representation based on format ("repr" for re-parsing, "unicode" for user display)
    std::string to_string(OutputFormat format) const {
        return to_string_format(format);
    }

private:
    // Helper to generate string based on format type ("repr" or "unicode")
    std::string to_string_format(OutputFormat format) const {
        std::ostringstream oss;
        PrecedenceInfo precInfo = getPrecedenceInfo(symbol);

        switch (type) {
            case VARIABLE:
                oss << var_name;
                break;
            case CONSTANT:
            case UNARY_OP:
            case BINARY_OP:
                oss << (format == OutputFormat::REPR ? precInfo.repr : precInfo.unicode);
                break;
            case LOGICAL_UNARY:
                oss << (format == OutputFormat::REPR ? precInfo.repr + " " : precInfo.unicode);
                oss << parenthesize(children[0], format, "left");
                break;
            case LOGICAL_BINARY:
                oss << parenthesize(children[0], format, "left") + " ";
                oss << (format == OutputFormat::REPR ? precInfo.repr : precInfo.unicode);
                oss << " " << parenthesize(children[1], format, "right");
                break;
            case APPLICATION:
                if (children[0]->type == BINARY_OP || children[0]->type == UNARY_OP) {
                    PrecedenceInfo childPrecInfo = getPrecedenceInfo(children[0]->symbol);

                    // Handle binary operators
                    if (childPrecInfo.fixity == Fixity::INFIX && children.size() == 3) {
                        oss << children[1]->to_string_format(format) << " ";
                        oss << (format == OutputFormat::REPR ? childPrecInfo.repr : childPrecInfo.unicode) << " "; // Print the operator
                        oss << children[2]->to_string_format(format);
                    }
                    // Handle unary operators
                    else if (childPrecInfo.fixity == Fixity::FUNCTIONAL || children.size() == 2) {
                        oss << (format == OutputFormat::REPR ? childPrecInfo.repr : childPrecInfo.unicode) << "(";
                        oss << children[1]->to_string_format(format);  // Print the argument
                        oss << ")";
                    }
                } else {
                    // If the operator is a variable or application
                    oss << children[0]->to_string_format(format) << "("; // Print the operator
                    for (size_t i = 1; i < children.size(); ++i) {
                        if (i > 1) oss << ", ";
                        oss << children[i]->to_string_format(format);  // Print the arguments
                    }
                    oss << ")";
                }
                break;
            case TUPLE:
                oss << "(";
                for (size_t i = 0; i < children.size(); ++i) {
                    if (i > 0) oss << ", ";
                    oss << children[i]->to_string_format(format);
                }
                oss << ")";
                break;
            case QUANTIFIER:
                oss << (format == OutputFormat::REPR ? precInfo.repr + " " : precInfo.unicode);
                oss << children[0]->to_string_format(format) << " " << parenthesize(children[1], format, "true");
                break;
            default:
                break;
        }
        return oss.str();
    }

    // Helper function to parenthesize based on precedence and associativity
    std::string parenthesize(const node *child, OutputFormat format, const std::string& childPosition) const {
        PrecedenceInfo parentPrecInfo = getPrecedenceInfo(symbol);
        PrecedenceInfo childPrecInfo = child->type == APPLICATION ?
                getPrecedenceInfo(child->children[0]->symbol) : getPrecedenceInfo(child->symbol);

        // If the child is a simple variable or constant, return it as is
        if (child->type == VARIABLE || child->type == CONSTANT ||
            child->type == TUPLE || child->type == QUANTIFIER ||
             (child->type == APPLICATION && childPrecInfo.fixity == Fixity::FUNCTIONAL)) {
            return child->to_string_format(format);
        }

        // Handle parentheses based on precedence and associativity
        if (childPrecInfo.precedence < parentPrecInfo.precedence) {
            return child->to_string_format(format);
        }

        if (childPrecInfo.precedence == parentPrecInfo.precedence) {
            if ((parentPrecInfo.associativity == Associativity::LEFT && childPosition == "right") ||
                (parentPrecInfo.associativity == Associativity::RIGHT && childPosition == "left")) {
                return "(" + child->to_string_format(format) + ")";
            }
        }

        return "(" + child->to_string_format(format) + ")";
    }
};

#endif // NODE_H

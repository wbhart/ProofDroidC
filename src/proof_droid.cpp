// proof_droid.cpp

#include "grammar.h"
#include "node.h"
#include "context.h"
#include <iostream>
#include <string>
#include <fstream>

// Function to print all formulas in the tableau
void print_tableau(const context_t& tab_ctx) {
    for (size_t i = 0; i < tab_ctx.tableau.size(); ++i) {
        const tabline_t& tabline = tab_ctx.tableau[i];
        std::cout << i + 1 << ": ";
        if (tabline.target) {
            std::cout << "Target: " << tabline.negation->to_string(UNICODE) << std::endl;
        } else {
            std::cout << tabline.formula->to_string(UNICODE) << std::endl;
        }
    }
}

int main(int argc, char** argv) {
    // Check for filename argument
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <filename>" << std::endl;
        return 1;
    }

    // Open the specified file
    std::ifstream infile(argv[1]);
    if (!infile) {
        std::cerr << "Error opening file: " << argv[1] << std::endl;
        return 1;
    }

    // Initialize a new blank context for the tableau
    context_t tab_ctx;

    // Initialize manager and parser context
    manager_t mgr;
    mgr.input = nullptr; // Initialize manager input
    mgr.pos = 0;

    parser_context_t *ctx = parser_create(&mgr);
    if (ctx == nullptr) {
        std::cerr << "Failed to create parser context." << std::endl;
        return 1;
    }

    std::string line;
    int line_number = 0;

    // Read the file line by line
    while (getline(infile, line)) {
        line_number++;
        if (line.empty()) continue; // Skip empty lines

        bool is_target = false;
        std::string formula_str = line;

        // Check if the line is a target (starts with "* ")
        if (line.size() >= 2 && line[0] == '*' && line[1] == ' ') {
            is_target = true;
            formula_str = line.substr(2); // Remove "* " prefix
        }

        std::string modified_input = formula_str;
        modified_input.push_back('\n'); // Add newline to the input

        // Set the input buffer and reset position
        mgr.input = modified_input.c_str();
        mgr.pos = 0;

        // Parse the input
        node* ast = nullptr;
        parser_parse(ctx, &ast);

        if (!ast) {
            std::cerr << "Error parsing line " << line_number << ": " << formula_str << std::endl;
            continue; // Skip to the next line
        }

        if (is_target) {
            // Deep copy and negate the formula
            node* ast_copy = deep_copy(ast);
            node* negated = negate_node(ast_copy);

            // Create a tabline struct with target=true
            tabline_t tabline(negated);
            tabline.negation = ast;
            tabline.target = true;

            // Add to the tableau
            tab_ctx.tableau.push_back(tabline);
        }
        else {
            // Non-target formula, add directly to the tableau
            tabline_t tabline(ast);
            tab_ctx.tableau.push_back(tabline);
        }
    }

    // Clean up parser context
    parser_destroy(ctx);
    infile.close();

    // Enter interactive mode
    std::cout << "> ";
    while (getline(std::cin, line)) {
        if (line == "r") {
            // Print all formulas in the context
            print_tableau(tab_ctx);
        }
        else if (line == "q") {
            break; // Exit the application
        }
        else {
            std::cout << "Unknown command. Use 'r' to print formulas or 'q' to quit." << std::endl;
        }

        std::cout << "> ";
    }

    // Clean up memory by deleting all node pointers in the tableau
    for (auto& tabline : tab_ctx.tableau) {
        if (tabline.target && tabline.negation) {
            delete tabline.negation;
        }
        if (tabline.formula) {
            delete tabline.formula;
        }
    }

    return 0;
}

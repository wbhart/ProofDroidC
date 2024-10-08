// proof_droid.cpp

#include "grammar.h"
#include "node.h"
#include "context.h"
#include "moves.h"
#include <iostream>
#include <string>
#include <fstream>
#include <vector>
#include <algorithm>

// Enum representing the possible user options
enum class option_t {
    OPTION_QUIT,
    OPTION_MANUAL,
    OPTION_SKOLEMIZE
};

// Structure representing an option entry with key and help message
struct option_entry {
    option_t option;
    std::string key;
    std::string message;
};

// Constant table containing all possible options with their keys and messages
const std::vector<option_entry> all_options = {
    {option_t::OPTION_QUIT, "q", "quit"},
    {option_t::OPTION_MANUAL, "m", "manual mode"},
    {option_t::OPTION_SKOLEMIZE, "s", "skolemize"}
};

// Function to print all formulas in the tableau
void print_tableau(const context_t& tab_ctx) {
    std::cout << "Hypotheses:" << std::endl;
    for (size_t i = 0; i < tab_ctx.tableau.size(); ++i) {
        const tabline_t& tabline = tab_ctx.tableau[i];
        if (!tabline.target) {
            std::cout << " " << i + 1 << ": ";
            std::cout << tabline.formula->to_string(UNICODE) << std::endl;
        }
    }
    
    std::cout << "Targets:" << std::endl;
    for (size_t i = 0; i < tab_ctx.tableau.size(); ++i) {
        const tabline_t& tabline = tab_ctx.tableau[i];
        if (tabline.target) {
            std::cout << " " << i + 1 << ": ";
            std::cout << tabline.negation->to_string(UNICODE) << std::endl;
        }
    }
}

// Function to print all active options
void print_options(const std::vector<option_t>& active_options) {
    std::cout << "Options:";

    bool first = true;
    for (const auto& opt : active_options) {
        // Find the option_entry corresponding to opt
        auto it = std::find_if(all_options.begin(), all_options.end(),
            [&opt](const option_entry& entry) { return entry.option == opt; });

        if (it != all_options.end()) {
            if (!first) {
                std::cout << ", ";
            } else {
                std::cout << " ";
                first = false;
            }
            std::cout << it->key << " = " << it->message;
        }
    }
    std::cout << std::endl;
}

// Function to find the option_t based on user input key, considering active options
bool get_option_from_key(const std::string& input_key, const std::vector<option_t>& active_options, option_t& selected_option) {
    for (const auto& opt : active_options) {
        // Find the option_entry corresponding to opt
        auto it = std::find_if(all_options.begin(), all_options.end(),
            [&opt](const option_entry& entry) { return entry.option == opt; });

        if (it != all_options.end() && it->key == input_key) {
            selected_option = it->option;
            return true;
        }
    }
    return false;
}

int main(int argc, char** argv) {
    // Check for filename argument
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <filename>" << std::endl;
        return 1;
    }

    std::cout << "Welcome to ProofDroid for C version 0.1!" << std::endl << std::endl;

    // Open the specified file
    std::cout << "Reading " << argv[1] << "..." << std::endl << std::endl;
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
            tabline_t tabline(negated); // The formula field holds the negated formula
            tabline.negation = ast; // The negation field holds the original formula
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

    // Initialize active options: quit, redisplay, manual
    std::vector<option_t> active_options = {
        option_t::OPTION_QUIT,
        option_t::OPTION_MANUAL
    };

    // Display the initial tableau
    print_tableau(tab_ctx);
    std::cout << std::endl;

    // Display the current options
    print_options(active_options);

    // Enter interactive mode
    std::cout << "> ";
    while (getline(std::cin, line)) {
        if (line.empty()) {
            std::cout << "> ";
            continue; // Prompt again
        }

        option_t selected_option;
        bool found = get_option_from_key(line, active_options, selected_option);

        if (!found) {
            std::cout << std::endl << "Unknown command." << std::endl;
        }
        else {
            switch (selected_option) {
                case option_t::OPTION_MANUAL:
                    // Apply parameterize_all to the tableau
                    parameterize_all(tab_ctx);
                    // Remove OPTION_MANUAL and add OPTION_SKOLEMIZE
                    active_options.erase(
                        std::remove(active_options.begin(), active_options.end(), option_t::OPTION_MANUAL),
                        active_options.end()
                    );
                    active_options.push_back(option_t::OPTION_SKOLEMIZE);
                    break;
                case option_t::OPTION_SKOLEMIZE:
                    // Apply skolemize_all to the tableau
                    skolemize_all(tab_ctx);
                    break;
                case option_t::OPTION_QUIT:
                    // Exit the application
                    goto exit_loop;
                default:
                    std::cout << "Unhandled option." << std::endl;
                    break;
            }
        
            std::cout << std::endl;
            print_tableau(tab_ctx);
        }

        // After handling the command, display the current options again
        std::cout << std::endl;
        print_options(active_options);
        std::cout << "> ";
    }

exit_loop:
    // Clean up parser context
    parser_destroy(ctx);
    infile.close();

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

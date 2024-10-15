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
#include <sstream>
#include <set>

// Enum representing the possible user options
enum class option_t {
    OPTION_QUIT,
    OPTION_MANUAL,
    OPTION_SEMI_AUTOMATIC, // Newly added option for Semi-Automatic Mode
    OPTION_SKOLEMIZE,
    OPTION_MODUS_PONENS,
    OPTION_MODUS_TOLLENS,
    OPTION_EXIT_MANUAL,
    OPTION_EXIT_SEMIAUTO,
    OPTION_CONJ_IDEM,
    OPTION_DISJ_IDEM,
    OPTION_SPLIT_CONJUNCTION,
    OPTION_SPLIT_DISJUNCTIVE_IMPLICATION,
    OPTION_SPLIT_CONJUNCTIVE_IMPLICATION,
    OPTION_NEGATED_IMPLICATION,
    OPTION_CONDITIONAL_PREMISE
};

// Structure representing an option entry with key, short message, and detailed description
struct option_entry {
    option_t option;
    std::string key;
    std::string short_message;
    std::string detailed_description;
};

// Constant table containing all possible options with their keys, short messages, and detailed descriptions
const std::vector<option_entry> all_options = {
    {option_t::OPTION_QUIT, "q", "quit", "Quit the program"},
    {option_t::OPTION_MANUAL, "m", "manual mode", "Enter manual mode"},
    {option_t::OPTION_SEMI_AUTOMATIC, "s", "semi-automatic mode", "Enter semi-automatic mode"}, // Newly added option
    {option_t::OPTION_SKOLEMIZE, "s", "skolemize", "Apply Skolemization"},
    {option_t::OPTION_MODUS_PONENS, "p", "modus ponens", "Apply Modus Ponens: p <implication_line> <line1> <line2> ..."},
    {option_t::OPTION_MODUS_TOLLENS, "t", "modus tollens", "Apply Modus Tollens: t <implication_line> <line1> <line2> ..."},
    {option_t::OPTION_EXIT_MANUAL, "x", "exit manual mode", "Exit manual mode"},
    {option_t::OPTION_EXIT_SEMIAUTO, "x", "exit semiautomatic mode", "Exit semiautomatic mode"},
    {option_t::OPTION_CONJ_IDEM, "ci", "conjunctive idempotence", "Apply Conjunctive Idempotence P ∧ P -> P"},
    {option_t::OPTION_DISJ_IDEM, "di", "disjunctive idempotence", "Apply Disjunctive Idempotence P ∨ P -> P"},
    {option_t::OPTION_SPLIT_CONJUNCTION, "sc", "split conjunctions", "Apply Split Conjunctions"},
    {option_t::OPTION_SPLIT_DISJUNCTIVE_IMPLICATION, "sdi", "split disjunctive implication", "Apply Split Disjunctive Implication"},
    {option_t::OPTION_SPLIT_CONJUNCTIVE_IMPLICATION, "sci", "split conjunctive implication", "Apply Split Conjunctive Implication"},
    {option_t::OPTION_NEGATED_IMPLICATION, "ni", "negated implication", "Apply Negated Implication"},
    {option_t::OPTION_CONDITIONAL_PREMISE, "cp", "conditional premise", "Apply Conditional Premise: cp <index>"}
};

// Function to print all formulas in the tableau with reasons
void print_tableau(const context_t& tab_ctx) {
    std::cout << "Hypotheses:" << std::endl;
    
    // First, print all active Hypotheses
    for (size_t i = 0; i < tab_ctx.tableau.size(); ++i) {
        const tabline_t& tabline = tab_ctx.tableau[i];
        if (tabline.active && !tabline.target) {
            std::cout << " " << i + 1 << " "; // Line number
            print_reason(tab_ctx, static_cast<int>(i)); // Print reason
            std::cout << ": " << tabline.formula->to_string(UNICODE) << std::endl;
        }
    }
    
    std::cout << std::endl << "Targets:" << std::endl;
    
    // Then, print all active Targets
    for (size_t i = 0; i < tab_ctx.tableau.size(); ++i) {
        const tabline_t& tabline = tab_ctx.tableau[i];
        if (tabline.active && tabline.target) {
            std::cout << " " << i + 1 << " "; // Line number
            print_reason(tab_ctx, static_cast<int>(i)); // Print reason
            std::cout << ": " << tabline.negation->to_string(UNICODE) << std::endl;
        }
    }
}

// Function to print all active options in a concise summary
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
            std::cout << it->key << " = " << it->short_message;
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

// Function to parse a command into tokens
std::vector<std::string> tokenize(const std::string& input) {
    std::vector<std::string> tokens;
    std::istringstream iss(input);
    std::string token;
    while (iss >> token) {
        tokens.push_back(token);
    }
    return tokens;
}

// Function to print the detailed list of commands in manual mode
void print_detailed_commands(const std::vector<option_t>& active_options) {
    std::cout << "Available Commands:" << std::endl;
    for (const auto& opt : active_options) {
        // Find the option_entry corresponding to opt
        auto it = std::find_if(all_options.begin(), all_options.end(),
            [&opt](const option_entry& entry) { return entry.option == opt; });

        if (it != all_options.end()) {
            std::cout << " " << it->detailed_description << std::endl;
        }
    }
    std::cout << std::endl;
}

// Function to print the summary of available options in manual mode
void print_summary(const std::vector<option_t>& active_options) {
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
            std::cout << it->key << " = " << it->short_message;
        }
    }

    std::cout << std::endl;
}

// Function to handle manual mode
void manual_mode(context_t& tab_ctx, const std::vector<option_t>& manual_active_options) {
    std::cout << "\nWelcome to manual mode." << std::endl;

    // Print detailed list of commands based on active options
    print_detailed_commands(manual_active_options);

    // Print the current tableau
    print_tableau(tab_ctx);
    std::cout << std::endl;

    // Print the summary of available options before the first prompt
    print_summary(manual_active_options);

    std::string input_line;
    while (true) {
        std::cout << "> ";
        if (!getline(std::cin, input_line)) {
            std::cout << std::endl;
            break; // Exit on EOF
        }

        if (input_line.empty()) {
            // Before each prompt, re-print the summary of options
            print_summary(manual_active_options);
            continue; // Prompt again
        }

        // Tokenize the input
        std::vector<std::string> tokens = tokenize(input_line);
        if (tokens.empty()) {
            // Before each prompt, re-print the summary of options
            print_summary(manual_active_options);
            continue;
        }

        std::string command = tokens[0];

        // Handle 'x' and 'q' commands first
        if (command == "x") {
            std::cout << "Exiting manual mode.\n" << std::endl;
            break;
        }

        if (command == "q") {
            exit(0);
        }

        // Check if the command is among the active options
        option_t selected_option;
        bool found = get_option_from_key(command, manual_active_options, selected_option);

        if (!found) {
            std::cerr << std::endl << "Unknown command. Available commands: ";
            // Dynamically construct the list of available commands
            bool first = true;
            for (const auto& opt : manual_active_options) {
                // Skip 'x' and 'q' as they are handled separately
                if (opt == option_t::OPTION_EXIT_MANUAL || opt == option_t::OPTION_QUIT) {
                    continue;
                }

                auto it = std::find_if(all_options.begin(), all_options.end(),
                    [&opt](const option_entry& entry) { return entry.option == opt; });

                if (it != all_options.end()) {
                    if (!first) {
                        std::cout << ", ";
                    } else {
                        first = false;
                    }
                    std::cout << it->key;
                }
            }

            // Always include 'x' and 'q' in the error message
            auto it_exit = std::find_if(all_options.begin(), all_options.end(),
                [](const option_entry& entry) { return entry.option == option_t::OPTION_EXIT_MANUAL; });
            if (it_exit != all_options.end()) {
                if (!manual_active_options.empty()) {
                    std::cout << ", ";
                }
                std::cout << it_exit->key;
            }

            auto it_quit = std::find_if(all_options.begin(), all_options.end(),
                [](const option_entry& entry) { return entry.option == option_t::OPTION_QUIT; });
            if (it_quit != all_options.end()) {
                if (!manual_active_options.empty() || it_exit != all_options.end()) {
                    std::cout << ", ";
                }
                std::cout << it_quit->key;
            }

            std::cout << "." << std::endl << std::endl;

            // Before next prompt, re-print the summary of options
            print_summary(manual_active_options);
            continue;
        }

        if (selected_option == option_t::OPTION_SKOLEMIZE) {
            // Apply skolemize_all to the tableau starting from 0
            bool skolemized = skolemize_all(tab_ctx, 0);
            if (skolemized) {
                std::cout << "Skolemization applied successfully.\n";
            } else {
                std::cout << "No Skolemization applied." << std::endl;
            }

            std::cout << std::endl;
            print_tableau(tab_ctx);
            std::cout << std::endl;

            // Before next prompt, re-print the summary of options
            print_summary(manual_active_options);
            continue;
        }

        // For 'p', 't', 'ci', 'di', 'sc', 'sci', 'sdi', 'ni', and 'cp', handle accordingly
        if (selected_option == option_t::OPTION_MODUS_PONENS || 
            selected_option == option_t::OPTION_MODUS_TOLLENS ||
            selected_option == option_t::OPTION_CONJ_IDEM ||
            selected_option == option_t::OPTION_DISJ_IDEM ||
            selected_option == option_t::OPTION_SPLIT_CONJUNCTION ||
            selected_option == option_t::OPTION_SPLIT_CONJUNCTIVE_IMPLICATION ||
            selected_option == option_t::OPTION_SPLIT_DISJUNCTIVE_IMPLICATION ||
            selected_option == option_t::OPTION_NEGATED_IMPLICATION ||
            selected_option == option_t::OPTION_CONDITIONAL_PREMISE) {
            
            // For 'ci', 'di', 'sc', 'sci', 'sdi', 'ni', and 'cp', handle without additional arguments
            if (selected_option == option_t::OPTION_CONJ_IDEM || 
                selected_option == option_t::OPTION_DISJ_IDEM ||
                selected_option == option_t::OPTION_SPLIT_CONJUNCTION ||
                selected_option == option_t::OPTION_SPLIT_CONJUNCTIVE_IMPLICATION ||
                selected_option == option_t::OPTION_SPLIT_DISJUNCTIVE_IMPLICATION ||
                selected_option == option_t::OPTION_NEGATED_IMPLICATION ||
                selected_option == option_t::OPTION_CONDITIONAL_PREMISE) {
                
                if (selected_option == option_t::OPTION_CONJ_IDEM) {
                    bool applied = move_ci(tab_ctx, 0); // Assuming start from 0
                    if (applied) {
                        std::cout << "Conjunctive Idempotence applied successfully." << std::endl;
                    } else {
                        std::cerr << "Conjunctive Idempotence could not be applied." << std::endl;
                    }
                } else if (selected_option == option_t::OPTION_DISJ_IDEM) {
                    bool applied = move_di(tab_ctx, 0); // Assuming start from 0
                    if (applied) {
                        std::cout << "Disjunctive Idempotence applied successfully." << std::endl;
                    } else {
                        std::cerr << "Disjunctive Idempotence could not be applied." << std::endl;
                    }
                } else if (selected_option == option_t::OPTION_SPLIT_CONJUNCTION) {
                    bool applied = move_sc(tab_ctx, 0); // Assuming start from 0
                    if (applied) {
                        std::cout << "Split Conjunctions applied successfully." << std::endl;
                    } else {
                        std::cerr << "Split Conjunctions could not be applied." << std::endl;
                    }
                } else if (selected_option == option_t::OPTION_SPLIT_CONJUNCTIVE_IMPLICATION) {
                    bool applied = move_sci(tab_ctx, 0); // Assuming start from 0
                    if (applied) {
                        std::cout << "Split Conjunctive Implication applied successfully." << std::endl;
                    } else {
                        std::cerr << "Split Conjunctive Implication could not be applied." << std::endl;
                    }
                } else if (selected_option == option_t::OPTION_SPLIT_DISJUNCTIVE_IMPLICATION) {
                    bool applied = move_sdi(tab_ctx, 0); // Assuming start from 0
                    if (applied) {
                        std::cout << "Split Disjunctive Implication applied successfully." << std::endl;
                    } else {
                        std::cerr << "Split Disjunctive Implication could not be applied." << std::endl;
                    }
                } else if (selected_option == option_t::OPTION_NEGATED_IMPLICATION) {
                    bool applied = move_ni(tab_ctx, 0); // Assuming start from 0
                    if (applied) {
                        std::cout << "Negated Implication applied successfully." << std::endl;
                    } else {
                        std::cerr << "Negated Implication could not be applied." << std::endl;
                    }
                } else if (selected_option == option_t::OPTION_CONDITIONAL_PREMISE) {
                    // Handle the new 'cp' move for Conditional Premise
                    if (tokens.size() < 2) { // Expecting 'cp <index>'
                        std::cerr << "Error: Insufficient arguments. Usage: cp <index>" << std::endl << std::endl;
                        // Before next prompt, re-print the summary of options
                        print_summary(manual_active_options);
                        continue;
                    }

                    // Parse the index
                    int target_index;
                    try {
                        target_index = std::stoi(tokens[1]) - 1; // Convert to 0-based index
                    } catch (...) {
                        std::cerr << "Error: Invalid index." << std::endl << std::endl;
                        // Before next prompt, re-print the summary of options
                        print_summary(manual_active_options);
                        continue;
                    }

                    // Apply conditional_premise
                    bool cp_moved = conditional_premise(tab_ctx, target_index);
                    if (cp_moved) {
                        std::cout << "Conditional Premise applied successfully." << std::endl;
                    } else {
                        std::cerr << "Error: Conditional Premise could not be applied on the specified index." << std::endl;
                    }
                }

                std::cout << std::endl;
                print_tableau(tab_ctx);
                std::cout << std::endl;

                // Before next prompt, re-print the summary of options
                print_summary(manual_active_options);
                continue;
            }

            // For modus ponens and modus tollens, handle as before
            if (selected_option == option_t::OPTION_MODUS_PONENS || 
                selected_option == option_t::OPTION_MODUS_TOLLENS) {
                
                if (tokens.size() < 3) { // Need at least implication_line and one other_line
                    std::cerr << "Error: Insufficient arguments. Usage: " 
                              << command 
                              << " <implication_line> <line1> <line2> ..." 
                              << std::endl << std::endl;
                    // Before next prompt, re-print the summary of options
                    print_summary(manual_active_options);
                    continue;
                }

                // Parse implication_line
                int implication_line;
                try {
                    implication_line = std::stoi(tokens[1]) - 1; // Convert to 0-based index
                } catch (...) {
                    std::cerr << "Error: Invalid implication line number." << std::endl << std::endl;
                    // Before next prompt, re-print the summary of options
                    print_summary(manual_active_options);
                    continue;
                }

                // Parse other_lines
                std::vector<int> other_lines;
                bool parse_error = false;
                for (size_t j = 2; j < tokens.size(); ++j) { // Changed loop variable to 'j' to avoid shadowing 'i'
                    try {
                        int line_num = std::stoi(tokens[j]) - 1; // Convert to 0-based index
                        other_lines.push_back(line_num);
                    } catch (...) {
                        std::cerr << "Error: Invalid line number '" << tokens[j] << "'." << std::endl << std::endl;
                        parse_error = true;
                        break;
                    }
                }
                if (parse_error) {
                    // Before next prompt, re-print the summary of options
                    print_summary(manual_active_options);
                    continue;
                }

                // Determine if applying modus ponens or modus tollens
                bool ponens = (selected_option == option_t::OPTION_MODUS_PONENS);

                // Apply move_mpt
                bool move_applied = move_mpt(tab_ctx, implication_line, other_lines, ponens);

                if (!move_applied) {
                    std::cerr << "Error: Modus " << (ponens ? "Ponens" : "Tollens") << " could not be applied." << std::endl;
                }

                std::cout << std::endl;
                print_tableau(tab_ctx);
                std::cout << std::endl;

                // Before next prompt, re-print the summary of options
                print_summary(manual_active_options);
                continue;
            }

            // For any other cases, re-print the summary of options
            print_summary(manual_active_options);
        }
    }
}

// Function to handle semi-automatic mode
void semi_automatic_mode(context_t& tab_ctx, const std::vector<option_t>& semi_auto_active_options) {
    std::cout << "\nWelcome to semi-automatic mode." << std::endl << std::endl;

    // Print detailed list of commands based on active options
    print_detailed_commands(semi_auto_active_options);

    cleanup_moves(tab_ctx, 0); // Starting from line 0

    // Print the current tableau
    print_tableau(tab_ctx);
    std::cout << std::endl;

    // Print the summary of available options before the first prompt
    print_summary(semi_auto_active_options);

    std::string input_line;
    while (true) {
        std::cout << "> ";
        if (!getline(std::cin, input_line)) {
            std::cout << std::endl;
            break; // Exit on EOF
        }

        if (input_line.empty()) {
            // Before each prompt, re-print the summary of options
            print_summary(semi_auto_active_options);
            continue; // Prompt again
        }

        // Tokenize the input
        std::vector<std::string> tokens = tokenize(input_line);
        if (tokens.empty()) {
            // Before each prompt, re-print the summary of options
            print_summary(semi_auto_active_options);
            continue;
        }

        std::string command = tokens[0];

        // Handle 'x' and 'q' commands first
        if (command == "x") {
            std::cout << "Exiting semi-automatic mode.\n" << std::endl;
            break;
        }

        if (command == "q") {
            exit(0);
        }

        // Check if the command is among the active options
        option_t selected_option;
        bool found = get_option_from_key(command, semi_auto_active_options, selected_option);

        if (!found) {
            std::cerr << std::endl << "Unknown command. Available commands: ";
            // Dynamically construct the list of available commands
            bool first = true;
            for (const auto& opt : semi_auto_active_options) {
                // Skip 'x' and 'q' as they are handled separately
                if (opt == option_t::OPTION_EXIT_SEMIAUTO || opt == option_t::OPTION_QUIT) {
                    continue;
                }

                auto it = std::find_if(all_options.begin(), all_options.end(),
                    [&opt](const option_entry& entry) { return entry.option == opt; });

                if (it != all_options.end()) {
                    if (!first) {
                        std::cout << ", ";
                    } else {
                        first = false;
                    }
                    std::cout << it->key;
                }
            }

            // Always include 'x' and 'q' in the error message
            auto it_exit = std::find_if(all_options.begin(), all_options.end(),
                [](const option_entry& entry) { return entry.option == option_t::OPTION_EXIT_SEMIAUTO; });
            if (it_exit != all_options.end()) {
                if (!semi_auto_active_options.empty()) {
                    std::cout << ", ";
                }
                std::cout << it_exit->key;
            }

            auto it_quit = std::find_if(all_options.begin(), all_options.end(),
                [](const option_entry& entry) { return entry.option == option_t::OPTION_QUIT; });
            if (it_quit != all_options.end()) {
                if (!semi_auto_active_options.empty() || it_exit != all_options.end()) {
                    std::cout << ", ";
                }
                std::cout << it_quit->key;
            }

            std::cout << "." << std::endl << std::endl;

            // Before next prompt, re-print the summary of options
            print_summary(semi_auto_active_options);
            continue;
        }

        // Handle 'p' and 't' commands
        if (selected_option == option_t::OPTION_MODUS_PONENS || 
            selected_option == option_t::OPTION_MODUS_TOLLENS) {
            
            if (tokens.size() < 3) { // Need at least implication_line and one other_line
                std::cerr << "Error: Insufficient arguments. Usage: " 
                          << command 
                          << " <implication_line> <line1> <line2> ..." 
                          << std::endl << std::endl;
                // Before next prompt, re-print the summary of options
                print_summary(semi_auto_active_options);
                continue;
            }

            // Parse implication_line
            int implication_line;
            try {
                implication_line = std::stoi(tokens[1]) - 1; // Convert to 0-based index
            } catch (...) {
                std::cerr << "Error: Invalid implication line number." << std::endl << std::endl;
                // Before next prompt, re-print the summary of options
                print_summary(semi_auto_active_options);
                continue;
            }

            // Parse other_lines
            std::vector<int> other_lines;
            bool parse_error = false;
            for (size_t j = 2; j < tokens.size(); ++j) { // Changed loop variable to 'j' to avoid shadowing 'i'
                try {
                    int line_num = std::stoi(tokens[j]) - 1; // Convert to 0-based index
                    other_lines.push_back(line_num);
                } catch (...) {
                    std::cerr << "Error: Invalid line number '" << tokens[j] << "'." << std::endl << std::endl;
                    parse_error = true;
                    break;
                }
            }
            if (parse_error) {
                // Before next prompt, re-print the summary of options
                print_summary(semi_auto_active_options);
                continue;
            }

            // Determine if applying modus ponens or modus tollens
            bool ponens = (selected_option == option_t::OPTION_MODUS_PONENS);

            // Apply move_mpt
            bool move_applied = move_mpt(tab_ctx, implication_line, other_lines, ponens);

            if (move_applied) {
                std::cout << "Modus " << (ponens ? "Ponens" : "Tollens") << " applied successfully." << std::endl;
                // After applying the move, run cleanup_moves automatically
                size_t cleanup_start_line = 0; // Starting from the beginning
                cleanup_moves(tab_ctx, cleanup_start_line);
            } else {
                std::cerr << "Error: Modus " << (ponens ? "Ponens" : "Tollens") << " could not be applied." << std::endl;
            }

            std::cout << std::endl;
            print_tableau(tab_ctx);
            std::cout << std::endl;

            // Before next prompt, re-print the summary of options
            print_summary(semi_auto_active_options);
            continue;
        }

        // For any other cases, re-print the summary of options
        print_summary(semi_auto_active_options);
    }
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
            std::cerr << "Error parsing line " << line_number << ": " << formula_str << std::endl << std::endl;
            continue; // Skip to the next line
        }

        if (is_target) {
            // Deep copy and negate the formula
            node* ast_copy = deep_copy(ast);
            node* negated = negate_node(ast_copy);

            // Create a tabline struct with target=true and set justification
            tabline_t tabline(negated); // The formula field holds the negated formula
            tabline.negation = ast; // The negation field holds the original formula
            tabline.target = true;
            tabline.active = true;

            // **Added:** Set justification for Target
            tabline.justification = { Reason::Target, {} };

            // Add to the tableau
            tab_ctx.tableau.push_back(tabline);
        }
        else {
            // Non-target formula, add directly to the tableau
            tabline_t tabline(ast);
            tabline.target = false;
            tabline.active = true;

            // **Added:** Set justification for Hypothesis
            tabline.justification = { Reason::Hypothesis, {} };

            tab_ctx.tableau.push_back(tabline);
        }
    }

    // Initialize active options: quit, manual, and semi-automatic
    std::vector<option_t> active_options = {
        option_t::OPTION_QUIT,
        option_t::OPTION_MANUAL,
        option_t::OPTION_SEMI_AUTOMATIC
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
                case option_t::OPTION_MANUAL: {
                    // Define active options within manual mode
                    std::vector<option_t> manual_active_options = {
                        option_t::OPTION_SKOLEMIZE,
                        option_t::OPTION_MODUS_PONENS,
                        option_t::OPTION_MODUS_TOLLENS,
                        option_t::OPTION_CONJ_IDEM,                      // Renamed Conjunctive Idempotence
                        option_t::OPTION_DISJ_IDEM,                      // Added Disjunctive Idempotence
                        option_t::OPTION_SPLIT_CONJUNCTION,               // Added Split Conjunctions
                        option_t::OPTION_SPLIT_CONJUNCTIVE_IMPLICATION,   // Added Split Conjunctive Implication
                        option_t::OPTION_SPLIT_DISJUNCTIVE_IMPLICATION,   // Existing Split Disjunctive Implication
                        option_t::OPTION_NEGATED_IMPLICATION,            // Added Negated Implication
                        option_t::OPTION_CONDITIONAL_PREMISE,            // Added Conditional Premise
                        option_t::OPTION_EXIT_MANUAL,
                        option_t::OPTION_QUIT
                    };

                    // Turn free variables into parameters (constant variables)
                    parameterize_all(tab_ctx);
                    
                    // set up initial hydras
                    tab_ctx.initialize_hydras();
                    std::vector<int> targets = tab_ctx.get_hydra();
                    tab_ctx.select_targets(targets);

                    // Enter manual mode with the current manual_active_options
                    manual_mode(tab_ctx, manual_active_options);

                    // After exiting manual mode, display the tableau and options again
                    print_tableau(tab_ctx);

                    // Reset active_options to include 'm', 's', and 'q'
                    active_options = {
                        option_t::OPTION_QUIT,
                        option_t::OPTION_MANUAL,
                        option_t::OPTION_SEMI_AUTOMATIC
                    };
                    break;
                }
                case option_t::OPTION_SEMI_AUTOMATIC: {
                    // Define active options within semi-automatic mode
                    std::vector<option_t> semi_auto_active_options = {
                        option_t::OPTION_MODUS_PONENS,
                        option_t::OPTION_MODUS_TOLLENS,
                        option_t::OPTION_EXIT_SEMIAUTO, // Reusing OPTION_EXIT as 'x' for exiting modes
                        option_t::OPTION_QUIT
                    };

                    parameterize_all(tab_ctx);
    
                    // set up initial hydras
                    tab_ctx.initialize_hydras();
                    std::vector<int> targets = tab_ctx.get_hydra();
                    tab_ctx.select_targets(targets);
                    
                    // Enter semi-automatic mode with the current semi_auto_active_options
                    semi_automatic_mode(tab_ctx, semi_auto_active_options);

                    // After exiting semi-automatic mode, display the tableau and options again
                    print_tableau(tab_ctx);

                    // Reset active_options to include 'm', 'sa', and 'q'
                    active_options = {
                        option_t::OPTION_QUIT,
                        option_t::OPTION_MANUAL,
                        option_t::OPTION_SEMI_AUTOMATIC
                    };
                    break;
                }
                case option_t::OPTION_SKOLEMIZE: {
                    // Apply skolemize_all to the tableau starting from 0
                    bool skolemized = skolemize_all(tab_ctx, 0);
                    if (skolemized) {
                        std::cout << "Skolemization applied successfully.\n";
                    } else {
                        std::cout << "No Skolemization applied." << std::endl;
                    }
                    print_tableau(tab_ctx);
                    break;
                }
                case option_t::OPTION_QUIT:
                    // Exit the application
                    goto exit_loop;
                default:
                    std::cout << "Unhandled option." << std::endl;
                    break;
            }
        }

        // After handling the command, display the current options again
        std::cout << std::endl;
        print_options(active_options);
        std::cout << "> ";
    }

    std::cout << std::endl;

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

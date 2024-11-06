// proof_droid.cpp

#include "grammar.h"
#include "node.h"
#include "context.h"
#include "moves.h"
#include "completion.h"
#include "library.h"
#include "automation.h"
#include <iostream>
#include <string>
#include <fstream>
#include <vector>
#include <algorithm>
#include <sstream>
#include <set>
#include <map>
#include <optional>

#define DEBUG_HYDRAS 0 // Whether to print hydras after every move in semiautomatic mode

// Enum representing the possible user options
enum class option_t {
    OPTION_QUIT,
    OPTION_MANUAL,
    OPTION_SEMI_AUTOMATIC,
    OPTION_AUTOMATIC,
    OPTION_SKOLEMIZE,
    OPTION_MODUS_PONENS,
    OPTION_MODUS_TOLLENS,
    OPTION_EXIT_MANUAL,
    OPTION_EXIT_SEMIAUTO,
    OPTION_CONJ_IDEM,
    OPTION_DISJ_IDEM,
    OPTION_SPLIT_CONJUNCTION,
    OPTION_SPLIT_DISJUNCTION,
    OPTION_SPLIT_DISJUNCTIVE_IMPLICATION,
    OPTION_SPLIT_CONJUNCTIVE_IMPLICATION,
    OPTION_NEGATED_IMPLICATION,
    OPTION_CONDITIONAL_PREMISE,
    OPTION_MATERIAL_EQUIVALENCE,
    OPTION_LIBRARY_FILTER,
    OPTION_LOAD_THEOREM
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
    {option_t::OPTION_SEMI_AUTOMATIC, "s", "semi-automatic mode", "Enter semi-automatic mode"},
    {option_t::OPTION_AUTOMATIC, "a", "automate", "Automate"},
    {option_t::OPTION_SKOLEMIZE, "s", "skolemize", "Apply Skolemization and Quantifier Elimination"},
    {option_t::OPTION_MODUS_PONENS, "p", "modus ponens P → Q, P", "Apply Modus Ponens: p <implication_line> <line1> <line2> ..."},
    {option_t::OPTION_MODUS_TOLLENS, "t", "modus tollens P → Q, ¬Q", "Apply Modus Tollens: t <implication_line> <line1> <line2> ..."},
    {option_t::OPTION_EXIT_MANUAL, "x", "exit manual mode", "Exit manual mode"},
    {option_t::OPTION_EXIT_SEMIAUTO, "x", "exit semi-automatic mode", "Exit semi-automatic mode"},
    {option_t::OPTION_CONJ_IDEM, "ci", "conjunctive idempotence P ∧ P", "Apply Conjunctive Idempotence"},
    {option_t::OPTION_DISJ_IDEM, "di", "disjunctive idempotence P ∨ P", "Apply Disjunctive Idempotence"},
    {option_t::OPTION_SPLIT_CONJUNCTION, "sc", "split conjunctions P ∧ Q", "Apply Split Conjunctions"},
    {option_t::OPTION_SPLIT_DISJUNCTION, "sd", "split disjunction P ∨ Q", "Apply Split Disjunctions: sd <disjunction_line>"},
    {option_t::OPTION_SPLIT_DISJUNCTIVE_IMPLICATION, "sdi", "split disjunctive implication P ∨ Q → R", "Apply Split Disjunctive Implication"},
    {option_t::OPTION_SPLIT_CONJUNCTIVE_IMPLICATION, "sci", "split conjunctive implication P → Q ∧ R", "Apply Split Conjunctive Implication"},
    {option_t::OPTION_NEGATED_IMPLICATION, "ni", "negated implication ¬(P → Q)", "Apply Negated Implication"},
    {option_t::OPTION_CONDITIONAL_PREMISE, "cp", "conditional premise (target) P → Q", "Apply Conditional Premise: cp <index>"},
    {option_t::OPTION_MATERIAL_EQUIVALENCE, "me", "material equivalence P ↔ Q", "Apply material equivalence"},
    {option_t::OPTION_LIBRARY_FILTER, "f", "library filter", "Filter library lines containing all given symbols: f <module_name> <symbol1> <symbol2> ..."},
    {option_t::OPTION_LOAD_THEOREM, "l", "load theorems", "Load theorems from a module: l <module_name> <line_no1> <line_no2> ..."}
};

// Function to convert a REPR-formatted string to its corresponding Unicode string.
std::string get_unicode_from_repr(const std::string& repr) {
    // Assuming precedenceTable is accessible here
    extern const std::map<symbol_enum, PrecedenceInfo> precedenceTable;
    for (const auto& [sym, info] : precedenceTable) {
        if (info.repr == repr) {
            return info.unicode;
        }
    }
    // If not found, return an empty string and optionally print a warning
    std::cerr << "Warning: REPR \"" << repr << "\" not found in precedenceTable." << std::endl;
    return "";
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

// Function to print the summary of available options in manual or semiautomatic mode
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

// Function to find a loaded module by filename stem
std::optional<context_t*> find_module(const context_t& tab_ctx, const std::string& filename_stem) {
    for (const auto& [fname, ctx] : tab_ctx.modules) {
        if (fname == filename_stem) {
            return const_cast<context_t*>(&ctx); // Casting away constness for simplicity
        }
    }
    return std::nullopt;
}

// Function to check if a module is loaded and if not to load it
// Returns true is module_ctx was loaded correctly
bool load_module(context_t& module_ctx, context_t& tab_ctx, const std::string filename_stem)
{
    // Check if the module is already loaded
    std::optional<context_t*> module_ctx_opt = tab_ctx.find_module(filename_stem);

    if (module_ctx_opt.has_value()) {
        module_ctx = *module_ctx_opt.value();
        std::cout << std::endl;
    }
    else {
        // Load the module
        std::cout << "Loading module \"" << filename_stem << "\"..." << std::endl;
        if (library_load(module_ctx, filename_stem)) {
            std::cout << "Module \"" << filename_stem << "\" loaded successfully." << std::endl << std::endl;
        }
        else {
            std::cerr << "Error: Failed to load module \"" << filename_stem << "\"." << std::endl << std::endl;
            return true;
        }
        module_ctx.get_constants(); // Populate constants
        module_ctx.get_ltor(); // Compute whether implications are left-to-right and/or right-to-left applicable

        tab_ctx.modules.emplace_back(filename_stem, module_ctx);
    }

    return true;
}

// Function to handle the "library filter" option in semiautomatic mode
void handle_library_filter(context_t& tab_ctx, const std::vector<std::string>& tokens) {
    if (tokens.size() < 3) {
        std::cerr << "Error: Insufficient arguments. Usage: f <library_file_name_stem> <symbol1> <symbol2> ..." << std::endl << std::endl;
        return;
    }

    std::string filename_stem = tokens[1];
    std::vector<std::string> repr_symbols(tokens.begin() + 2, tokens.end());

    // Convert REPR symbols to Unicode strings
    std::vector<std::string> unicode_symbols;
    for (const auto& repr : repr_symbols) {
        std::string unicode = get_unicode_from_repr(repr);
        if (!unicode.empty()) {
            unicode_symbols.push_back(unicode);
        }
        else {
            std::cerr << "Error: Failed to convert REPR \"" << repr << "\" to Unicode." << std::endl;
            // Optionally, decide to skip or continue
        }
    }

    if (unicode_symbols.empty()) {
        std::cerr << "Error: No valid symbols provided after conversion." << std::endl << std::endl;
        return;
    }

    context_t module_ctx;
    
    if (!load_module(module_ctx, tab_ctx, filename_stem)) {
        return;
    }

    // Iterate through the digest of the module
    for (const auto& digest_entry : module_ctx.digest) {
        // Each digest_entry is a vector of digest_item
        for (const auto& [module_line_idx, main_tableau_line_idx, kind] : digest_entry) {
            // Validate line index
            if (module_line_idx >= module_ctx.tableau.size()) {
                std::cerr << "Warning: Line index " << module_line_idx << " in digest is out of bounds." << std::endl;
                continue; // Skip invalid indices
            }

            const tabline_t& tabline = module_ctx.tableau[module_line_idx];

            // Check if the tabline's constants contain all the unicode symbols
            bool contains_all = true;
            for (const auto& symbol : unicode_symbols) {
                if (std::find(tabline.constants1.begin(), tabline.constants1.end(), symbol) == tabline.constants1.end() &&
                    std::find(tabline.constants2.begin(), tabline.constants2.end(), symbol) == tabline.constants2.end()) {
                    contains_all = false;
                    break;
                }
            }

            if (contains_all) {
                // Convert the formula to a Unicode string
                std::string formula_str = tabline.formula->to_string(UNICODE);

                // Print the line number (1-based index) and the formula
                std::cout << (module_line_idx + 1) << ": " << formula_str << std::endl;
            }
        }
    }

    std::cout << std::endl;
}

// handle the load theorems option
void handle_load_theorems(context_t& tab_ctx, const std::vector<std::string>& tokens) {
    // Validate the number of arguments
    if (tokens.size() < 3) {
        std::cerr << "Error: Incorrect usage. Usage: l <module_name> <line_no1> <line_no2> ..." << std::endl << std::endl;
        return;
    }

    std::string module_name = tokens[1];
    std::vector<std::string> line_no_strs(tokens.begin() + 2, tokens.end());

    // Find the module in tab_ctx.modules
    std::optional<context_t*> module_ctx_opt = find_module(tab_ctx, module_name);
    if (!module_ctx_opt.has_value()) {
        std::cerr << "Error: Module \"" << module_name << "\" is not loaded." << std::endl << std::endl;
        return;
    }

    context_t* module_ctx = module_ctx_opt.value();

    for (const auto& line_no_str : line_no_strs) {
        // Convert line_no to size_t
        size_t line_no;
        try {
            line_no = std::stoul(line_no_str);
            if (line_no == 0) {
                throw std::invalid_argument("Line number cannot be zero.");
            }
            line_no -= 1; // Convert to 0-based index
        }
        catch (const std::exception& e) {
            std::cerr << "Error: Invalid line number '" << line_no_str << "'. " << e.what() << std::endl;
            continue; // Skip to the next line_no
        }

        // Validate the line number within the module's tableau
        if (line_no >= module_ctx->tableau.size()) {
            std::cerr << "Error: Line number " << (line_no + 1) << " is out of bounds in module \"" << module_name << "\"." << std::endl;
            continue; // Skip to the next line_no
        }

        // Retrieve the digest entry for the specified line
        bool found = false;
        LIBRARY kind;
        for (auto& digest_entry : module_ctx->digest) {
            for (auto& [mod_line_idx, main_line_idx, entry_kind] : digest_entry) {
                if (mod_line_idx == line_no) {
                    if (main_line_idx != -static_cast<size_t>(1)) {
                        std::cerr << "Error: " << (entry_kind == LIBRARY::Theorem ? "Theorem" : "Definition")
                                  << " from module \"" << module_name << "\", line " << (line_no + 1)
                                  << " has already been loaded." << std::endl;
                        found = true; // Mark as found to prevent duplication
                        break;
                    }
                    main_line_idx = tab_ctx.tableau.size(); // Will be the new line index in main tableau
                    kind = entry_kind;
                    found = true;
                    break;
                }
            }
            if (found) break;
        }

        if (!found) {
            std::cerr << "Error: Fact from module \"" << module_name << "\", line " << (line_no + 1)
                      << " not found in digest." << std::endl;
            continue; // Skip to the next line_no
        }

        // Copy the fact's tabline from the module to the main tableau
        const tabline_t& module_tabline = module_ctx->tableau[line_no];
        tabline_t copied_tabline = module_tabline; // Assuming a copy constructor is available

        // Set the reason based on the kind
        if (kind == LIBRARY::Theorem) {
            copied_tabline.justification = { Reason::Theorem, {} };
        }
        else if (kind == LIBRARY::Definition) {
            copied_tabline.justification = { Reason::Definition, {} };
        }

        // Append the copied tabline to the main tableau
        tab_ctx.tableau.push_back(copied_tabline);

        // Update the digest to mark this fact as loaded
        for (auto& digest_entry : module_ctx->digest) {
            for (auto& [mod_line_idx, main_line_idx, entry_kind] : digest_entry) {
                if (mod_line_idx == line_no) {
                    main_line_idx = tab_ctx.tableau.size() - 1; // Set to the index of the newly added line
                    break;
                }
            }
        }
    }

    std::cout << "Theorem(s) loaded successfully" << std::endl;
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
                // Check if done
                check_done(tab_ctx);
            }

            std::cout << std::endl;
            print_tableau(tab_ctx);
            std::cout << std::endl;

            // Before next prompt, re-print the summary of options
            print_summary(manual_active_options);
            continue;
        }

        // For 'p', 't', 'ci', 'di', 'sc', 'sci', 'sdi', 'ni', 'cp', 'me', 'sd' handle accordingly
        if (selected_option == option_t::OPTION_MODUS_PONENS || 
            selected_option == option_t::OPTION_MODUS_TOLLENS ||
            selected_option == option_t::OPTION_CONJ_IDEM ||
            selected_option == option_t::OPTION_DISJ_IDEM ||
            selected_option == option_t::OPTION_SPLIT_CONJUNCTION ||
            selected_option == option_t::OPTION_SPLIT_CONJUNCTIVE_IMPLICATION ||
            selected_option == option_t::OPTION_SPLIT_DISJUNCTIVE_IMPLICATION ||
            selected_option == option_t::OPTION_NEGATED_IMPLICATION ||
            selected_option == option_t::OPTION_CONDITIONAL_PREMISE ||
            selected_option == option_t::OPTION_MATERIAL_EQUIVALENCE ||
            selected_option == option_t::OPTION_SPLIT_DISJUNCTION ||
            selected_option == option_t::OPTION_LIBRARY_FILTER ||
            selected_option == option_t::OPTION_LOAD_THEOREM) {
            
            // For 'ci', 'di', 'sc', 'sci', 'sdi', 'ni', 'cp', and 'me', handle without additional arguments
            if (selected_option == option_t::OPTION_CONJ_IDEM || 
                selected_option == option_t::OPTION_DISJ_IDEM ||
                selected_option == option_t::OPTION_SPLIT_CONJUNCTION ||
                selected_option == option_t::OPTION_SPLIT_CONJUNCTIVE_IMPLICATION ||
                selected_option == option_t::OPTION_SPLIT_DISJUNCTIVE_IMPLICATION ||
                selected_option == option_t::OPTION_NEGATED_IMPLICATION ||
                selected_option == option_t::OPTION_MATERIAL_EQUIVALENCE ||
                selected_option == option_t::OPTION_CONDITIONAL_PREMISE) {
                
                if (selected_option == option_t::OPTION_CONJ_IDEM) {
                    bool applied = move_ci(tab_ctx, 0); // Assuming start from 0
                    if (applied) {
                        // Check if done
                        check_done(tab_ctx, false);
                    }
                } else if (selected_option == option_t::OPTION_DISJ_IDEM) {
                    bool applied = move_di(tab_ctx, 0); // Assuming start from 0
                    if (applied) {
                        // Check if done
                        check_done(tab_ctx, false);
                    }
                } else if (selected_option == option_t::OPTION_SPLIT_CONJUNCTION) {
                    bool applied = move_sc(tab_ctx, 0); // Assuming start from 0
                    if (applied) {
                        // Check if done
                        check_done(tab_ctx, false);
                    }
                } else if (selected_option == option_t::OPTION_SPLIT_CONJUNCTIVE_IMPLICATION) {
                    bool applied = move_sci(tab_ctx, 0); // Assuming start from 0
                    if (applied) {
                        // Check if done
                        check_done(tab_ctx, false);
                    }
                } else if (selected_option == option_t::OPTION_SPLIT_DISJUNCTIVE_IMPLICATION) {
                    bool applied = move_sdi(tab_ctx, 0); // Assuming start from 0
                    if (applied) {
                        // Check if done
                        check_done(tab_ctx, false);
                    }
                } else if (selected_option == option_t::OPTION_NEGATED_IMPLICATION) {
                    bool applied = move_ni(tab_ctx, 0); // Assuming start from 0
                    if (applied) {
                        // Check if done
                        check_done(tab_ctx, false);
                    }
                } else if (selected_option == option_t::OPTION_MATERIAL_EQUIVALENCE) {
                    bool applied = move_me(tab_ctx, 0); // Assuming start from 0
                    if (applied) {
                        // Check if done
                        check_done(tab_ctx, false);
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
                        // Check if done
                        check_done(tab_ctx, false);
                    }
                }

                std::cout << std::endl;
                print_tableau(tab_ctx);
                std::cout << std::endl;

                // Before next prompt, re-print the summary of options
                print_summary(manual_active_options);
                continue;
            }

            // Handle the new 'l' (load theorem) command
            if (selected_option == option_t::OPTION_LOAD_THEOREM) {
                handle_load_theorems(tab_ctx, tokens);
                // After handling, re-print the tableau

                std::cout << std::endl;
                print_tableau(tab_ctx);
                std::cout << std::endl;
                
    #if DEBUG_HYDRAS
                tab_ctx.print_hydras();
    #endif

                // Before next prompt, re-print the summary of options
                print_summary(manual_active_options);
                continue;
            }

            // Handle the existing 'f' (library filter) command
            if (selected_option == option_t::OPTION_LIBRARY_FILTER) {
                handle_library_filter(tab_ctx, tokens);
                // Don't re-print the tableau for this option

    #if DEBUG_HYDRAS
                tab_ctx.print_hydras();
    #endif

                // Before next prompt, re-print the summary of options
                print_summary(manual_active_options);
                continue;
            }

            // Handle other commands like 'sd', 'p', 't', etc.
            if (selected_option == option_t::OPTION_SPLIT_DISJUNCTION) {
                if (tokens.size() != 2) {
                    std::cerr << "Error: Need disjunction line. Usage: "
                            << "sd <disjunction_line>"
                            << std::endl << std::endl;
                    print_summary(manual_active_options);
                    continue;
                }

                // Parse disjunction_line
                size_t disjunction_line;
                try {
                    disjunction_line = std::stoul(tokens[1]) - 1; // Convert to 0-based index
                } catch (...) {
                    std::cerr << "Error: Invalid disjunction line number." << std::endl << std::endl;
                    // Before next prompt, re-print the summary of options
                    print_summary(manual_active_options);
                    continue;
                }

                // Apply move_sd
                bool move_applied = move_sd(tab_ctx, disjunction_line);

                if (move_applied) {
                    // After applying the move, run cleanup_moves automatically
                    cleanup_moves(tab_ctx, tab_ctx.upto);

                    // Check if done
                    check_done(tab_ctx);
                } else {
                    std::cerr << "Error: Split disjunction could not be applied." << std::endl;
                }
                
                std::cout << std::endl;
                print_tableau(tab_ctx);
                std::cout << std::endl;

    #if DEBUG_HYDRAS
                tab_ctx.print_hydras();
    #endif

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

                if (move_applied) {
                    // After applying the move, run cleanup_moves automatically
                    cleanup_moves(tab_ctx, tab_ctx.upto);

                    // Check if done
                    check_done(tab_ctx);
                } else {
                    std::cerr << "Error: Modus " << (ponens ? "Ponens" : "Tollens") << " could not be applied." << std::endl;
                }
                
                std::cout << std::endl;
                print_tableau(tab_ctx);
                std::cout << std::endl;

#if DEBUG_HYDRAS
                tab_ctx.print_hydras();
#endif

                // Before next prompt, re-print the summary of options
                print_summary(manual_active_options);
                continue;
            }
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

        // Handle the new 'l' (load theorem) command
        if (selected_option == option_t::OPTION_LOAD_THEOREM) {
            handle_load_theorems(tab_ctx, tokens);
            // After handling, re-print the tableau

            std::cout << std::endl;
            print_tableau(tab_ctx);
            std::cout << std::endl;
            
#if DEBUG_HYDRAS
            tab_ctx.print_hydras();
#endif

            // Before next prompt, re-print the summary of options
            print_summary(semi_auto_active_options);
            continue;
        }

        // Handle the existing 'f' (library filter) command
        if (selected_option == option_t::OPTION_LIBRARY_FILTER) {
            handle_library_filter(tab_ctx, tokens);
            // Don't re-print the tableau for this option

#if DEBUG_HYDRAS
            tab_ctx.print_hydras();
#endif

            // Before next prompt, re-print the summary of options
            print_summary(semi_auto_active_options);
            continue;
        }

        // Handle other commands like 'sd', 'p', 't', etc.
        if (selected_option == option_t::OPTION_SPLIT_DISJUNCTION) {
            if (tokens.size() != 2) {
                std::cerr << "Error: Need disjunction line. Usage: "
                          << "sd <disjunction_line>"
                          << std::endl << std::endl;
                print_summary(semi_auto_active_options);
                continue;
            }

            // Parse disjunction_line
            size_t disjunction_line;
            try {
                disjunction_line = std::stoul(tokens[1]) - 1; // Convert to 0-based index
            } catch (...) {
                std::cerr << "Error: Invalid disjunction line number." << std::endl << std::endl;
                // Before next prompt, re-print the summary of options
                print_summary(semi_auto_active_options);
                continue;
            }

            // Apply move_sd
            bool move_applied = move_sd(tab_ctx, disjunction_line);

            if (move_applied) {
                // After applying the move, run cleanup_moves automatically
                cleanup_moves(tab_ctx, tab_ctx.upto);

                // Check if done
                check_done(tab_ctx);
            } else {
                std::cerr << "Error: Split disjunction could not be applied." << std::endl;
            }
            
            std::cout << std::endl;
            print_tableau(tab_ctx);
            std::cout << std::endl;

#if DEBUG_HYDRAS
            tab_ctx.print_hydras();
#endif

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
                // After applying the move, run cleanup_moves automatically
                cleanup_moves(tab_ctx, tab_ctx.upto);

                // Check if done
                check_done(tab_ctx);
            } else {
                std::cerr << "Error: Modus " << (ponens ? "Ponens" : "Tollens") << " could not be applied." << std::endl;
            }
            
            std::cout << std::endl;
            print_tableau(tab_ctx);
            std::cout << std::endl;

#if DEBUG_HYDRAS
            tab_ctx.print_hydras();
#endif

            // Before next prompt, re-print the summary of options
            print_summary(semi_auto_active_options);
            continue;
        }

        // For any other cases, re-print the summary of options
        print_summary(semi_auto_active_options);
    }
}

// Entry point of the application
int main(int argc, char** argv) {
    // Initialize variables for command-line parsing
    bool interactive_mode = false;
    std::string filename;

    // Command-line parsing using std::string for safer comparisons
    if (argc == 3 && std::string(argv[1]) == "-i") {
        // Interactive mode: ./proof_droid -i filename.thm
        interactive_mode = true;
        filename = argv[2];
    }
    else if (argc == 2) {
        // Automatic mode: ./proof_droid filename.thm
        interactive_mode = false;
        filename = argv[1];
    }
    else {
        // Invalid usage
        std::cerr << "Usage:\n";
        std::cerr << "  " << argv[0] << " -i <filename.thm>  (Interactive mode)\n";
        std::cerr << "  " << argv[0] << " <filename.thm>     (Automatic mode)\n";
        return 1;
    }

    std::cout << "Welcome to ProofDroid for C version 0.1!" << std::endl << std::endl;

    // Open the specified file
    std::cout << "Reading " << filename << "..." << std::endl << std::endl;
    std::ifstream infile(filename);
    if (!infile) {
        std::cerr << "Error opening file: " << filename << std::endl;
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
            tabline.negation = ast;      // The negation field holds the original formula
            tabline.target = true;
            tabline.active = true;

            // Set justification for Target
            tabline.justification = { Reason::Target, {} };

            // Add to the tableau
            tab_ctx.tableau.push_back(tabline);
        }
        else {
            // Non-target formula, add directly to the tableau
            tabline_t tabline(ast);
            tabline.target = false;
            tabline.active = true;

            // Set justification for Hypothesis
            tabline.justification = { Reason::Hypothesis, {} };

            tab_ctx.tableau.push_back(tabline);
        }
    }

    infile.close(); // Close the input file

    if (interactive_mode) {
        // Interactive Mode: Present options to the user

        // Display the initial tableau
        print_tableau(tab_ctx);
        std::cout << std::endl;

        // Initialize active options: quit, manual, semi-automatic, and automatic
        std::vector<option_t> active_options = {
            option_t::OPTION_QUIT,
            option_t::OPTION_MANUAL,
            option_t::OPTION_SEMI_AUTOMATIC,
            option_t::OPTION_AUTOMATIC
        };

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
                            option_t::OPTION_CONJ_IDEM,
                            option_t::OPTION_DISJ_IDEM,
                            option_t::OPTION_SPLIT_CONJUNCTION,
                            option_t::OPTION_SPLIT_CONJUNCTIVE_IMPLICATION,
                            option_t::OPTION_SPLIT_DISJUNCTIVE_IMPLICATION,
                            option_t::OPTION_NEGATED_IMPLICATION,
                            option_t::OPTION_CONDITIONAL_PREMISE,
                            option_t::OPTION_MATERIAL_EQUIVALENCE,
                            option_t::OPTION_SPLIT_DISJUNCTION,
                            option_t::OPTION_LIBRARY_FILTER,
                            option_t::OPTION_LOAD_THEOREM,
                            option_t::OPTION_EXIT_MANUAL,
                            option_t::OPTION_QUIT
                        };

                        // Turn free variables into parameters (constant variables)
                        parameterize_all(tab_ctx);
                        
                        // Set up initial hydras
                        tab_ctx.initialize_hydras();
                        std::vector<int> targets = tab_ctx.get_hydra();
                        tab_ctx.select_targets(targets);

                        // Enter manual mode with the current manual_active_options
                        manual_mode(tab_ctx, manual_active_options);

                        // After exiting manual mode, display the tableau and options again
                        print_tableau(tab_ctx);

                        // Reset active_options to include 'm', 'sa', 'a', and 'q'
                        active_options = {
                            option_t::OPTION_QUIT,
                            option_t::OPTION_MANUAL,
                            option_t::OPTION_SEMI_AUTOMATIC,
                            option_t::OPTION_AUTOMATIC
                        };
                        break;
                    }
                    case option_t::OPTION_SEMI_AUTOMATIC: {
                        // Define active options within semi-automatic mode
                        std::vector<option_t> semi_auto_active_options = {
                            option_t::OPTION_MODUS_PONENS,
                            option_t::OPTION_MODUS_TOLLENS,
                            option_t::OPTION_SPLIT_DISJUNCTION,
                            option_t::OPTION_LIBRARY_FILTER,
                            option_t::OPTION_LOAD_THEOREM,
                            option_t::OPTION_EXIT_SEMIAUTO,
                            option_t::OPTION_QUIT
                        };

                        parameterize_all(tab_ctx);

                        // Set up initial hydras
                        tab_ctx.initialize_hydras();
                        std::vector<int> targets = tab_ctx.get_hydra();
                        tab_ctx.select_targets(targets);
                        
                        // Enter semi-automatic mode with the current semi_auto_active_options
                        semi_automatic_mode(tab_ctx, semi_auto_active_options);

                        // After exiting semi-automatic mode, display the tableau and options again
                        print_tableau(tab_ctx);

                        // Reset active_options to include 'm', 'sa', 'a', and 'q'
                        active_options = {
                            option_t::OPTION_QUIT,
                            option_t::OPTION_MANUAL,
                            option_t::OPTION_SEMI_AUTOMATIC,
                            option_t::OPTION_AUTOMATIC
                        };
                        break;
                    }
                    case option_t::OPTION_AUTOMATIC: {
                        // Automatic Mode within Interactive Mode
                        context_t module_ctx;
                        // For now, hard code single module load
                        load_module(module_ctx, tab_ctx, "set");

                        parameterize_all(tab_ctx);

                        // Set up initial hydras
                        tab_ctx.initialize_hydras();
                        std::vector<int> targets = tab_ctx.get_hydra();
                        tab_ctx.select_targets(targets);
                        
                        cleanup_moves(tab_ctx, 0); // Starting from line 0

                        // Get constants for the tableau
                        tab_ctx.get_constants();

                        // Call the automate function
                        bool success = automate(tab_ctx);

                        if (success) {
                            tab_ctx.reanimate();
                        }

                        std::cout << std::endl;

                        // After automation, display the tableau again
                        print_tableau(tab_ctx);

                        if (success) {
                            std::cout << std::endl;

                            tab_ctx.print_statistics();
                            std::cout << std::endl;
                        }
                        
                        // Do NOT print additional messages or exit
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
    else {
        // Non-Interactive Mode: Run automatic mode immediately

        // Display the initial tableau
        print_tableau(tab_ctx);
        std::cout << std::endl;

        // Perform automatic mode steps
        context_t module_ctx;
        // For now, hard code single module load
        load_module(module_ctx, tab_ctx, "set");

        parameterize_all(tab_ctx);

        // Set up initial hydras
        tab_ctx.initialize_hydras();
        std::vector<int> targets = tab_ctx.get_hydra();
        tab_ctx.select_targets(targets);
        
        cleanup_moves(tab_ctx, 0); // Starting from line 0

        // Get constants for the tableau
        tab_ctx.get_constants();

        // Call the automate function
        bool success = automate(tab_ctx);

        // Set all lines to active for final display
        tab_ctx.reanimate();

        // After automation, display the tableau again
        print_tableau(tab_ctx);
        std::cout << std::endl;

        if (success) {                        
            tab_ctx.print_statistics(filename, true);
            std::cout << std::endl;
        }

        // Perform cleanup
        parser_destroy(ctx);

        // Clean up memory by deleting all node pointers in the tableau
        for (auto& tabline : tab_ctx.tableau) {
            if (tabline.target && tabline.negation) {
                delete tabline.negation;
            }
            if (tabline.formula) {
                delete tabline.formula;
            }
        }

        // Exit with appropriate return value based on automation success
        if (success) {
            // Optional: Uncomment the next line if you want to print a success message
            // std::cout << "Automatic mode completed successfully." << std::endl;
            return 0; // Success
        }
        else {
            // Optional: Uncomment the next line if you want to print a failure message
            // std::cout << "Automatic mode failed to prove the theorem." << std::endl;
            return 1; // Failure
        }
    }
}
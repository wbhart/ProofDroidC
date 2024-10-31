// library.cpp

#include "context.h"
#include "grammar.h"
#include "hydra.h"
#include "moves.h"
#include <fstream>
#include <iostream>
#include <string>
#include <vector>
#include <set>
#include <utility>
#include <memory>
#include <unordered_set>
#include <functional>

// Loads theorems and definitions from a .dat file into the tableau.
bool library_load(context_t& context, const std::string& base_str) {
    // Step 1: Generate Filename
    std::string filename = base_str + ".dat";

    // Step 2: Open File
    std::ifstream infile(filename);
    if (!infile.is_open()) {
        std::cerr << "Error: Could not open file " << filename << std::endl;
        return false;
    }

    // Step 3: Initialize Parser
    manager_t mgr;
    mgr.input = nullptr; // Initialize manager input
    mgr.pos = 0;

    parser_context_t* ctx = parser_create(&mgr);
    if (ctx == nullptr) {
        std::cerr << "Failed to create parser context." << std::endl;
        infile.close();
        return false;
    }

    // Step 4: Read and Process Records
    std::string line_type, line_formula, line_blank;
    int record_number = 0;

    while (std::getline(infile, line_type)) {
        // Skip blank lines between records
        if (line_type.empty()) {
            continue;
        }

        // Read the formula line
        if (!std::getline(infile, line_formula)) {
            std::cerr << "Error: Incomplete record after type: " << line_type << std::endl;
            break; // Exit the loop as the record is incomplete
        }

        // Read the blank line separating records
        std::getline(infile, line_blank); // This can be empty; no action needed

        record_number++;

        // Step 5: Record initial_upto
        size_t initial_upto = context.upto;

        // Step 6: Parse the Formula
        std::string modified_input = line_formula + "\n"; // Add newline as per parser requirement
        mgr.input = modified_input.c_str();
        mgr.pos = 0;

        node* ast = nullptr;
        parser_parse(ctx, &ast);

        if (!ast) {
            std::cerr << "Error parsing formula in record " << record_number << ": " << line_formula << std::endl;
            continue; // Skip to the next record
        }

        // Step 7: Add Formula to Tableau
        tabline_t new_tabline(ast);
        context.tableau.push_back(new_tabline);

        // Step 8: Perform Cleanup Based on Record Type
        if (line_type == "definition") {
            cleanup_definition(context, initial_upto);
        }
        else if (line_type == "theorem") {
            cleanup_moves(context, initial_upto);
        }
        else {
            std::cerr << "Warning: Unknown record type '" << line_type << "' in record " << record_number << "." << std::endl;
            // Optionally, handle unknown types differently
        }

        context.upto = context.tableau.size();

        // Step 9: Update Digest with LIBRARY Kind
        std::vector<digest_item> digest_entry;
        for (size_t i = initial_upto; i < context.upto; ++i) {
            if (context.tableau[i].active) { // Only consider active lines
                // Determine the kind based on line_type
                LIBRARY kind;
                if (line_type == "theorem") {
                    kind = LIBRARY::Theorem;
                }
                else if (line_type == "definition") {
                    kind = LIBRARY::Definition;
                }
                else {
                    // If unknown type, default or skip
                    std::cerr << "Warning: Skipping line " << (i + 1) << " due to unknown type '" << line_type << "'." << std::endl;
                    continue;
                }

                // Initialize main_tableau_line_idx to a special value (e.g., max size_t) indicating not loaded yet
                digest_entry.emplace_back(i, -static_cast<size_t>(1), kind);
            }
        }

        if (!digest_entry.empty()) {
            context.digest.push_back(digest_entry);
        }
    }

    // Step 10: Clean Up Parser and Close File
    parser_destroy(ctx);
    infile.close();

    return true;
}

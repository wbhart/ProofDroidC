// automation.cpp

#include "automation.h"

#define DEBUG_TABLEAU 1 // whether to print tableau
#define DEBUG_LISTS 0 // whether to print lists of units, targets, impls and associated constants
#define DEBUG_MOVES 1 // whether to print moves that are executed

// whether consts2 is a subset of consts1
bool consts_subset(const std::vector<std::string>& consts1, const std::vector<std::string>& consts2) {
    for (const auto& const_str : consts2) {
        if (std::find(consts1.begin(), consts1.end(), const_str) == consts1.end()) {
            return false;
        }
    }

    return true;
}

// Automation using a waterfall architecture
// Returns true if theorem successfully proved, else false if the automation gets stuck
bool automate(context_t& ctx) {
    // Declare vectors to hold constants and hypothesis indices
    std::vector<std::string> tabc;              // All constants from active lines that are not theorems or definitions
    std::vector<std::string> tarc;              // All constants from active target lines
    std::vector<size_t> impls;                  // Indices of active implication hypotheses
    std::vector<size_t> units;                  // Indices of active non-implication hypotheses

    bool move_made = false; // whether a move was made at any step

    // Waterfall Architecture Loop
    while (true) {
#if DEBUG_TABLEAU
        if (move_made) {
            std::cout << std::endl;
            print_tableau(ctx);
            std::cout << std::endl << std:: endl;
        }
#endif

        move_made = false;

        // Clear data for current tableau
        tabc.clear();
        tarc.clear();
        impls.clear();
        units.clear();
        
        // Accumulate constants and indices using get_tableau_constants
        ctx.get_tableau_constants(tabc, tarc, impls, units);

        // Level 1 of the Waterfall
        // ------------------------
        // (Omitted as per current implementation)

        // Level 2 of the Waterfall (non-library backwards reasoning)
        // ----------------------------------------------------------

        // Access the current leaf hydra (last hydra in the current_hydra path)
        std::shared_ptr<hydra> current_leaf = ctx.current_hydra.back();

        // Extract target indices from the current leaf hydra
        std::vector<int> targets = current_leaf->target_indices;

        move_made = false; // Flag to check if any move was made in this iteration

#if DEBUG_LISTS
        std::cout << "targets: ";
        print_list(targets);
        std::cout << std::endl;
        
        std::cout << "impls: ";
        print_list(impls);
        std::cout << std::endl;

        std::cout << "units: ";
        print_list(units);
        std::cout << std::endl;

        std::cout << "tableau consts: ";
        print_list(tabc);
        std::cout << std::endl;

        std::cout << "target consts: ";
        print_list(tarc);
        std::cout << std::endl;
#endif

        // Iterate over each target in the current leaf hydra
        for (const int target : targets) {
            const tabline_t& target_tabline = ctx.tableau[target];
            const std::vector<std::string>& target_consts = target_tabline.constants1;

            // Iterate over each implication hypothesis
            for (const size_t impl_idx : impls) {
                tabline_t& impl_tabline = ctx.tableau[impl_idx];
                const std::vector<std::string>& impl_consts1 = impl_tabline.constants1;
                const std::vector<std::string>& impl_consts2 = impl_tabline.constants2;

                // Check if the implication has already been applied to this target
                if (std::find(impl_tabline.applied_units.begin(), impl_tabline.applied_units.end(), target) != impl_tabline.applied_units.end()) {
                    continue; // Skip if already applied
                }

#if DEBUG_LISTS
                std::cout << "target constants: ";
                print_list(target_consts);
                std::cout << std::endl;
        
                std::cout << "implication constants: ";
                print_list(impl_consts);
                std::cout << std::endl;
#endif

                // Check if all implication constants are contained within target constants
                bool all_contained_left = consts_subset(target_consts, impl_consts1);
                bool all_contained_right = consts_subset(target_consts, impl_consts2);
                bool consts_ltor = consts_subset(impl_consts1, impl_consts2) || !consts_subset(impl_consts2, impl_consts1);
                bool consts_rtol = consts_subset(impl_consts2, impl_consts1) || !consts_subset(impl_consts1, impl_consts2);
                
                // Prepare the list of other lines (only the target in this case)
                std::vector<int> other_lines = { target };
                bool move_success = false;

                if (all_contained_right && consts_rtol && impl_tabline.rtol) {
                    // Attempt Modus Ponens
                    move_success = move_mpt(ctx, impl_idx, other_lines, true, true); // ponens=true, silent=true

#if DEBUG_MOVES
                    if (move_success) {
                        std::cout << "Level 2: mp " << impl_idx + 1 << " " << target + 1 << std::endl << std::endl;
                    }
#endif
                }

                if (!move_success && all_contained_left && consts_ltor && impl_tabline.ltor) {
                    // Attempt Modus Tollens since Modus Ponens failed
                    move_success = move_mpt(ctx, impl_idx, other_lines, false, true); // ponens=false, silent=true

#if DEBUG_MOVES
                    if (move_success) {
                        std::cout << "Level 2: mt " << impl_idx + 1 << " " << target + 1 << std::endl << std::endl;
                    }
#endif
                }

                if (move_success) {
                    // Add the target to applied_units to prevent reapplication
                    impl_tabline.applied_units.push_back(target);

                    // Cleanup
                    cleanup_moves(ctx, ctx.upto);

                    // Check if the proof is done after applying the move
                    bool done = check_done(ctx, true); // apply_cleanup=true
                    if (done) {
                        return true; // Proof completed successfully
                    }

                    move_made = true; // A move was made; continue the waterfall
                    break; // Exit the implications loop to restart the waterfall
                }
            }

            if (move_made) {
                break; // A move was made; restart the waterfall from the beginning
            }
        }

        if (move_made) { 
            continue; // Move made at this level, restart waterfall at level 1
        }
        
        // Level 3 of the Waterfall (non-library forwards reasoning)
        // ----------------------------------------------------------

        // Iterate over each unit in the units list
        for (const int unit_idx : units) {
            const tabline_t& unit_tabline = ctx.tableau[unit_idx];
            const std::vector<std::string>& unit_consts = unit_tabline.constants1;

            // Iterate over each implication hypothesis
            for (const size_t impl_idx : impls) {
                tabline_t& impl_tabline = ctx.tableau[impl_idx];
                const std::vector<std::string>& impl_consts1 = impl_tabline.constants1;
                const std::vector<std::string>& impl_consts2 = impl_tabline.constants2;

#if DEBUG_LISTS
                std::cout << "unit constants: ";
                print_list(unit_consts);
                std::cout << std::endl;
        
                std::cout << "implication constants: ";
                print_list(impl_consts);
                std::cout << std::endl;
#endif

                // Check if the implication has already been applied to this unit
                if (std::find(impl_tabline.applied_units.begin(), impl_tabline.applied_units.end(), unit_idx) != impl_tabline.applied_units.end()) {
                    continue; // Skip if already applied
                }

                // Check if all unit constants are contained within implication constants
                bool all_contained_left = consts_subset(unit_consts, impl_consts1);
                bool all_contained_right = consts_subset(unit_consts, impl_consts2);
                bool consts_ltor = consts_subset(impl_consts2, impl_consts1) || !consts_subset(impl_consts1, impl_consts2);
                bool consts_rtol = consts_subset(impl_consts1, impl_consts2) || !consts_subset(impl_consts2, impl_consts1);
                
                // Prepare the list of other lines (only the unit in this case)
                std::vector<int> other_lines = { unit_idx };
                bool move_success = false;
                
                if (all_contained_left && consts_ltor && impl_tabline.ltor) {
                    // Attempt Modus Ponens
                    move_success = move_mpt(ctx, impl_idx, other_lines, true, true); // ponens=true, silent=true

#if DEBUG_MOVES
                    if (move_success) {
                        std::cout << "Level 3: mp " << impl_idx + 1 << " " << unit_idx + 1 << std::endl << std::endl;
                    }
#endif
                }

                if (!move_success && all_contained_right && consts_rtol && impl_tabline.rtol) {
                    // Attempt Modus Tollens since Modus Ponens failed
                    move_success = move_mpt(ctx, impl_idx, other_lines, false, true); // ponens=false, silent=true

#if DEBUG_MOVES
                    if (move_success) {
                        std::cout << "Level 3: mt " << impl_idx + 1 << " " << unit_idx + 1 << std::endl << std::endl;
                    }
#endif
                }

                if (move_success) {
                    // Add the unit to applied_units to prevent reapplication
                    impl_tabline.applied_units.push_back(unit_idx);

                    // Cleanup
                    cleanup_moves(ctx, ctx.upto);

                    // Check if the proof is done after applying the move
                    bool done = check_done(ctx, true); // apply_cleanup=true
                    if (done) {
                        return true; // Proof completed successfully
                    }

                    move_made = true; // A move was made; continue the waterfall
                    break; // Exit the implications loop to restart the waterfall
                }
            }

            if (move_made) {
                break; // A move was made; restart the waterfall from the beginning
            }
        }

        if (move_made) { 
            continue; // Move made at this level, restart waterfall at level 1
        }

        // Level 9 of the Waterfall (Library forwards reasoning)
        // ----------------------------------------------------------

        // Iterate over each unit in the units list
        for (const int unit_idx : units) {
            tabline_t& unit_tabline = ctx.tableau[unit_idx];
            const std::vector<std::string>& unit_consts = unit_tabline.constants1;

            for (auto& [name, mod_ctx] : ctx.modules) { // for each loaded module
                for (auto& digest_entry : mod_ctx.digest) { // for each digest record
                    for (auto& [mod_line_idx, main_line_idx, entry_kind] : digest_entry) { // for each theorem in record
                        tabline_t& mod_tabline = mod_ctx.tableau[mod_line_idx];

                        if (entry_kind == LIBRARY::Theorem) {
                            if (mod_tabline.formula->is_implication()) { // library result is implication
                                std::cout << "is theorem" << std::endl;
                                // Check if this theorem has been applied already
                                std::pair<std::string, size_t> mod_pair = {name, mod_line_idx};
                                if (find(unit_tabline.lib_applied.begin(), unit_tabline.lib_applied.end(),
                                                                           mod_pair) == unit_tabline.lib_applied.end()) {
                                    
                                    std::cout << "found" << std::endl;
                                    
                                    const std::vector<std::string>& mod_consts1 = mod_tabline.constants1;
                                    const std::vector<std::string>& mod_consts2 = mod_tabline.constants2;
                                    bool all_contained_left = consts_subset(unit_consts, mod_consts1);
                                    bool all_contained_right = consts_subset(unit_consts, mod_consts2);
                                    bool tab_contained_left = consts_subset(tabc, mod_consts1);
                                    std::cout << "tabc ";
                                    print_list(tabc);
                                    std::cout << std::endl;
                                    std::cout << "modc1 ";
                                    print_list(mod_consts1);
                                    std::cout << std::endl;
                                    
                                    bool tab_contained_right = consts_subset(tabc, mod_consts2);
                                    bool consts_ltor = consts_subset(mod_consts2, mod_consts1) || !consts_subset(mod_consts1, mod_consts2);
                                    bool consts_rtol = consts_subset(mod_consts1, mod_consts2) || !consts_subset(mod_consts2, mod_consts1);
                                    bool failed_left = false;
                                    bool failed_right = false;

                                    node* unit_formula = unit_tabline.formula;
                                        
                                    if (!all_contained_left || !consts_ltor){
                                        failed_left = true;
                                    }

                                    std::cout << "failed_left = " << failed_left << std::endl;
                                    std::cout << "tab_contained_left = " << tab_contained_left << std::endl;

                                    if (!failed_left && tab_contained_left) {
                                        std::cout << "contained_left" << std::endl;
                                            
                                        node* mod_formula = deep_copy(mod_tabline.formula->children[0]);
                                        
                                        // Find common variables
                                        std::set<std::string> common_vars = find_common_variables(unit_formula, mod_formula);
                                        
                                        // Rename variables to prevent capture
                                        if (!common_vars.empty()) {
                                            std::vector<std::pair<std::string, std::string>> rename_list = vars_rename_list(mod_ctx, common_vars);
                                        
                                            // Rename variables in the entire implication_copy
                                            rename_vars(mod_formula, rename_list);
                                        }

                                        std::cout << "mod_formula: " << mod_formula->to_string(UNICODE) << std::endl;
                                        std::cout << "unit_formula: " << unit_formula->to_string(UNICODE) << std::endl;
                                        
                                        // Attempt unification
                                        Substitution subst;
                                        std::optional<Substitution> maybe_subst = unify(mod_formula, unit_formula, subst);

                                        // Check if unification succeeded
                                        if (maybe_subst.has_value()) {
                                            std::cout << "unify succeeded" << std::endl;
                                            
                                            if (main_line_idx == -static_cast<size_t>(1)) {
                                                // Copy the theorems tabline from the module to the main tableau
                                                tabline_t copied_tabline = mod_tabline;

                                                // Set the reason based on the kind
                                                copied_tabline.justification = { Reason::Theorem, {} };

                                                // Append the copied tabline to the main tableau
                                                ctx.tableau.push_back(copied_tabline);

                                                // Update the digest to mark this fact as loaded
                                                main_line_idx = ctx.tableau.size() - 1; // Set to the index of the newly added line
                                            }

                                            std::cout << "attempt mp" << std::endl;
                                            
                                            // Attempt to apply modus_ponens
                                            std::vector<int> other_lines = {unit_idx};
                                            bool move_applied = move_mpt(ctx, main_line_idx, other_lines, true);

                                            if (move_applied) {
                                                move_made = true;

                                                std::pair<std::string, size_t> mod_pair = {name, mod_line_idx};
                                                unit_tabline.lib_applied.push_back(mod_pair);
                                                
                                                // After applying the move, run cleanup_moves automatically
                                                cleanup_moves(ctx, ctx.upto);

                                                // Check if done
                                                if (check_done(ctx)) {
                                                    return true;
                                                }
                                            } else {
                                                failed_left = true;
                                            }
                                        } else {
                                            failed_left = true;
                                        }
        
                                        delete mod_formula;
                                    }

                                    if (!all_contained_right || !consts_rtol){
                                        failed_right = true;
                                    }

                                    if (failed_left && !failed_right && tab_contained_right) {
                                        node* mod_formula = negate_node(deep_copy(mod_tabline.formula->children[1]));
                                        
                                        // Find common variables
                                        std::set<std::string> common_vars = find_common_variables(unit_formula, mod_formula);
                                        
                                        // Rename variables to prevent capture
                                        if (!common_vars.empty()) {
                                            std::vector<std::pair<std::string, std::string>> rename_list = vars_rename_list(mod_ctx, common_vars);
                                        
                                            // Rename variables in the entire implication_copy
                                            rename_vars(mod_formula, rename_list);
                                        }

                                        // Attempt unification
                                        Substitution subst;
                                        std::optional<Substitution> maybe_subst = unify(mod_formula, unit_formula, subst);

                                        // Check if unification succeeded
                                        if (maybe_subst.has_value()) {
                                            if (main_line_idx == -static_cast<size_t>(1)) {
                                                // Copy the theorems tabline from the module to the main tableau
                                                tabline_t copied_tabline = mod_tabline;

                                                // Set the reason based on the kind
                                                copied_tabline.justification = { Reason::Theorem, {} };

                                                // Append the copied tabline to the main tableau
                                                ctx.tableau.push_back(copied_tabline);

                                                // Update the digest to mark this fact as loaded
                                                main_line_idx = ctx.tableau.size() - 1; // Set to the index of the newly added line
                                            }
                                            
                                            // Attempt to apply modus_tollens
                                            std::vector<int> other_lines = {unit_idx};
                                            bool move_applied = move_mpt(ctx, main_line_idx, other_lines, false);

                                            if (move_applied) {
                                                move_made = true;

                                                std::pair<std::string, size_t> mod_pair = {name, mod_line_idx};
                                                unit_tabline.lib_applied.push_back(mod_pair);
                                                
                                                // After applying the move, run cleanup_moves automatically
                                                cleanup_moves(ctx, ctx.upto);

                                                // Check if done
                                                if (check_done(ctx)) {
                                                    return true;
                                                }
                                            } else {
                                                failed_right = true;
                                            }
                                        } else {
                                            failed_right = true;
                                        }

                                        delete mod_formula;
                                    }

                                    // Mark theorem as applied
                                    if (failed_left && failed_right) {
                                        std::pair<std::string, size_t> mod_pair = {name, mod_line_idx};
                                        unit_tabline.lib_applied.push_back(mod_pair);
                                    }
                                }
                            }
                        }

                        if (move_made) {
                            break; // A move was made; restart the waterfall from the beginning
                        }
                    }

                    if (move_made) {
                        break; // A move was made; restart the waterfall from the beginning
                    }
                }

                if (move_made) {
                    break; // A move was made; restart the waterfall from the beginning
                }
            }

            if (move_made) {
                break; // A move was made; restart the waterfall from the beginning
            }
        }

        if (move_made) { 
            continue; // Move made at this level, restart waterfall at level 1
        }

        // Other levels here
        // ....

        if (!move_made) {
            std::cout << "Unable to prove theorem" << std::endl;

            // No moves were made at any level; automation cannot proceed further
            return false;
        }

        // Continue the waterfall loop
    }

    // This point is never reached due to the loop's structure
    return false;
}
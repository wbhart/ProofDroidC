// automation.cpp

#include "automation.h"

#define DEBUG_TABLEAU 0 // whether to print tableau
#define DEBUG_LISTS 0 // whether to print lists of units, targets, impls and associated constants
#define DEBUG_MOVES 0 // whether to print moves that are executed
#define DEBUG_HYDRAS 0 // whether to print hydra graph

// whether consts2 is a subset of consts1
bool consts_subset(const std::vector<std::string>& consts1, const std::vector<std::string>& consts2) {
    for (const auto& const_str : consts2) {
        if (std::find(consts1.begin(), consts1.end(), const_str) == consts1.end()) {
            return false;
        }
    }

    return true;
}

// Performs trial unification for Modus Ponens.
// Returns true if trial unification is successful, false otherwise.
bool trial_modus_ponens(context_t& ctx, const tabline_t& impl_tabline, const tabline_t& unit_tabline, bool forward)
{
    node* impl_formula = unwrap_special(impl_tabline.formula);
    node* antecedent = forward ? deep_copy(impl_formula->children[0]) :
                                  negate_node(deep_copy(impl_formula)->children[1]);

    node* unit_formula = unwrap_special(unit_tabline.formula);

    // Find common variables between unit_formula and impl_formula's antecedent
    std::set<std::string> common_vars = find_common_variables(unit_formula, antecedent);

    // Rename variables to prevent capture
    std::vector<std::pair<std::string, std::string>> rename_list;
    if (!common_vars.empty()) {
        rename_list = vars_rename_list(ctx, common_vars);
        rename_vars(antecedent, rename_list);
    }

    // Attempt unification between the antecedent of the implication and the unit's formula
    Substitution subst;
    bool success = unify(antecedent, unit_formula, subst).has_value();

    delete antecedent; // Clean up the copied formula

    return success;
}

// Performs trial unification for Modus Tollens.
// Returns true if trial unification is successful, false otherwise.
bool trial_modus_tollens(context_t& ctx, const tabline_t& impl_tabline, const tabline_t& unit_tabline, bool forward)
{
    node * consequent = forward ? negate_node(deep_copy(impl_tabline.formula->children[1])) :
                                  deep_copy(impl_tabline.formula->children[0]);

    // Find common variables between unit_formula and negated_consequent
    std::set<std::string> common_vars = find_common_variables(unit_tabline.formula, consequent);

    // Rename variables to prevent capture
    std::vector<std::pair<std::string, std::string>> rename_list;
    if (!common_vars.empty()) {
        rename_list = vars_rename_list(ctx, common_vars);
        rename_vars(consequent, rename_list);
    }

    // Attempt unification between the unit's formula and the negated consequent of the implication
    Substitution subst;
    bool success = unify(consequent, unit_tabline.formula, subst).has_value();

    delete consequent; // Clean up the negated formula

    return success;
}

// Loads a theorem from the module tableau to the main tableau.
// Updates main_line_idx by reference if the theorem is loaded.
// Returns true if the theorem was loaded, false otherwise.
void load_theorem(context_t& ctx, tabline_t& mod_tabline, size_t& main_line_idx, LIBRARY kind)
{
    if (main_line_idx == -static_cast<size_t>(1)) {
        // Copy the theorem's tabline from the module to the main tableau
        tabline_t copied_tabline = mod_tabline;

        // Set the justification based on the kind
        if (mod_tabline.formula->is_implication()) {
            if (kind == LIBRARY::Theorem) {
                copied_tabline.justification = { Reason::Theorem, {} };
            } else {
                copied_tabline.justification = { Reason::Definition, {} };
            }
        } else {
            copied_tabline.justification = { Reason::Special, {} };
        }

        // Append the copied tabline to the main tableau
        ctx.tableau.push_back(copied_tabline);

        // Update main_line_idx to the index of the newly added line
        main_line_idx = ctx.tableau.size() - 1;
    }
}

std::pair<bool, bool> metavar_check(tabline_t& tabline) {
    std::set<std::string> vars_left, vars_right;
    
    vars_used(vars_left, tabline.formula->children[0], false, false);
    vars_used(vars_right, tabline.formula->children[1], false, false);
    bool vars_ltor = (std::includes(vars_left.begin(), vars_left.end(),
        vars_right.begin(), vars_right.end()));
    bool vars_rtol = (std::includes(vars_right.begin(), vars_right.end(),
        vars_left.begin(), vars_left.end()));

    return std::pair(vars_ltor, vars_rtol);
}

// Automation using a waterfall architecture
// Returns true if theorem successfully proved, else false if the automation gets stuck
bool automate(context_t& ctx) {
    // Declare vectors to hold constants and hypothesis indices
    std::vector<std::string> tabc;              // All constants from active lines that are not theorems or definitions
    std::vector<std::string> tarc;              // All constants from active target lines
    std::vector<size_t> impls;                  // Indices of active implication hypotheses
    std::vector<size_t> units;                  // Indices of active non-implication hypotheses
    std::vector<size_t> specials;               // Indices of active special predicates

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

#if DEBUG_HYDRAS
        if (move_made) {
            ctx.print_hydras();
        }
#endif

        move_made = false;

        // Clear data for current tableau
        tabc.clear();
        tarc.clear();
        impls.clear();
        units.clear();
        specials.clear();
        
        // Accumulate constants and indices using get_tableau_constants
        ctx.get_tableau_constants(tabc, tarc, impls, units, specials);

        /*
        // Heuristic: sort units by maximum term depth
        std::sort(units.begin(), units.end(), [&ctx](size_t a, size_t b) {
            tabline_t& aline = ctx.tableau[a];
            tabline_t& bline = ctx.tableau[b];
            return max_term_depth(unwrap_special(aline.formula)) < max_term_depth(unwrap_special(bline.formula));
        });
        */

        move_made = false; // Flag to check if any move was made in this iteration

        // Level 1 of the Waterfall
        // ------------------------

        for (auto& [name, mod_ctx] : ctx.modules) { // for each loaded module
            for (auto& digest_entry : mod_ctx.digest) { // for each digest record
                for (auto& [mod_line_idx, main_line_idx, entry_kind] : digest_entry) { // for each theorem in record
                    tabline_t& mod_tabline = mod_ctx.tableau[mod_line_idx];

                    if (entry_kind == LIBRARY::Theorem) {
                        if (!mod_tabline.formula->is_implication()) { // library result is implication
                            if (main_line_idx != -static_cast<size_t>(1)) {
                                continue; // Skip if already loaded
                            }

                            const std::vector<std::string>& mod_consts = mod_tabline.constants1;
                            bool tab_contained = consts_subset(tabc, mod_consts);
                            
                            if (tab_contained) {
                                load_theorem(ctx, mod_tabline, main_line_idx, LIBRARY::Theorem);

#if DEBUG_MOVES
                                std::cout << "Level 1: load " << main_line_idx + 1 << std::endl;
#endif

                                move_made = true;
                            }
                        }
                    }
                }
            }
        }

        if (move_made) {
            // After applying the move, run cleanup_moves automatically
            cleanup_moves(ctx, ctx.upto);

            // Check if done
            if (check_done(ctx)) {
                return true;
            }

            continue; // A move was made; restart the waterfall from the beginning
        }


        // Level 2 of the Waterfall (non-library backwards reasoning)
        // ----------------------------------------------------------

        // Access the current leaf hydra (last hydra in the current_hydra path)
        std::shared_ptr<hydra> current_leaf = ctx.current_hydra.back();

        // Extract target indices from the current leaf hydra
        std::vector<int> targets = current_leaf->target_indices;

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
            // Iterate over each implication hypothesis
            for (const size_t impl_idx : impls) {
                const tabline_t& target_tabline = ctx.tableau[target];
                const std::vector<std::string>& target_consts = target_tabline.constants1;
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
                    move_success = move_mpt(ctx, impl_idx, other_lines, specials, true, true); // ponens=true, silent=true

#if DEBUG_MOVES
                    if (move_success) {
                        std::cout << "Level 2: mp " << impl_idx + 1 << " " << target + 1 << std::endl << std::endl;
                    }
#endif
                }

                if (!move_success && all_contained_left && consts_ltor && impl_tabline.ltor) {
                    // Attempt Modus Tollens since Modus Ponens failed
                    move_success = move_mpt(ctx, impl_idx, other_lines, specials, false, true); // ponens=false, silent=true

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
            // Iterate over each implication hypothesis
            for (const size_t impl_idx : impls) {
                const tabline_t& unit_tabline = ctx.tableau[unit_idx];
                const std::vector<std::string>& unit_consts = unit_tabline.constants1;
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
                bool consts_ltor = consts_subset(impl_consts1, impl_consts2) || !consts_subset(impl_consts2, impl_consts1);
                bool consts_rtol = consts_subset(impl_consts2, impl_consts1) || !consts_subset(impl_consts1, impl_consts2);
                
                // Prepare the list of other lines (only the unit in this case)
                std::vector<int> other_lines = { unit_idx };
                bool move_success = false;
                
                if (all_contained_left && consts_ltor && impl_tabline.ltor && impl_tabline.ltor_safe) {
                    // Attempt Modus Ponens
                    move_success = move_mpt(ctx, impl_idx, other_lines, specials, true, true); // ponens=true, silent=true
                    
#if DEBUG_MOVES
                    if (move_success) {
                        std::cout << "Level 3: mp " << impl_idx + 1 << " " << unit_idx + 1 << std::endl << std::endl;
                    }
#endif
                }

                if (!move_success && all_contained_right && consts_rtol && impl_tabline.rtol && impl_tabline.rtol_safe) {
                    // Attempt Modus Tollens since Modus Ponens failed
                    move_success = move_mpt(ctx, impl_idx, other_lines, specials, false, true); // ponens=false, silent=true

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

        // Level 4 of the Waterfall (tableau/disjunction splitting)
        // ----------------------------------------------------------

        for (const size_t impl_idx : impls) {
            tabline_t& impl_tabline = ctx.tableau[impl_idx];
            node* impl = impl_tabline.formula;
            
            // Check if already split
            if (impl_tabline.split) {
                continue;
            }

            // Get common metavariables
            std::set<std::string> common_vars = find_common_variables(impl->children[0], impl->children[1]);

            // If there are shared metavars, skip this case
            if (!common_vars.empty()) {
                continue;
            }

            bool move_success = move_sd(ctx, impl_idx);

            if (move_success) {
#if DEBUG_MOVES
                std::cout << "Level 4: split " << impl_idx + 1 << std::endl << std::endl;
#endif
                // Cleanup
                cleanup_moves(ctx, ctx.upto);

                // Check if the proof is done after applying the move
                bool done = check_done(ctx, true); // apply_cleanup=true
                if (done) {
                    return true; // Proof completed successfully
                }
                    
                move_made = true;

                break;
            }

        }

        if (move_made) {
            continue; // Move made at this level, restart waterfall at level 1
        }

        // Level 6 of the Waterfall (safe target expansion)
        // ----------------------------------------------------------

        std::shared_ptr<hydra> current_leaf_hydra = ctx.current_hydra.back();

        // Iterate over each current target
        for (const int tar_idx : current_leaf_hydra->target_indices) {       
            for (auto& [name, mod_ctx] : ctx.modules) { // for each loaded module
                for (auto& digest_entry : mod_ctx.digest) { // for each digest record
                    size_t entry = -static_cast<size_t>(1);
                    for (auto& [mod_line_idx, main_line_idx, entry_kind] : digest_entry) { // for each theorem in record
                        // Index of entry in record
                        entry++;
                        
                        tabline_t& tar_tabline = ctx.tableau[tar_idx];
                        const std::vector<std::string>& tar_consts = tar_tabline.constants1;
                        tabline_t& mod_tabline = mod_ctx.tableau[mod_line_idx];

                        if (entry_kind == LIBRARY::Definition) {
                            if (mod_tabline.formula->is_implication()) { // library result is implication
                                // Check if this theorem has been applied already
                                std::pair<std::string, size_t> mod_pair = {name, mod_line_idx};
                                if (std::find(tar_tabline.lib_applied.begin(), tar_tabline.lib_applied.end(), mod_pair) != tar_tabline.lib_applied.end()) {
                                    continue; // Skip if already applied
                                }

                                const std::vector<std::string>& mod_consts1 = mod_tabline.constants1;
                                const std::vector<std::string>& mod_consts2 = mod_tabline.constants2;
                                bool all_contained_left = consts_subset(tar_consts, mod_consts1);
                                bool all_contained_right = consts_subset(tar_consts, mod_consts2);
                                bool tar_contained_left = consts_subset(tarc, mod_consts1);
                                bool tar_contained_right = consts_subset(tarc, mod_consts2);
                                bool failed_left = false;
                                bool failed_right = false;

                                // Check if all left constants are contained and conditions for Modus Ponens are met
                                if (entry != 1 || !all_contained_right){
                                    failed_left = true;
                                }

                                if (!failed_left && (tar_contained_right || units.empty())) {
                                    // Perform trial unification for Modus Ponens
                                    bool trial_mp_success = trial_modus_ponens(ctx, mod_tabline, tar_tabline, false);

                                    if (trial_mp_success) {
                                        // Load the theorem into the main tableau
                                        load_theorem(ctx, mod_tabline, main_line_idx, LIBRARY::Definition);

                                        // Attempt to apply Modus Ponens
                                        std::vector<int> other_lines = {tar_idx};
                                        bool move_success = move_mpt(ctx, main_line_idx, other_lines, specials, true, true); // ponens=true, silent=true
                                        
                                        if (move_success) {
#if DEBUG_MOVES
                                            std::cout << "Level 6: mp " << main_line_idx + 1 << " " << tar_idx + 1 << std::endl << std::endl;
#endif

                                            move_made = true;

                                            tar_tabline.lib_applied.push_back(mod_pair);
                                            
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
                                }

                                // Check if all right constants are contained and conditions for Modus Tollens are met
                                if (entry != 0 || !all_contained_left){
                                    failed_right = true;
                                }

                                if (failed_left && !failed_right && (tar_contained_left || units.empty())) {
                                    // Perform trial unification for Modus Tollens
                                    bool trial_mt_success = trial_modus_tollens(ctx, mod_tabline, tar_tabline, false);

                                    if (trial_mt_success) {
                                        // Load the theorem into the main tableau
                                        load_theorem(ctx, mod_tabline, main_line_idx, LIBRARY::Definition);

                                        // Attempt to apply Modus Tollens
                                        std::vector<int> other_lines = {tar_idx};
                                        bool move_success = move_mpt(ctx, main_line_idx, other_lines, specials, false, true); // ponens=false, silent=true

                                        if (move_success) {
#if DEBUG_MOVES
                                            std::cout << "Level 6: mt " << main_line_idx + 1 << " " << tar_idx + 1 << std::endl << std::endl;
#endif

                                            move_made = true;

                                            tar_tabline.lib_applied.push_back(mod_pair);
                                            
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
                                }

                                // Mark theorem as applied if both trials failed
                                if (failed_left && failed_right) {
                                    tar_tabline.lib_applied.push_back(mod_pair);
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

        // Level 7 of the Waterfall (safe hypothesis expansion)
        // ----------------------------------------------------------
        
        // Iterate over each unit in the units list
        for (const int unit_idx : units) {
            for (auto& [name, mod_ctx] : ctx.modules) { // for each loaded module
                for (auto& digest_entry : mod_ctx.digest) { // for each digest record
                    size_t entry = -static_cast<size_t>(1);
                    for (auto& [mod_line_idx, main_line_idx, entry_kind] : digest_entry) { // for each theorem in record
                        // Index of entry within record
                        entry++;
                        
                        tabline_t& unit_tabline = ctx.tableau[unit_idx];
                        const std::vector<std::string>& unit_consts = unit_tabline.constants1;
                        tabline_t& mod_tabline = mod_ctx.tableau[mod_line_idx];

                        if (entry_kind == LIBRARY::Definition) {
                            if (mod_tabline.formula->is_implication()) { // library result is implication
                                // Check if this theorem has been applied already
                                std::pair<std::string, size_t> mod_pair = {name, mod_line_idx};
                                if (std::find(unit_tabline.lib_applied.begin(), unit_tabline.lib_applied.end(), mod_pair) != unit_tabline.lib_applied.end()) {
                                    continue; // Skip if already applied
                                }

                                const std::vector<std::string>& mod_consts1 = mod_tabline.constants1;
                                const std::vector<std::string>& mod_consts2 = mod_tabline.constants2;
                                bool all_contained_left = consts_subset(unit_consts, mod_consts1);
                                bool all_contained_right = consts_subset(unit_consts, mod_consts2);
                                bool tab_contained_left = consts_subset(tabc, mod_consts1);
                                bool tab_contained_right = consts_subset(tabc, mod_consts2);
                                bool failed_left = false;
                                bool failed_right = false;

                                // Check if all left constants are contained and conditions for Modus Ponens are met
                                if (entry != 0 || !all_contained_left){
                                    failed_left = true;
                                }

                                if (!failed_left && tab_contained_left) {
                                    // Perform trial unification for Modus Ponens
                                    bool trial_mp_success = trial_modus_ponens(ctx, mod_tabline, unit_tabline, true);

                                    if (trial_mp_success) {
                                        load_theorem(ctx, mod_tabline, main_line_idx, LIBRARY::Definition);

                                        // Attempt to apply Modus Ponens
                                        std::vector<int> other_lines = {unit_idx};
                                        bool move_success = move_mpt(ctx, main_line_idx, other_lines, specials, true, true); // ponens=true, silent=true

                                        if (move_success) {
        #if DEBUG_MOVES
                                            std::cout << "Level 7: mp " << main_line_idx + 1 << " " << unit_idx + 1 << std::endl << std::endl;
        #endif
                                            move_made = true;

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
                                }

                                // Check if all right constants are contained and conditions for Modus Tollens are met
                                if (entry != 1 || !all_contained_right){
                                    failed_right = true;
                                }

                                if (failed_left && !failed_right && tab_contained_right) {
                                    // Perform trial unification for Modus Tollens
                                    bool trial_mt_success = trial_modus_tollens(ctx, mod_tabline, unit_tabline, true);

                                    if (trial_mt_success) {
                                        // Load the theorem into the main tableau
                                        load_theorem(ctx, mod_tabline, main_line_idx, LIBRARY::Definition);
                                        
                                        // Attempt to apply Modus Tollens
                                        std::vector<int> other_lines = {unit_idx};
                                        bool move_success = move_mpt(ctx, main_line_idx, other_lines, specials, false, true); // ponens=false, silent=true

                                        if (move_success) {
        #if DEBUG_MOVES
                                            std::cout << "Level 7: mt " << main_line_idx + 1 << " " << unit_idx + 1 << std::endl << std::endl;
        #endif
                                            move_made = true;

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
                                }

                                // Mark theorem as applied if both trials failed
                                if (failed_left && failed_right) {
                                    unit_tabline.lib_applied.push_back(mod_pair);
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
        }

        if (move_made) { 
            continue; // Move made at this level, restart waterfall at level 1
        }

        // Level 9 of the Waterfall (Library forwards reasoning)
        // ----------------------------------------------------------

        // Iterate over each unit in the units list
        for (const int unit_idx : units) {
            for (auto& [name, mod_ctx] : ctx.modules) { // for each loaded module
                for (auto& digest_entry : mod_ctx.digest) { // for each digest record
                    for (auto& [mod_line_idx, main_line_idx, entry_kind] : digest_entry) { // for each theorem in record
                        tabline_t& unit_tabline = ctx.tableau[unit_idx];
                        const std::vector<std::string>& unit_consts = unit_tabline.constants1;
                        tabline_t& mod_tabline = mod_ctx.tableau[mod_line_idx];

                        if (entry_kind == LIBRARY::Theorem) {
                            if (mod_tabline.formula->is_implication()) { // library result is implication
                                // Check if this theorem has been applied already
                                std::pair<std::string, size_t> mod_pair = {name, mod_line_idx};
                                if (std::find(unit_tabline.lib_applied.begin(), unit_tabline.lib_applied.end(), mod_pair) != unit_tabline.lib_applied.end()) {
                                    continue; // Skip if already applied
                                }

                                auto[vars_ltor, vars_rtol] = metavar_check(mod_tabline);
                                const std::vector<std::string>& mod_consts1 = mod_tabline.constants1;
                                const std::vector<std::string>& mod_consts2 = mod_tabline.constants2;
                                bool all_contained_left = consts_subset(unit_consts, mod_consts1);
                                bool all_contained_right = consts_subset(unit_consts, mod_consts2);
                                bool tab_contained_left = consts_subset(tabc, mod_consts1);
                                bool tab_contained_right = consts_subset(tabc, mod_consts2);
                                bool consts_ltor = consts_subset(mod_consts1, mod_consts2) || !consts_subset(mod_consts2, mod_consts1);
                                bool consts_rtol = consts_subset(mod_consts2, mod_consts1) || !consts_subset(mod_consts1, mod_consts2);
                                bool failed_left = false;
                                bool failed_right = false;

                                // Check if all left constants are contained and conditions for Modus Ponens are met
                                if (!all_contained_left || !consts_ltor || !vars_ltor){
                                    failed_left = true;
                                }

                                if (!failed_left && tab_contained_left) {
                                    // Perform trial unification for Modus Ponens
                                    bool trial_mp_success = trial_modus_ponens(ctx, mod_tabline, unit_tabline, true);

                                    if (trial_mp_success) {
                                        // Load the theorem into the main tableau
                                        load_theorem(ctx, mod_tabline, main_line_idx, LIBRARY::Theorem);

                                        // Attempt to apply Modus Ponens
                                        std::vector<int> other_lines = {unit_idx};
                                        bool move_success = move_mpt(ctx, main_line_idx, other_lines, specials, true, true); // ponens=true, silent=true

                                        if (move_success) {
#if DEBUG_MOVES
                                            std::cout << "Level 9: mp " << main_line_idx + 1 << " " << unit_idx + 1 << std::endl << std::endl;
#endif
                                            move_made = true;

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
                                }

                                // Check if all right constants are contained and conditions for Modus Tollens are met
                                if (!all_contained_right || !consts_rtol || !vars_rtol){
                                    failed_right = true;
                                }

                                if (failed_left && !failed_right && tab_contained_right) {
                                    // Perform trial unification for Modus Tollens
                                    bool trial_mt_success = trial_modus_tollens(ctx, mod_tabline, unit_tabline, true);

                                    if (trial_mt_success) {
                                        // Load the theorem into the main tableau
                                        load_theorem(ctx, mod_tabline, main_line_idx, LIBRARY::Theorem);

                                        // Attempt to apply Modus Tollens
                                        std::vector<int> other_lines = {unit_idx};
                                        bool move_success = move_mpt(ctx, main_line_idx, other_lines, specials, false, true); // ponens=false, silent=true

                                        if (move_success) {
#if DEBUG_MOVES
                                            std::cout << "Level 9: mt " << main_line_idx + 1 << " " << unit_idx + 1 << std::endl << std::endl;
#endif
                                            move_made = true;

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
                                }

                                // Mark theorem as applied if both trials failed
                                if (failed_left && failed_right) {
                                    unit_tabline.lib_applied.push_back(mod_pair);
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

        // Level 10 of the Waterfall (Library backwards reasoning)
        // ----------------------------------------------------------

        current_leaf_hydra = ctx.current_hydra.back();

        // Iterate over each current target
        for (const int tar_idx : current_leaf_hydra->target_indices) {
            for (auto& [name, mod_ctx] : ctx.modules) { // for each loaded module
                for (auto& digest_entry : mod_ctx.digest) { // for each digest record
                    for (auto& [mod_line_idx, main_line_idx, entry_kind] : digest_entry) { // for each theorem in record
                        tabline_t& tar_tabline = ctx.tableau[tar_idx];
                        const std::vector<std::string>& tar_consts = tar_tabline.constants1;
                        tabline_t& mod_tabline = mod_ctx.tableau[mod_line_idx];

                        if (entry_kind == LIBRARY::Theorem) {
                            if (mod_tabline.formula->is_implication()) { // library result is implication
                                // Check if this theorem has been applied already
                                std::pair<std::string, size_t> mod_pair = {name, mod_line_idx};
                                if (std::find(tar_tabline.lib_applied.begin(), tar_tabline.lib_applied.end(), mod_pair) != tar_tabline.lib_applied.end()) {
                                    continue; // Skip if already applied
                                }

                                auto[vars_ltor, vars_rtol] = metavar_check(mod_tabline);
                                const std::vector<std::string>& mod_consts1 = mod_tabline.constants1;
                                const std::vector<std::string>& mod_consts2 = mod_tabline.constants2;
                                bool all_contained_left = consts_subset(tar_consts, mod_consts1);
                                bool all_contained_right = consts_subset(tar_consts, mod_consts2);
                                bool tar_contained_left = consts_subset(tarc, mod_consts1);
                                bool tar_contained_right = consts_subset(tarc, mod_consts2);
                                bool consts_ltor = consts_subset(mod_consts1, mod_consts2) || !consts_subset(mod_consts2, mod_consts1);
                                bool consts_rtol = consts_subset(mod_consts2, mod_consts1) || !consts_subset(mod_consts1, mod_consts2);
                                bool failed_left = false;
                                bool failed_right = false;

                                // Check if all left constants are contained and conditions for Modus Ponens are met
                                if (!all_contained_right || !consts_rtol || !vars_rtol){
                                    failed_left = true;
                                }

                                if (!failed_left && (tar_contained_right || units.empty())) {
                                    // Perform trial unification for Modus Ponens
                                    bool trial_mp_success = trial_modus_ponens(ctx, mod_tabline, tar_tabline, false);

                                    if (trial_mp_success) {
                                        // Load the theorem into the main tableau
                                        load_theorem(ctx, mod_tabline, main_line_idx, LIBRARY::Theorem);

                                        // Attempt to apply Modus Ponens
                                        std::vector<int> other_lines = {tar_idx};
                                        bool move_success = move_mpt(ctx, main_line_idx, other_lines, specials, true, true); // ponens=true, silent=true

                                        if (move_success) {
#if DEBUG_MOVES
                                            std::cout << "Level 10: mp " << main_line_idx + 1 << " " << tar_idx + 1 << std::endl << std::endl;
#endif

                                            move_made = true;

                                            tar_tabline.lib_applied.push_back(mod_pair);
                                            
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
                                }

                                // Check if all right constants are contained and conditions for Modus Tollens are met
                                if (!all_contained_left || !consts_ltor || !vars_ltor){
                                    failed_right = true;
                                }

                                if (failed_left && !failed_right && (tar_contained_left || units.empty())) {
                                    // Perform trial unification for Modus Tollens
                                    bool trial_mt_success = trial_modus_tollens(ctx, mod_tabline, tar_tabline, false);

                                    if (trial_mt_success) {
                                        // Load the theorem into the main tableau
                                        load_theorem(ctx, mod_tabline, main_line_idx, LIBRARY::Theorem);

                                        // Attempt to apply Modus Tollens
                                        std::vector<int> other_lines = {tar_idx};
                                        bool move_success = move_mpt(ctx, main_line_idx, other_lines, specials, false, true); // ponens=false, silent=true

                                        if (move_success) {
#if DEBUG_MOVES
                                            std::cout << "Level 10: mt " << main_line_idx + 1 << " " << tar_idx + 1 << std::endl << std::endl;
#endif

                                            move_made = true;

                                            tar_tabline.lib_applied.push_back(mod_pair);
                                            
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
                                }

                                // Mark theorem as applied if both trials failed
                                if (failed_left && failed_right) {
                                    tar_tabline.lib_applied.push_back(mod_pair);
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

        // Level Extra of the Waterfall (unsafe non-library forwards reasoning)
        // ----------------------------------------------------------
        
        // Iterate over each unit in the units list
        for (const int unit_idx : units) {
            // Iterate over each implication hypothesis
            for (const size_t impl_idx : impls) {
                const tabline_t& unit_tabline = ctx.tableau[unit_idx];
                const std::vector<std::string>& unit_consts = unit_tabline.constants1;
                tabline_t& impl_tabline = ctx.tableau[impl_idx];
                const std::vector<std::string>& impl_consts1 = impl_tabline.constants1;
                const std::vector<std::string>& impl_consts2 = impl_tabline.constants2;

                // Check if the implication has already been applied to this unit
                if (std::find(impl_tabline.applied_units.begin(), impl_tabline.applied_units.end(), unit_idx) != impl_tabline.applied_units.end()) {
                    continue; // Skip if already applied
                }

                // Check if all unit constants are contained within implication constants
                bool all_contained_left = consts_subset(unit_consts, impl_consts1);
                bool all_contained_right = consts_subset(unit_consts, impl_consts2);
                
                // Prepare the list of other lines (only the unit in this case)
                std::vector<int> other_lines = { unit_idx };
                bool move_success = false;
                
                if (all_contained_left && impl_tabline.ltor) {
                    // Attempt Modus Ponens
                    move_success = move_mpt(ctx, impl_idx, other_lines, specials, true, true); // ponens=true, silent=true

#if DEBUG_MOVES
                    if (move_success) {
                        std::cout << "Level Extra: mp " << impl_idx + 1 << " " << unit_idx + 1 << std::endl << std::endl;
                    }
#endif
                }

                if (!move_success && all_contained_right && impl_tabline.rtol) {
                    // Attempt Modus Tollens since Modus Ponens failed
                    move_success = move_mpt(ctx, impl_idx, other_lines, specials, false, true); // ponens=false, silent=true

#if DEBUG_MOVES
                    if (move_success) {
                        std::cout << "Level Extra: mt " << impl_idx + 1 << " " << unit_idx + 1 << std::endl << std::endl;
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
        
        // Level Extra 2 of the Waterfall (unsafe non-library backwards reasoning)
        // ----------------------------------------------------------

        // Access the current leaf hydra (last hydra in the current_hydra path)
        current_leaf = ctx.current_hydra.back();

        // Extract target indices from the current leaf hydra
        targets = current_leaf->target_indices;

        // Iterate over each target in the current leaf hydra
        for (const int target : targets) {
            // Iterate over each implication hypothesis
            for (const size_t impl_idx : impls) {
                const tabline_t& target_tabline = ctx.tableau[target];
                const std::vector<std::string>& target_consts = target_tabline.constants1;
                tabline_t& impl_tabline = ctx.tableau[impl_idx];
                const std::vector<std::string>& impl_consts1 = impl_tabline.constants1;
                const std::vector<std::string>& impl_consts2 = impl_tabline.constants2;

                // Check if the implication has already been applied to this target
                if (std::find(impl_tabline.applied_units.begin(), impl_tabline.applied_units.end(), target) != impl_tabline.applied_units.end()) {
                    continue; // Skip if already applied
                }

                // Check if all implication constants are contained within target constants
                bool all_contained_left = consts_subset(target_consts, impl_consts1);
                bool all_contained_right = consts_subset(target_consts, impl_consts2);
                
                // Prepare the list of other lines (only the target in this case)
                std::vector<int> other_lines = { target };
                bool move_success = false;

                if (all_contained_right && impl_tabline.rtol) {
                    // Attempt Modus Ponens
                    move_success = move_mpt(ctx, impl_idx, other_lines, specials, true, true); // ponens=true, silent=true

#if DEBUG_MOVES
                    if (move_success) {
                        std::cout << "Level Extra 2: mp " << impl_idx + 1 << " " << target + 1 << std::endl << std::endl;
                    }
#endif
                }

                if (!move_success && all_contained_left  && impl_tabline.ltor) {
                    // Attempt Modus Tollens since Modus Ponens failed
                    move_success = move_mpt(ctx, impl_idx, other_lines, specials, false, true); // ponens=false, silent=true

#if DEBUG_MOVES
                    if (move_success) {
                        std::cout << "Level Extra 2: mt " << impl_idx + 1 << " " << target + 1 << std::endl << std::endl;
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
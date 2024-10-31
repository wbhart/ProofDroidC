// automation.cpp

#include "automation.h"

// Automation using a waterfall architecture
// Returns true if theorem successfully proved, else false if the automation gets stuck
bool automate(context_t& ctx) {
    // Declare vectors to hold constants and hypothesis indices
    std::vector<std::string> tabc;              // All constants from active lines that are not theorems or definitions
    std::vector<std::string> tarc;              // All constants from active target lines
    std::vector<size_t> impls;                  // Indices of active implication hypotheses
    std::vector<size_t> units;                  // Indices of active non-implication hypotheses

    // Accumulate constants and indices using get_tableau_constants
    ctx.get_tableau_constants(tabc, tarc, impls, units);

    bool move_made; // whether a move was made at any step

    // Waterfall Architecture Loop
    while (true) {
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

        // Iterate over each target in the current leaf hydra
        for (const int target : targets) {
            const tabline_t& target_tabline = ctx.tableau[target];
            const std::vector<std::string>& target_consts = target_tabline.constants;

            // Iterate over each implication hypothesis
            for (const size_t impl_idx : impls) {
                tabline_t& impl_tabline = ctx.tableau[impl_idx];
                const std::vector<std::string>& impl_consts = impl_tabline.constants;

                // Check if the implication has already been applied to this target
                if (std::find(impl_tabline.applied_units.begin(), impl_tabline.applied_units.end(), target) != impl_tabline.applied_units.end()) {
                    continue; // Skip if already applied
                }

                // Check if all target constants are contained within implication constants
                bool all_contained = true;
                for (const auto& const_str : target_consts) {
                    if (std::find(impl_consts.begin(), impl_consts.end(), const_str) == impl_consts.end()) {
                        all_contained = false;
                        break;
                    }
                }

                if (all_contained) {
                    // Prepare the list of other lines (only the target in this case)
                    std::vector<int> other_lines = { target };

                    // Attempt Modus Ponens
                    bool move_success = move_mpt(ctx, impl_idx, other_lines, true, true); // ponens=true, silent=true

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
                    else {
                        // Attempt Modus Tollens since Modus Ponens failed
                        move_success = move_mpt(ctx, impl_idx, other_lines, false, true); // ponens=false, silent=true

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
            // No moves were made at any level; automation cannot proceed further
            return false;
        }

        // Continue the waterfall loop
    }

    // This point is never reached due to the loop's structure
    return false;
}
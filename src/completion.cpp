// completion.cpp

#include "completion.h"
#include "unify.h"
#include "substitute.h"
#include "context.h"
#include "hydra.h"
#include <iostream>
#include <unordered_set>
#include <algorithm>
#include <functional>
#include <optional>
#include <stack>
#include <vector>
#include <string>

// Define DEBUG_STEP_2 to enable debug traces for Step 2
#define DEBUG_STEP_2 0

bool check_done(context_t& ctx) {
    // Step 1: Negate formulas of all non-target lines starting from 'upto'
    for (int j = ctx.upto; j < static_cast<int>(ctx.tableau.size()); ++j) {
        tabline_t& current_line = ctx.tableau[j];
        if (!current_line.target) {
            current_line.negation = negate_node(current_line.formula);
        }
    }

    // Step 2: compute potential unifications (incremental, from upto)
    for (int j = ctx.upto; j < static_cast<int>(ctx.tableau.size()); ++j) {
        tabline_t& current_line = ctx.tableau[j];

        // **Ensure the current line is not dead**
        if (current_line.dead) {
            continue; // Skip dead lines
        }

        // **Determine if the current line is a target**
        bool is_current_target = current_line.target;

#if DEBUG_STEP_2
        std::cout << "Processing Current Line: " << j 
                  << (is_current_target ? " [Target]" : " [Hypothesis]") << "\n";
        std::cout << "  Restrictions: ";
        for (const auto& res : current_line.restrictions) {
            std::cout << res << " ";
        }
        std::cout << "\n  Assumptions: ";
        for (const auto& assm : current_line.assumptions) {
            std::cout << assm << " ";
        }
        std::cout << "\n";
#endif

        for (int i = 0; i < j; ++i) {
            tabline_t& previous_line = ctx.tableau[i];

            // **Skip dead previous lines**
            if (previous_line.dead) {
                continue;
            }

            // **If current_line is a target, only unify with non-dead hypotheses**
            if (is_current_target && previous_line.target) {
                continue; // Skip if previous_line is also a target
            }

#if DEBUG_STEP_2
            std::cout << "  Checking against Previous Line: " << i 
                      << (previous_line.target ? " [Target]" : " [Hypothesis]") << "\n";
            std::cout << "    Previous Restrictions: ";
            for (const auto& res : previous_line.restrictions) {
                std::cout << res << " ";
            }
            std::cout << "\n    Previous Assumptions: ";
            for (const auto& assm : previous_line.assumptions) {
                std::cout << assm << " ";
            }
            std::cout << "\n";
#endif

            // **Compatibility Checks**
            bool restrictions_ok = restrictions_compatible(current_line.restrictions, previous_line.restrictions);
            bool assumptions_ok = assumptions_compatible(current_line.assumptions, previous_line.assumptions);

#if DEBUG_STEP_2
            std::cout << "    Restrictions Compatible: " << (restrictions_ok ? "Yes" : "No") << "\n";
            std::cout << "    Assumptions Compatible: " << (assumptions_ok ? "Yes" : "No") << "\n";
#endif

            if (restrictions_ok && assumptions_ok) {
                Substitution subst;

                std::optional<Substitution> result = unify(current_line.negation, previous_line.formula, subst);
                if (result.has_value()) {
#if DEBUG_STEP_2
                    std::cout << "    Unification Successful between Line " << j 
                              << " and Line " << i << "\n";
#endif
                    // **Determine where to append the unification pair (i, j)**
                    if (is_current_target) {
                        // Current line is a target: append to current_line.unifications
                        current_line.unifications.emplace_back(i, j);
#if DEBUG_STEP_2
                        std::cout << "      Appended (" << i << ", " << j << ") to Current Target's Unifications.\n";
#endif
                    }
                    else if (previous_line.target) {
                        // Previous line is a target: append to previous_line.unifications
                        previous_line.unifications.emplace_back(i, j);
#if DEBUG_STEP_2
                        std::cout << "      Appended (" << i << ", " << j << ") to Previous Target's Unifications.\n";
#endif
                    }
                    else {
                        // Both lines are hypotheses: append to relevant targets based on combined restrictions
                        std::vector<int> combined_targets = combine_restrictions(current_line.restrictions, previous_line.restrictions);
#if DEBUG_STEP_2
                        std::cout << "      Combined Targets from Restrictions: ";
                        if (combined_targets.empty()) {
                            std::cout << "None (Appending to All Non-Dead Targets)\n";
                        } else {
                            for (const auto& ct : combined_targets) {
                                std::cout << ct << " ";
                            }
                            std::cout << "\n";
                        }
#endif
                        if (combined_targets.empty()) {
                            // If combined_targets is empty, append to all non-dead targets
                            for (size_t t = 0; t < ctx.tableau.size(); ++t) {
                                tabline_t& target_line = ctx.tableau[t];
                                if (target_line.target && !target_line.dead) {
                                    target_line.unifications.emplace_back(i, j);
#if DEBUG_STEP_2
                                    std::cout << "        Appended (" << i << ", " << j 
                                              << ") to Target Line " << t << "'s Unifications.\n";
#endif
                                }
                            }
                        }
                        else {
                            // Append to specific targets based on combined restrictions
                            for (int target_idx : combined_targets) {
                                // **Verify that target_idx is indeed a target and not out of bounds**
                                if (target_idx >= 0 && target_idx < static_cast<int>(ctx.tableau.size())) {
                                    tabline_t& target_line = ctx.tableau[target_idx];
                                    if (target_line.target) {
                                        target_line.unifications.emplace_back(i, j);
#if DEBUG_STEP_2
                                        std::cout << "        Appended (" << i << ", " << j 
                                                  << ") to Target Line " << target_idx << "'s Unifications.\n";
#endif
                                    }
                                    else {
                                        std::cerr << "Warning: Line " << target_idx 
                                                  << " is not a target but was identified as one based on combined restrictions.\n";
                                    }
                                }
                                else {
                                    std::cerr << "Error: Combined target index " << target_idx << " is out of bounds.\n";
                                }
                            }
                        }
                    }
                }
#if DEBUG_STEP_2
                else {
                    std::cout << "    Unification Failed between Line " << j 
                              << " and Line " << i << "\n";
                }
#endif
            }
#if DEBUG_STEP_2
            else {
                std::cout << "    Skipping Unification between Line " << j 
                          << " and Line " << i << " due to incompatible restrictions or assumptions.\n";
            }
#endif
        }
    }

    // Step 3: update upto
    ctx.upto = static_cast<int>(ctx.tableau.size());

    // Step 4: Check that each target in the current leaf hydra has at least one unification pair
    if (ctx.current_hydra.empty()) {
        std::cerr << "Error: current_hydra is empty.\n";
        return false;
    }

    // Access the current leaf hydra (last entry in current_hydra)
    hydra& current_leaf_hydra = ctx.current_hydra.back().get();
    const std::vector<int>& target_indices = current_leaf_hydra.target_indices;

    // Ensure there is at least one target
    if (target_indices.empty()) {
        std::cerr << "Error: No targets in the current leaf hydra.\n";
        return false;
    }

    // Collect unifications for each target
    std::vector<std::vector<std::pair<int, int>>> unifications_lists;
    unifications_lists.reserve(target_indices.size());

    for (int target_idx : target_indices) {
        const tabline_t& target_line = ctx.tableau[target_idx];
        if (target_line.unifications.empty()) {
            // Step 4 Failure: At least one target has no unifications
            std::cerr << "Error: Target line " << target_idx + 1 << " has no unifications.\n";
            return false;
        }
        unifications_lists.emplace_back(target_line.unifications);
    }

    // Step 5: Attempt simultaneous unifications across all targets
    // We need to iterate over all possible tuples where each tuple contains one unification from each target
    // and attempt to perform a simultaneous unification using a common substitution,
    // ensuring that the assumptions of the hypotheses in the tuple are compatible.

    // Number of targets
    size_t num_targets = unifications_lists.size();

    // Flag to indicate if a successful simultaneous unification is found
    bool simultaneous_unification_found = false;

    // Variable to store the merged assumptions from the successful tuple
    std::vector<int> successful_merged_assumptions;

    // Recursively attempts to find a simultaneous unification across all targets.
    std::function<void(size_t, Substitution, std::vector<int>&)> recurse = [&](size_t depth, Substitution current_subst, std::vector<int>& merged_assumptions_ref) {
        if (simultaneous_unification_found) {
            return; // Early exit if already found
        }

        if (depth == num_targets) {
            // All targets have been processed successfully
            simultaneous_unification_found = true;
            successful_merged_assumptions = merged_assumptions_ref;
            return;
        }

        // Iterate through all unifications for the current target
        for (size_t k = 0; k < unifications_lists[depth].size(); ++k) {
            const std::pair<int, int>& unif_pair = unifications_lists[depth][k];
            int first_line_idx = unif_pair.first;
            int second_line_idx = unif_pair.second;

            const tabline_t& first_line = ctx.tableau[first_line_idx];
            const tabline_t& second_line = ctx.tableau[second_line_idx];

            // Determine which line is a hypothesis (target == false)
            std::vector<int> hypothesis_assumptions;

            if (!first_line.target && second_line.target) {
                // Case 1: (i is hypothesis, j is target)
                hypothesis_assumptions = first_line.assumptions;
            }
            else if (!second_line.target && first_line.target) {
                // Case 2: (i is target, j is hypothesis)
                hypothesis_assumptions = second_line.assumptions;
            }
            else if (!first_line.target && !second_line.target) {
                // Case 3: Both lines are hypotheses
                // Attempt to merge their assumptions within the pair
                // First, check compatibility between the two sets
                if (!assumptions_compatible(first_line.assumptions, second_line.assumptions)) {
                    std::cerr << "Error: Incompatible assumptions within pair (" << first_line_idx << ", " << second_line_idx << ").\n";
                    continue; // Skip this unification pair
                }
                // If compatible, merge them
                hypothesis_assumptions = merge_assumptions(first_line.assumptions, second_line.assumptions);
            }
            else {
                // Case 4: Both lines are targets or neither is a hypothesis; invalid pair
                std::cerr << "Error: Invalid unification pair (" << first_line_idx << ", " << second_line_idx << ") where neither or both are hypotheses.\n";
                continue; // Skip this unification pair
            }

            // Check compatibility with the running tally
            if (!assumptions_compatible(merged_assumptions_ref, hypothesis_assumptions)) {
                // Incompatible assumptions, skip this unification pair
                continue;
            }

            // Merge the new assumptions into the running tally
            std::vector<int> updated_merged_assumptions = merge_assumptions(merged_assumptions_ref, hypothesis_assumptions);

            // Attempt to unify the target's negated formula with the hypothesis's formula
            node* target_negation = second_line.target ? second_line.negation : first_line.negation;
            node* hypothesis_formula = second_line.target ? first_line.formula : second_line.formula;

            Substitution new_subst = current_subst; // Copy current substitution

            std::optional<Substitution> unif_result = unify(target_negation, hypothesis_formula, new_subst);
            if (unif_result.has_value()) {
                // Continue to the next target with the updated substitution and merged assumptions
                recurse(depth + 1, unif_result.value(), updated_merged_assumptions);

                if (simultaneous_unification_found) {
                    return; // Early exit if found
                }
            }
            // Else, unification failed for this pair; try next unification
        }
    };

    // Initialize substitution and merged assumptions
    Substitution initial_subst; // Start with an empty substitution
    std::vector<int> initial_assumptions; // Start with empty assumptions

    recurse(0, initial_subst, initial_assumptions);

    if (simultaneous_unification_found) {
        // Step 6: Add the merged assumptions to the current leaf hydra
        bool add_success = current_leaf_hydra.add_assumption(successful_merged_assumptions);

        if (add_success) {
            // Prepare the list of target indices as a string for the message
            std::string targets_proved = "";
            for (size_t i = 0; i < target_indices.size(); ++i) {
                targets_proved += std::to_string(target_indices[i] + 1);
                if (i != target_indices.size() - 1) {
                    targets_proved += ", ";
                }
            }

            // Print the success message
            std::cout << "Target(s) " << targets_proved << " proved.\n";

            // Set targets to not active and dead in the tableau
            for (int target_idx : target_indices) {
                ctx.tableau[target_idx].active = false;
                ctx.tableau[target_idx].dead = true;
            }

            // Store a reference to the current hydra before popping
            hydra& current_hydra_ref = ctx.current_hydra.back().get();

            // Remove the current leaf hydra from current_hydra
            ctx.current_hydra.pop_back();

            // Detach the current hydra from its parent hydra in the hydra graph
            if (!ctx.current_hydra.empty()) {
                hydra& parent_hydra_ref = ctx.current_hydra.back().get();
                auto& children = parent_hydra_ref.children;

                // Remove the child hydra from parent's children
                children.erase(std::remove_if(children.begin(), children.end(),
                    [&](const std::shared_ptr<hydra>& child_ptr) {
                        return &(*child_ptr) == &current_hydra_ref;
                    }), children.end());
            }

            // Kill all hypotheses that could only be used to prove dead targets
            ctx.purge_dead();

            // Call get_hydra() and select_targets(targets)
            std::vector<int> new_targets = ctx.get_hydra();

            if (new_targets.empty()) {
                std::cout << "All targets proved!" << std::endl;

                return true;
            }

            ctx.select_targets(new_targets);

            return false; // New targets to prove
        }
        else {
            return false; // Not fully proved yet
        }
    }
    else {
        // No simultaneous unification found
        return false;
    }
}

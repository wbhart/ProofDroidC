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
#include <queue>

#define DEBUG_STEP_2 0 // enable debug traces for Step 2
#define DEBUG_CHECK 0 // print tableaus and hydras for check_done

bool check_done(context_t& ctx, bool apply_cleanup) {
    // Step 1: Negate formulas of all non-target lines starting from 'upto'
    for (int j = ctx.upto; j < static_cast<int>(ctx.tableau.size()); ++j) {
        tabline_t& current_line = ctx.tableau[j];
        if (!current_line.target) {
            if (current_line.negation)
                delete current_line.negation;
            current_line.negation = negate_node(deep_copy(current_line.formula));
        }
    }

    // Step 2: Compute potential unifications (incremental, from upto)
    for (int j = ctx.upto; j < static_cast<int>(ctx.tableau.size()); ++j) {
        tabline_t& current_line = ctx.tableau[j];

        // Ensure the current line is not dead
        if (current_line.dead || current_line.is_theorem() || current_line.is_definition()) {
            continue; // Skip dead lines
        }

        // Determine if the current line is a target
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

            // Skip dead previous lines
            if (previous_line.dead || previous_line.is_theorem() || previous_line.is_definition()) {
                continue;
            }

            // If current_line is a target, only unify with non-dead hypotheses
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

            // Compatibility Checks
            bool restrictions_ok = restrictions_compatible(current_line.restrictions, previous_line.restrictions);
            bool assumptions_ok = assumptions_compatible(current_line.assumptions, previous_line.assumptions);

#if DEBUG_STEP_2
            std::cout << "    Restrictions Compatible: " << (restrictions_ok ? "Yes" : "No") << "\n";
            std::cout << "    Assumptions Compatible: " << (assumptions_ok ? "Yes" : "No") << "\n";
#endif

            if (restrictions_ok && assumptions_ok) {
                Substitution subst;

                std::optional<Substitution> result = unify(current_line.negation, previous_line.formula, subst, true);
                if (result.has_value()) {
#if DEBUG_STEP_2
                    std::cout << "    Unification Successful between Line " << j 
                              << " and Line " << i << "\n";
#endif
                    // Determine where to append the unification pair (i, j)
                    if (is_current_target) {
                        // Current line is a target: append to current_line.unifications
                        if (previous_line.restrictions.empty() ||
                             find(previous_line.restrictions.begin(), previous_line.restrictions.end(),
                                 j) != previous_line.restrictions.end())
                            current_line.unifications.emplace_back(i, j);
#if DEBUG_STEP_2
                        std::cout << "      Appended (" << i << ", " << j << ") to Current Target's Unifications.\n";
#endif
                    }
                    else if (previous_line.target) {
                        // Previous line is a target: append to previous_line.unifications
                        if (current_line.restrictions.empty() ||
                             find(current_line.restrictions.begin(), current_line.restrictions.end(),
                                 i) != current_line.restrictions.end())
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
                                // Verify that target_idx is indeed a target and not out of bounds
                                if (target_idx < 0 || target_idx >= static_cast<int>(ctx.tableau.size())) {
                                    std::cerr << "Error: Combined target index " << target_idx << " is out of bounds.\n";
                                    continue;
                                }
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

    // Step 3: Update 'upto'
    ctx.upto = static_cast<int>(ctx.tableau.size());

    // Step 4: Iterate over all hydras in the current_hydra list
    // We will process hydras from the current_hydra list, collect hydras to remove, and handle flags
    std::vector<std::shared_ptr<hydra>> hydras_to_remove;
    bool assumption_changed_flag = false;

    for (size_t hydra_idx = 0; hydra_idx < ctx.current_hydra.size(); ++hydra_idx) {
        std::shared_ptr<hydra> current_hydra_ptr = ctx.current_hydra[hydra_idx];
        std::vector<int> target_indices = current_hydra_ptr->target_indices;

        // Collect unifications for each target in the current hydra
        std::vector<std::vector<std::pair<int, int>>> unifications_lists;
        unifications_lists.reserve(target_indices.size());

        bool has_empty_unifications = false;
        for (int target_idx : target_indices) {
            // Bounds checking for target_idx
            if (target_idx < 0 || target_idx >= static_cast<int>(ctx.tableau.size())) {
                std::cerr << "Error: Hydra's target index " << target_idx << " is out of bounds.\n";
                has_empty_unifications = true;
                break; // Treat as having empty unifications
            }

            const tabline_t& target_line = ctx.tableau[target_idx];
            if (target_line.unifications.empty()) {
                has_empty_unifications = true;
                break; // No possible unifications, cannot prove this hydra yet
            }
            unifications_lists.emplace_back(target_line.unifications);
        }

        if (has_empty_unifications) {
            continue; // Skip this hydra as it cannot be proved without unifications
        }

        // Attempt to find simultaneous unifications for all targets in this hydra

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

            // Ensure depth is within bounds
            if (depth >= unifications_lists.size()) {
                return; // Prevent out-of-bounds access
            }
        
            // Iterate through all unifications for the current target
            for (size_t k = 0; k < unifications_lists[depth].size(); ++k) {
                const std::pair<int, int>& unif_pair = unifications_lists[depth][k];
                auto [first_line_idx, second_line_idx] = unif_pair;
                
                // Bounds checking for first_line_idx and second_line_idx
                if (first_line_idx < 0 || first_line_idx >= static_cast<int>(ctx.tableau.size()) ||
                    second_line_idx < 0 || second_line_idx >= static_cast<int>(ctx.tableau.size())) {
                    std::cerr << "Error: Unification pair (" << first_line_idx << ", " << second_line_idx << ") has out-of-bounds indices.\n";
                    continue; // Skip this unification pair
                }

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

                std::optional<Substitution> unif_result = unify(target_negation, hypothesis_formula, new_subst, true);
                if (unif_result.has_value()) {
                    if (depth + 1 == num_targets) {
                        // Check if already proved for those assumptions
                        if (!current_hydra_ptr->assumption_exists(updated_merged_assumptions)) {
                            simultaneous_unification_found = true;
                            successful_merged_assumptions = updated_merged_assumptions;
                            return;
                        }
                    }
                    else {
                        // Continue to the next target with the updated substitution and merged assumptions
                        recurse(depth + 1, unif_result.value(), updated_merged_assumptions);

                        if (simultaneous_unification_found) {
                            return; // Early exit if found
                        }
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
            // Attempt to add the merged assumptions to the current hydra and all its descendants
            std::vector<std::shared_ptr<hydra>> hydras_processed;
            std::function<void(std::shared_ptr<hydra>)> add_assumption_recursive = [&](std::shared_ptr<hydra> hydra_ptr) {
                int add_success = hydra_ptr->add_assumption(successful_merged_assumptions);
                hydras_processed.emplace_back(hydra_ptr);

                if (add_success == 1) {
                    // Hydra is now proved unconditionally

                    // Inform the user
                    std::string targets_proved = "";
                    for (size_t i = 0; i < hydra_ptr->target_indices.size(); ++i) {
                        targets_proved += std::to_string(hydra_ptr->target_indices[i] + 1); // Assuming line numbers start at 1
                        if (i != hydra_ptr->target_indices.size() - 1) {
                            targets_proved += ", ";
                        }
                    }

                    // Print the success message
                    if (!targets_proved.empty()) {
                        std::cout << "Target" << (targets_proved.size() == 1 ? " " : "s ") << targets_proved << " proved.\n";
                    }

                    // Add hydra to deletion list
                    hydras_to_remove.emplace_back(hydra_ptr);
                }
                else if (add_success == 0) {
                    // Hydra is proved under new assumptions
                    assumption_changed_flag = true;
                    // Continue processing descendants
                    for (auto& child_hydra : hydra_ptr->children) {
                        add_assumption_recursive(child_hydra);
                    }
                }
                // else -1: No action needed
            };

            // Start recursive assumption addition from current_hydra_ptr
            add_assumption_recursive(current_hydra_ptr);
        }
    }

    // Now handle the deletion list
    if (!hydras_to_remove.empty()) { 
        // Traverse the hydra_graph recursively to remove hydras from the deletion list
        std::queue<std::pair<std::shared_ptr<hydra>, std::vector<std::shared_ptr<hydra>>>> bfs_queue;
        // Initialize the queue with the root hydra_graph and an empty path
        bfs_queue.emplace(ctx.hydra_graph, std::vector<std::shared_ptr<hydra>>());

        while (!bfs_queue.empty()) {
            auto [current_node, path] = bfs_queue.front();
            bfs_queue.pop();

            // Update the current path
            std::vector<std::shared_ptr<hydra>> current_path = path;
            current_path.emplace_back(current_node);

            // Check if the current_node is in the deletion list
            if (std::find(hydras_to_remove.begin(), hydras_to_remove.end(), current_node) != hydras_to_remove.end()) {
                std::vector<int> targets_proved;
                // Found a hydra to remove
                // Traverse back the current_path to find the last node with more than one child
                int remove_index = 0;
                for (int i = static_cast<int>(current_path.size()) - 2; i >= 0; --i) {
                    if (current_path[i]->children.size() > 1) {
                        remove_index = i + 1; // The next hydra in the path to remove
                        break;
                    } else {
                        for (size_t j = 0; j < current_path[i]->target_indices.size(); ++j) {
                            targets_proved.push_back(current_path[i]->target_indices[j]); // Assuming line numbers start at 1
                        }
                    }
                }

                // Print the success message
                if (!targets_proved.empty()) {
                    std::cout << "Target" << (targets_proved.size() == 1 ? " " : "s ");
                    for (size_t j = 0; j < targets_proved.size(); j++) {
                       std::cout << targets_proved[j] + 1;
                       if (j != targets_proved.size() - 1) {
                           std::cout << ", ";
                       }
                    }
                    std::cout << " proved.\n";
                }

                std::shared_ptr<hydra> hydra_to_remove = current_path[remove_index];
                
                // Mark all targets in hydra_to_remove and its descendants as dead and inactive
                std::function<void(std::shared_ptr<hydra>)> mark_dead = [&](std::shared_ptr<hydra> hydra_ptr) {
                    for (int target_idx : hydra_ptr->target_indices) {
                        ctx.tableau[target_idx].active = false;
                        ctx.tableau[target_idx].dead = true;
                    }
                    for (auto& child : hydra_ptr->children) {
                        mark_dead(child);
                    }
                };
                mark_dead(hydra_to_remove);

                if (remove_index != 0) {
                    std::shared_ptr<hydra> parent_hydra = current_path[remove_index - 1];

                    // Remove hydra_to_remove from parent_hydra's children
                    parent_hydra->children.erase(
                        std::remove(parent_hydra->children.begin(), parent_hydra->children.end(), hydra_to_remove),
                        parent_hydra->children.end()
                    );
                } else {
                    ctx.hydra_graph = std::make_shared<hydra>(
                        std::vector<int>{}, // Empty targets
                        std::vector<std::vector<int>>{} // Empty proved
                    );
                }

                // Remove hydra_to_remove and its descendants from current_hydra
                // Traverse hydra_to_remove's subtree to remove any hydras from current_hydra
                std::function<void(std::shared_ptr<hydra>)> remove_from_current_hydra = [&](std::shared_ptr<hydra> hydra_ptr) {
                    auto it = std::find(ctx.current_hydra.begin(), ctx.current_hydra.end(), hydra_ptr);
                    if (it != ctx.current_hydra.end()) {
                        ctx.current_hydra.erase(it);
                    }
                    for (auto& child : hydra_ptr->children) {
                        remove_from_current_hydra(child);
                    }
                };
                remove_from_current_hydra(hydra_to_remove);
            }

            // Enqueue children for BFS traversal
            for (auto& child : current_node->children) {
                bfs_queue.emplace(child, current_path);
            }
        }

        // If deletions occurred, call purge_dead and proceed with original logic
        ctx.purge_dead();

        // Call get_hydra() and select_targets(targets)
        std::vector<int> new_targets = ctx.get_hydra();

        if (new_targets.empty()) {
            std::cout << std::endl << "All targets proved!\n";
            return true;
        }

        ctx.select_targets(new_targets);

        if (apply_cleanup) {
            ctx.upto = 0;
            cleanup_moves(ctx, ctx.upto);

            // Perform original logic as per deletion case
            return check_done(ctx, true);
        } else {
            return false;
        }
    }

    if (assumption_changed_flag) {
        // current hydra is not fully proved yet

        std::shared_ptr<hydra> current_leaf_hydra = ctx.current_hydra.back();

        std::vector<int> new_assumptions = current_leaf_hydra->proved.back();

        // Switch sign of final assumption
        new_assumptions.back() = -new_assumptions.back();

        // Select hypotheses with compatible assumptions
        ctx.select_hypotheses(current_leaf_hydra->target_indices, new_assumptions);

        if (apply_cleanup) {
            cleanup_moves(ctx, ctx.upto);

            // Check if done
            return check_done(ctx, true);
        }
    }

    return false;
}
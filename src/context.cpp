// context.cpp

#include "context.h"
#include "node.h"      // Assuming node.h contains the definition for node and vars_used
#include <algorithm>
#include <unordered_set>
#include <functional>
#include <iostream>
#include <stdexcept>

#define DEBUG_SELECT_HYPOTHESES 0

void tabline_t::print_restrictions() const {
    print_list(restrictions);
}

void tabline_t::print_assumptions() const {
    print_list(assumptions);
}

// Constructor: Initializes the context (empty var_indices)
context_t::context_t() 
    : hydra_graph(),          // 1. hydra_graph
      current_hydra(),        // 2. current_hydra
      upto(0),                // 3. upto
      var_indices()           // 4. var_indices
{}

// Retrieves and increments the next index for the given variable
int context_t::get_next_index(const std::string& var_name) {
    // If the variable is not present, initialize its index to 0
    if (var_indices.find(var_name) == var_indices.end()) {
        var_indices[var_name] = 0;
    } else {
        // Increment the index
        var_indices[var_name] += 1;
    }
    return var_indices[var_name];
}

// Retrieves the current index for the given variable without incrementing
int context_t::get_current_index(const std::string& var_name) const {
    auto it = var_indices.find(var_name);
    if (it != var_indices.end()) {
        return it->second;
    }
    return -1; // Indicates that the variable was not found
}

// Resets the index for the given variable to 0
void context_t::reset_index(const std::string& var_name) {
    var_indices[var_name] = 0;
}

// Checks if the variable exists in the context
bool context_t::has_variable(const std::string& var_name) const {
    return var_indices.find(var_name) != var_indices.end();
}

// Prints the current state of var_indices for debugging
void context_t::print_context() const {
    std::cout << "Current Context State:\n";
    for (const auto& pair : var_indices) {
        std::cout << "Variable: " << pair.first << ", Latest Index: " << pair.second << "\n";
    }
    std::cout << "--------------------------\n";
}

// Generates renaming pairs for common variables based on the context
std::vector<std::pair<std::string, std::string>> vars_rename_list(context_t& ctx, const std::set<std::string>& common_vars) {
    std::vector<std::pair<std::string, std::string>> renaming_pairs;

    for (const auto& var : common_vars) {
        std::string base = remove_subscript(var); // Assuming remove_subscript is defined elsewhere
        std::string new_var;

        if (base != var) {
            // Variable has a subscript, e.g., "x_1" -> "x"
            if (ctx.has_variable(base)) {
                // Base variable exists in context; get next index
                int new_index = ctx.get_next_index(base); // Increments and retrieves
                new_var = append_subscript(base, new_index); // Use append_subscript defined elsewhere
            } else {
                // Base variable does not exist; initialize it with index 0
                new_var = append_subscript(base, 0); // Use append_subscript defined elsewhere
                ctx.reset_index(base); // Sets "x" -> 0
            }
        } else {
            // Variable does not have a subscript, e.g., "x"
            if (ctx.has_variable(base)) {
                // Base variable exists in context; get next index
                int new_index = ctx.get_next_index(base); // Increments and retrieves
                new_var = append_subscript(base, new_index); // Use append_subscript defined elsewhere
            }
            else {
                // Variable does not exist; initialize it with index 0
                new_var = append_subscript(base, 0); // Use append_subscript defined elsewhere
                ctx.reset_index(base); // Sets "x" -> 0
            }
        }

        // Add the renaming pair to the list
        renaming_pairs.emplace_back(var, new_var);
    }

    return renaming_pairs;
}

// Function to print the reason for a given tableau line index
void print_reason(const context_t& context, int index) {
    // Check if the index is within the bounds of the tableau
    if (index < 0 || static_cast<size_t>(index) >= context.tableau.size()) {
        std::cerr << "Error: Line index " << index << " is out of bounds." << std::endl;
        return;
    }

    // Retrieve the specified tableau line
    const tabline_t& tabline = context.tableau[index];

    // Extract the justification reason and associated line numbers
    Reason reason = tabline.justification.first;
    const std::vector<int>& associated_lines = tabline.justification.second;

    // Determine the output based on the reason
    switch (reason) {
        case Reason::Target:
            std::cout << "Tar";
            break;

        case Reason::Hypothesis:
            std::cout << "Hyp";
            break;

        case Reason::Theorem:
        case Reason::Special:
            std::cout << "Thm";
            break;

        case Reason::Definition:
            std::cout << "Defn";
            break;

        case Reason::ModusPonens: {
            std::cout << "MP[";
            for (size_t i = 0; i < associated_lines.size(); ++i) {
                std::cout << associated_lines[i] + 1;
                if (i != associated_lines.size() - 1) {
                    std::cout << ", ";
                }
            }
            std::cout << "]";
            break;
        }

        case Reason::ModusTollens: {
            std::cout << "MT[";
            for (size_t i = 0; i < associated_lines.size(); ++i) {
                std::cout << associated_lines[i] + 1;
                if (i != associated_lines.size() - 1) {
                    std::cout << ", ";
                }
            }
            std::cout << "]";
            break;
        }

        case Reason::DisjunctiveIdempotence: {
            std::cout << "DI[";
            std::cout << associated_lines[0] + 1;
            std::cout << "]";
            break;
        }

        case Reason::ConjunctiveIdempotence: {
            std::cout << "CI[";
            std::cout << associated_lines[0] + 1;
            std::cout << "]";
            break;
        }

        case Reason::SplitConjunction: {
            std::cout << "SC[";
            std::cout << associated_lines[0] + 1;
            std::cout << "]";
            break;
        }

        case Reason::SplitDisjunction: {
            std::cout << "SD[";
            std::cout << associated_lines[0] + 1;
            std::cout << "]";
            break;
        }

        case Reason::SplitConjunctiveImplication: {
            std::cout << "SCI[";
            std::cout << associated_lines[0] + 1;
            std::cout << "]";
            break;
        }

        case Reason::SplitDisjunctiveImplication: {
            std::cout << "SDI[";
            std::cout << associated_lines[0] + 1;
            std::cout << "]";
            break;
        }

        case Reason::NegatedImplication: {
            std::cout << "NI[";
            std::cout << associated_lines[0] + 1;
            std::cout << "]";
            break;
        }

        case Reason::MaterialEquivalence: {
            std::cout << "ME[";
            std::cout << associated_lines[0] + 1;
            std::cout << "]";
            break;
        }

        case Reason::ConditionalPremise: {
            std::cout << "CP[";
            std::cout << associated_lines[0] + 1;
            std::cout << "]";
            break;
        }

        default:
            std::cout << "Unknown";
            break;
    }
}

void context_t::purge_dead() {
    // Iterate through all lines in the tableau
    for (size_t j = 0; j < tableau.size(); ++j) {
        tabline_t& current_line = tableau[j];
        
        // Check if the current line is a hypothesis
        if (!current_line.target) {
            // Proceed only if the restriction list is non-empty
            if (!current_line.restrictions.empty()) {
                bool all_targets_dead = true; // Flag to check if all restricted targets are dead
                
                // Iterate through each target index in the restriction list
                for (int target_idx : current_line.restrictions) {
                    const tabline_t& target_line = tableau[target_idx];
                    
                    // Check if the target is dead
                    if (!target_line.dead) {
                        // Found a target that is not dead; no need to purge this hypothesis
                        all_targets_dead = false;
                        break;
                    }
                }
                
                // If all restricted targets are dead, mark the hypothesis as dead and inactive
                if (all_targets_dead) {
                    current_line.dead = true;
                    current_line.active = false;
                }
            }
        }
    }
}

// Combine a pair of restrictions into a single restriction
std::vector<int> combine_restrictions(const std::vector<int>& res1, const std::vector<int>& res2) {
    if (res1.empty()) {
        return res2;
    }
    if (res2.empty()) {
        return res1;
    }

    std::vector<int> combined;
    combined.reserve(res1.size() + res2.size());

    // Sort both vectors
    std::vector<int> sorted_res1 = res1;
    std::vector<int> sorted_res2 = res2;
    std::sort(sorted_res1.begin(), sorted_res1.end());
    std::sort(sorted_res2.begin(), sorted_res2.end());

    // Perform set_union to merge without duplicates
    std::set_intersection(sorted_res1.begin(), sorted_res1.end(),
                  sorted_res2.begin(), sorted_res2.end(),
                  std::back_inserter(combined));

    return combined;
}

// Check if restrictions are compatible
bool restrictions_compatible(const std::vector<int>& res1, const std::vector<int>& res2) {
    if (res1.empty() || res2.empty()) {
        return true;
    }

    // Convert one vector to an unordered_set for O(1) lookups
    std::unordered_set<int> set_res1(res1.begin(), res1.end());

    // Check for at least one common element
    for (const int& elem : res2) {
        if (set_res1.find(elem) != set_res1.end()) {
            return true;
        }
    }

    return false;
}

// Updated Wrapper Function: Check if restrictions are compatible with error handling
bool check_restrictions(const std::vector<int>& res1, const std::vector<int>& res2) {
    if (!restrictions_compatible(res1, res2)) {
        std::cerr << "Restrictions incompatible.\n";
        std::cerr << "Restrictions 1: ";
        for (const auto& r : res1) {
            std::cerr << r << " ";
        }
        std::cerr << "\nRestrictions 2: ";
        for (const auto& r : res2) {
            std::cerr << r << " ";
        }
        std::cerr << std::endl;
        return false;
    }
    return true;
}

// Combine a pair of assumptions into a single set of assumptions
std::vector<int> combine_assumptions(const std::vector<int>& assm1, const std::vector<int>& assm2) {
    if (assm1.empty()) {
        return assm2;
    }
    if (assm2.empty()) {
        return assm1;
    }

    std::vector<int> combined;
    combined.reserve(assm1.size() + assm2.size());

    // Sort both vectors
    std::vector<int> sorted_assm1 = assm1;
    std::vector<int> sorted_assm2 = assm2;
    std::sort(sorted_assm1.begin(), sorted_assm1.end());
    std::sort(sorted_assm2.begin(), sorted_assm2.end());

    // Perform set_union to merge without duplicates
    std::set_union(sorted_assm1.begin(), sorted_assm1.end(),
                  sorted_assm2.begin(), sorted_assm2.end(),
                  std::back_inserter(combined));

    return combined;
}

// Check if assumptions are compatible
bool assumptions_compatible(const std::vector<int>& assm1, const std::vector<int>& assm2) {
    if (assm1.empty() || assm2.empty()) {
        return true;
    }

    // Convert assm2 to an unordered_set for O(1) lookups
    std::unordered_set<int> set_assm2(assm2.begin(), assm2.end());

    // Check for conflicting assumptions: n in assm1 vs -n in assm2
    for (const int& n : assm1) {
        if (set_assm2.find(-n) != set_assm2.end()) {
            return false; // Conflict found
        }
    }

    return true; // No conflicts
}

std::vector<int> merge_assumptions(const std::vector<int>& assm1, const std::vector<int>& assm2) {
    std::vector<int> merged = assm1;
    for (const int& n : assm2) {
        if (std::find(merged.begin(), merged.end(), n) == merged.end()) { // Avoid duplicates
            merged.push_back(n);
        }
    }
    return merged;
}

// Check if assumptions are compatible with error reporting
bool check_assumptions(const std::vector<int>& assm1, const std::vector<int>& assm2) {
    if (!assumptions_compatible(assm1, assm2)) {
        std::cerr << "Assumptions incompatible.\n";
        std::cerr << "Assumptions 1: ";
        for (const auto& a : assm1) {
            std::cerr << a << " ";
        }
        std::cerr << "\nAssumptions 2: ";
        for (const auto& a : assm2) {
            std::cerr << a << " ";
        }
        std::cerr << std::endl;
        return false;
    }
    return true;
}

void print_hydra_node(int tabs, std::shared_ptr<hydra> hyd) {
    // print indents
    for (int i = 0; i < tabs; i++) {
        std::cout << "  ";
    }
    hyd->print_targets();
    std::cout << std::endl;

    for (size_t i = 0; i < hyd->children.size(); ++i) {
        print_hydra_node(tabs + 1, hyd->children[i]);
    }
}

void context_t::print_hydras() {
    // Check if hydra_graph is initialized
    if (!hydra_graph) {
        std::cerr << "Hydra graph is not initialized.\n";
        return;
    }

    // Function to recursively print hydras
    for (const auto& child_hydra : hydra_graph->children) {
        print_hydra_node(0, child_hydra);
    }
}

void context_t::initialize_hydras() {
    // exit if already initialized
    if (hydra_graph) {
        return;
    }
    
    // Initialize the root hydra with no targets and empty proved using make_shared
    hydra_graph = std::make_shared<hydra>(
        std::vector<int>{}, // Empty targets
        std::vector<std::vector<int>>{} // Empty proved
    );

    // Iterate through the tableau to add child hydras for active target lines
    for (size_t i = 0; i < tableau.size(); ++i) {
        const tabline_t& tabline = tableau[i];

        if (tabline.target) {
            // Create a new hydra with the current index as its target
            std::vector<int> targets = { static_cast<int>(i) };
            std::vector<std::vector<int>> prfs = hydra_graph->proved;

            // Create a shared_ptr for the new hydra directly without creating an instance
            std::shared_ptr<hydra> new_hydra = std::make_shared<hydra>(targets, prfs);

            // Add the new hydra as a child to the hydra_graph
            hydra_graph->add_child(new_hydra);
        }
    }

    // After initialization, set current_hydra to point to the first child hydra
    if (!hydra_graph->children.empty()) {
        current_hydra.emplace_back(hydra_graph->children.front());
    }
}

// Sets current_hydra to the path from root to the first leaf in DFS
std::vector<int> context_t::get_hydra() {
    // Clear the current_hydra path
    current_hydra.clear();

    // Check if hydra_graph is initialized
    if (!hydra_graph) {
        std::cerr << "Hydra graph is not initialized.\n";
        return {};
    }

    // Add the root hydra to the path
    current_hydra.emplace_back(hydra_graph);

    // Initialize a shared_ptr to traverse the hydra tree
    std::shared_ptr<hydra> current_node = hydra_graph;

    // Traverse the first child recursively until a leaf is found
    while (!current_node->children.empty()) {
        std::shared_ptr<hydra> child_ptr = current_node->children.front();
        if (child_ptr) {
            current_hydra.emplace_back(child_ptr);
            current_node = child_ptr;
        }
        else {
            std::cerr << "Encountered a null child hydra. Stopping traversal.\n";
            break; // No valid child found
        }
    }

    // Return the list of targets in the leaf hydra
    return current_node->target_indices;
}

// Selects and activates/deactivates targets and hypotheses based on the provided list
void context_t::select_targets(const std::vector<int>& targets) {
    // Convert targets to an unordered_set for O(1) lookups
    std::unordered_set<int> target_set(targets.begin(), targets.end());

    for (size_t i = 0; i < tableau.size(); ++i) {
        tabline_t& tabline = tableau[i];

        if (tabline.target) {
            // If the tabline is a target, set active if its index is in the targets list
            tabline.active = (target_set.find(static_cast<int>(i)) != target_set.end());
        }
        else {
            // If the tabline is a hypothesis
            bool restrictions_empty = tabline.restrictions.empty();
            bool alive = !tabline.dead;
            bool restrictions_contains_target = false;

            // Check if any restriction is in the target_set
            for (const int& res : tabline.restrictions) {
                if (target_set.find(res) != target_set.end()) {
                    restrictions_contains_target = true;
                    break;
                }
            }

            // Determine activation based on the specified conditions
            if (alive && (restrictions_empty || restrictions_contains_target)) {
                tabline.active = true;
            }
            else {
                tabline.active = false;
            }
        }
    }
}

// Selects targets based on the current leaf hydra
void context_t::select_targets() {
    // Check if current_hydra is not empty
    if (current_hydra.empty()) {
        std::cerr << "Error: current_hydra is empty. Cannot select targets.\n";
        return;
    }

    // Access the current leaf hydra (last hydra in the current_hydra path)
    std::shared_ptr<hydra> current_leaf = current_hydra.back();

    // Extract target indices from the current leaf hydra
    std::vector<int> targets = current_leaf->target_indices;

    // Pass the extracted targets to the original select_targets function
    select_targets(targets);
}

void context_t::select_hypotheses(const std::vector<int>& targets, const std::vector<int>& assumptions) {
    // Convert targets to an unordered_set for O(1) lookups
    std::unordered_set<int> target_set(targets.begin(), targets.end());
    std::unordered_set<int> assumption_set(assumptions.begin(), assumptions.end());

#if DEBUG_SELECT_HYPOTHESES
    std::cout << "Assumptions : ";
    for (const int& res : assumptions) {
        std::cout << res << ", ";
    }
    std::cout << std::endl;
#endif

    for (size_t i = 0; i < tableau.size(); ++i) {
        tabline_t& tabline = tableau[i];

        if (!tabline.target) {
            // If the tabline is a hypothesis
            bool restrictions_empty = tabline.restrictions.empty();
            bool alive = !tabline.dead;
            bool restrictions_contains_target = false;

            // Check if any restriction is in the target_set
            for (const int& res : tabline.restrictions) {
                if (target_set.find(res) != target_set.end()) {
                    restrictions_contains_target = true;
                    break;
                }
            }

            // Determine activation based on the specified conditions
            if (alive && (restrictions_empty || restrictions_contains_target)) {
                if (tabline.assumptions.empty()) {
                    tabline.active = true;
                } else {
#if DEBUG_SELECT_HYPOTHESES
                    std::cout << "Assumptions " << i << " : ";
                    for (const int& res : tabline.assumptions) {
                        std::cout << res << ", ";
                    }
                    std::cout << std::endl;
#endif

                    bool assumptions_found = true; // whether all assumptions in list compatible
                    // check if all assumptions are in the given list
                    for (const int& res : assumption_set) {
                        if (find(tabline.assumptions.begin(), tabline.assumptions.end(), -res) != tabline.assumptions.end()) {
                            assumptions_found = false;
                            break;
                        }
                    }

#if DEBUG_SELECT_HYPOTHESES
                    std::cout << "Assumptions found: " << assumptions_found << std::endl;
#endif
                    // set tabline.active
                    tabline.active = assumptions_found;
                }
            }
            else {
                tabline.active = false;
            }
        }
    }
}

// Replaces target i with j in the current leaf hydra
void context_t::hydra_replace(int i, int j, bool shared) {
    if (current_hydra.empty()) {
        std::cerr << "Error: No hydra available to replace targets." << std::endl;
        return;
    }

    // Get the current leaf hydra (last in current_hydra)
    std::shared_ptr<hydra> current_leaf = current_hydra.back();

    // Create new target_indices by replacing i with j
    std::vector<int> new_targets = current_leaf->target_indices;

    // Check if target i exists in the current leaf hydra
    auto it = std::find(new_targets.begin(), new_targets.end(), i);
    if (it == new_targets.end()) {
        std::cerr << "Error: Target " << i << " not found in the current leaf hydra." << std::endl;
        return;
    }

    *it = j; // Replace the first occurrence of i with j

    // Create a new hydra with the updated targets and copy 'proved' from current_leaf
    std::shared_ptr<hydra> new_hydra_ptr = std::make_shared<hydra>(new_targets, current_leaf->proved);

    // New hydra has shared metavariables if the old one did or if shared was set to true
    new_hydra_ptr->shared = current_leaf->shared | shared;

    // Add new hydra to graph
    current_leaf->add_child(new_hydra_ptr);

    // Make the new hydra the current leaf by appending it to current_hydra
    current_hydra.emplace_back(new_hydra_ptr);
}

// Replaces target i with j in all restrictions
void context_t::restrictions_replace(int i, int j) {
    for (size_t k = 0; k < tableau.size(); ++k) {
        tabline_t& tabline = tableau[k];

        if (!tabline.dead) {
            // Find i in restrictions
            auto it = std::find(tabline.restrictions.begin(), tabline.restrictions.end(), i);
            if (it != tabline.restrictions.end()) {
                // Add j to restrictions
                tabline.restrictions.push_back(j);
            }
        }
    }
}

// Splits target i into j1 and j2 in the current leaf hydra
void context_t::hydra_split(int i, int j1, int j2) {
    if (current_hydra.empty()) {
        std::cerr << "Error: No hydra available to split targets." << std::endl;
        return;
    }

    // Get the current leaf hydra
    std::shared_ptr<hydra> current_leaf = current_hydra.back();

    // Find target i in target_indices
    auto it = std::find(current_leaf->target_indices.begin(), current_leaf->target_indices.end(), i);
    if (it == current_leaf->target_indices.end()) {
        std::cerr << "Error: Target " << i << " not found in the current leaf hydra." << std::endl;
        return;
    }

    // Check if there are shared variables
    std::set<std::string> shared = find_common_variables(tableau[j1].formula, tableau[j2].formula);
    if (!shared.empty()) { // there are shared variables
        // mark variables as shared
        mark_shared(tableau[j1].formula, shared);
        mark_shared(tableau[j2].formula, shared);
    }

    if (current_leaf->target_indices.size() == 1 && !current_leaf->shared && shared.empty())
    {
        // make new list of targets with just j1
        std::vector<int> new_targets1 = {j1};
        
        // Create a new hydra with the updated targets and copy 'proved' from current_leaf
        std::shared_ptr<hydra> new_hydra_ptr1 = std::make_shared<hydra>(new_targets1, current_leaf->proved);

        // Attach the new hydra as the sole child of the current leaf
        current_leaf->add_child(new_hydra_ptr1);

        // make new list of targets with just j2
        std::vector<int> new_targets2 = {j2};

        // make new hydra
        std::shared_ptr<hydra> new_hydra_ptr2 = std::make_shared<hydra>(new_targets2, current_leaf->proved);
         
        // Attach the new hydra as the sole child of the current leaf
        current_leaf->add_child(new_hydra_ptr2);

        // Make the new hydra the current leaf by appending it to current_hydra
        current_hydra.emplace_back(new_hydra_ptr2);
    } else {
        // Create new target_indices by replacing i with j1 and j2
        std::vector<int> new_targets = current_leaf->target_indices;

        *std::find(new_targets.begin(), new_targets.end(), i) = j1;; // Replace the first occurrence of i with j1
        new_targets.push_back(j2); // append j2

        // Create a new hydra with the updated targets and copy 'proved' from current_leaf
        std::shared_ptr<hydra> new_hydra_ptr = std::make_shared<hydra>(new_targets, current_leaf->proved);

        // Original hydra was either shared or we introduced shared variables
        new_hydra_ptr->shared = true;

        // Attach the new hydra as the sole child of the current leaf
        current_leaf->add_child(new_hydra_ptr);

        // Make the new hydra the current leaf by appending it to current_hydra
        current_hydra.emplace_back(new_hydra_ptr);
    }
}

void context_t::restrictions_split(int i, int j1, int j2) {
    for (size_t k = 0; k < tableau.size(); ++k) {
        tabline_t& tabline = tableau[k];

        if (!tabline.dead) {
            // find i in restrictions
            auto it = std::find(tabline.restrictions.begin(), tabline.restrictions.end(), i);
            if (it != tabline.restrictions.end()) {
                // add j1 and j2 to restrictions
                tabline.restrictions.push_back(j1);
                tabline.restrictions.push_back(j2);
            }
        }
    }
}

// Replaces a list of targets with a single target j in the current leaf hydra
void context_t::hydra_replace_list(const std::vector<int>& targets, int j) {
    if (current_hydra.empty()) {
        std::cerr << "Error: No hydra available to replace targets." << std::endl;
        return;
    }

    // Get the current leaf hydra
    std::shared_ptr<hydra> current_leaf = current_hydra.back();

    // Verify that all targets in the list exist in the current leaf hydra
    for (const int& t : targets) {
        if (std::find(current_leaf->target_indices.begin(), current_leaf->target_indices.end(), t) == current_leaf->target_indices.end()) {
            std::cerr << "Error: Target " << t << " not found in the current leaf hydra." << std::endl;
            return;
        }
    }

    // Create a new target list by removing all targets in 'targets' and adding 'j' once
    std::vector<int> new_targets;
    new_targets.reserve(current_leaf->target_indices.size() - targets.size() + 1);

    for (const int& t : current_leaf->target_indices) {
        if (std::find(targets.begin(), targets.end(), t) == targets.end()) {
            new_targets.push_back(t);
        }
    }

    // Add 'j' if it's not already present
    if (std::find(new_targets.begin(), new_targets.end(), j) == new_targets.end()) {
        new_targets.push_back(j);
    }

    // Check if the new target list already exists among children hydras
    bool duplicate = false;
    for (const auto& child_ptr : current_leaf->children) {
        if (child_ptr->target_indices == new_targets) {
            duplicate = true;
            break;
        }
    }

    if (duplicate) {
        std::cerr << "Warning: Replacement targets already exist among children hydras. No new hydra added." << std::endl;
        return;
    }

    // Create a new hydra with the updated targets and copy 'proved' from current_leaf
    std::shared_ptr<hydra> new_hydra_ptr = std::make_shared<hydra>(new_targets, current_leaf->proved);

    // Add new hydra to graph
    current_leaf->add_child(new_hydra_ptr);

    // Make new hydra current one
    current_hydra.emplace_back(new_hydra_ptr);
}

void context_t::restrictions_replace_list(const std::vector<int>& targets, int j) {
    for (size_t k = 0; k < tableau.size(); ++k) {
        tabline_t& tabline = tableau[k];

        if (!tabline.dead) {
            bool found_target = false; // whether a target in the list has been found
            // find i in restrictions
            for (size_t  m = 0; m < targets.size(); ++m) {
                int i = targets[m];
                auto it = std::find(tabline.restrictions.begin(), tabline.restrictions.end(), i);
                if (it != tabline.restrictions.end()) {
                    found_target = true;
                    break;
                }
            }
            
            if (found_target){    // add j to restrictions
                tabline.restrictions.push_back(j);
            }
        }
    }
}

std::vector<std::shared_ptr<hydra>> context_t::partition_hydra(hydra& h) {
    // 1. Map from variable to list of target indices that use it
    std::unordered_map<std::string, std::vector<int>> var_to_targets;

    // 2. Iterate over targets to populate var_to_targets
    for (const int& target_idx : h.target_indices) {
        if (target_idx < 0 || static_cast<size_t>(target_idx) >= tableau.size()) {
            std::cerr << "Error: Target index " << target_idx << " is out of bounds in the tableau." << std::endl;
            continue; // Skip invalid targets
        }

        const tabline_t& tabline = tableau[target_idx];
        if (!tabline.formula) {
            std::cerr << "Error: Formula pointer is null for target index " << target_idx << "." << std::endl;
            continue; // Skip if formula is null
        }

        // Get variables used in the formula
        std::set<std::string> variables;
        vars_used(variables, tabline.formula, false); // include_params = false

        for (const std::string& var : variables) {
            var_to_targets[var].push_back(target_idx);
        }
    }

    // 3. Identify shared variables (variables used in more than one target)
    std::set<std::string> shared_vars;
    for (const auto& [var, targets] : var_to_targets) {
        if (targets.size() > 1) { // Variable is shared
            shared_vars.insert(var);
        }
    }

    // 4. Mark shared variables in all target formulas
    for (const int& target_idx : h.target_indices) {
        tabline_t& tabline = tableau[target_idx];

        // Mark shared variables in the formula
        mark_shared(tabline.formula, shared_vars);

        // mark shared in negation if it exists
        if (tabline.negation) {
            mark_shared(tabline.negation, shared_vars);
        }
    }

    // 5. Initialize Union-Find structure for partitioning
    std::unordered_map<int, int> parent;
    for (const int& target_idx : h.target_indices) {
        parent[target_idx] = target_idx;
    }

    // 6. Lambda function for finding the root parent
    std::function<int(int)> find_set = [&](int x) -> int {
        if (parent[x] != x) {
            parent[x] = find_set(parent[x]); // Path compression
        }
        return parent[x];
    };

    // 7. Lambda function for uniting two sets
    auto union_set = [&](int x, int y) -> void {
        int fx = find_set(x);
        int fy = find_set(y);
        if (fx != fy) {
            parent[fx] = fy;
        }
    };

    // 8. Union targets that share the same variable
    for (const auto& [var, targets] : var_to_targets) {
        if (targets.size() < 2) continue; // Not a shared variable

        for (size_t k = 1; k < targets.size(); ++k) {
            union_set(targets[0], targets[k]);
        }
    }

    // 9. Group targets by their root parent to form partitions
    std::unordered_map<int, std::vector<int>> partitions;
    for (const int& target_idx : h.target_indices) {
        int root = find_set(target_idx);
        partitions[root].push_back(target_idx);
    }

    // 10. Create new hydras for each partition
    std::vector<std::shared_ptr<hydra>> new_hydras;
    new_hydras.reserve(partitions.size()); // Optional: reserve space to optimize

    for (const auto& [root, partition_targets] : partitions) {
        if (partition_targets.empty()) {
            continue; // Skip empty partitions
        }

        // Create a new hydra for this partition
        std::shared_ptr<hydra> new_hydra_ptr = std::make_shared<hydra>(
            partition_targets, // Passed by value; hydra constructor copies
            h.proved          // Copy the 'proved' member from the original hydra
        );

        // Partitions with more than one target have shared variables
        new_hydra_ptr->shared = partition_targets.size() != 1;

        new_hydras.push_back(new_hydra_ptr);
    }

    // 11. Attach all new hydras as children of the original hydra
    for (const auto& hyd_ptr : new_hydras) {
        h.add_child(hyd_ptr);
    }

    return new_hydras;
}

// Function to print all formulas in the tableau with reasons
void print_tableau(const context_t& tab_ctx) {
    bool theorems_exist = false; // if any hypotheses are theorems
    std::cout << "Hypotheses:" << std::endl;
    
    // First, print all active Hypotheses that are not theorems
    for (size_t i = 0; i < tab_ctx.tableau.size(); ++i) {
        const tabline_t& tabline = tab_ctx.tableau[i];
        if (tabline.active && !tabline.target) {
            if (!tabline.is_theorem() && !tabline.is_special() && !tabline.is_definition()) {
                std::cout << " " << i + 1 << " "; // Line number
                print_reason(tab_ctx, static_cast<int>(i)); // Print reason
                std::cout << ": " << tabline.formula->to_string(UNICODE);
                if (!tabline.assumptions.empty()) {
                    std::cout << "    ass:";
                    tabline.print_assumptions();
                }
                if (!tabline.restrictions.empty()) {
                    std::cout << "    res:";
                    tabline.print_restrictions();
                }
                std::cout << std::endl;
            } else {
                theorems_exist = true;
            }
        }
    }
    
    if (theorems_exist) {
        std::cout << std::endl << "Library premises:" << std::endl;
        
        // First, print all active Hypotheses that are not theorems
        for (size_t i = 0; i < tab_ctx.tableau.size(); ++i) {
            const tabline_t& tabline = tab_ctx.tableau[i];
            if (tabline.active && !tabline.target && (tabline.is_theorem() || tabline.is_definition() || tabline.is_special())) {
                std::cout << " " << i + 1 << " "; // Line number
                print_reason(tab_ctx, static_cast<int>(i)); // Print reason
                std::cout << ": " << tabline.formula->to_string(UNICODE);
                std::cout << std::endl;
            }
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

void context_t::get_constants() {
    // Check if the digest has been computed
    bool has_digest = !digest.empty();

    if (has_digest) {
        // Iterate through each digest entry (each vector of digest_item)
        for (const auto& digest_entry : digest) {
            // Iterate through each digest_item in the digest_entry
            for (const auto& digest_item : digest_entry) {
                size_t line_idx = digest_item.module_line_idx;
                // size_t cannot be negative, so no need to check line_idx < 0

                // Validate line index
                if (line_idx >= tableau.size()) {
                    std::cerr << "Warning: Line index " << line_idx << " in digest is out of bounds." << std::endl;
                    continue; // Skip invalid indices
                }

                tabline_t& tabline = tableau[line_idx];

                // Compute constants using node_get_constants
                if (tabline.formula->is_implication()) {
                    node_get_constants(tabline.constants1, tabline.formula->children[0]);
                    node_get_constants(tabline.constants2, tabline.formula->children[1]);
                } else {
                    node_get_constants(tabline.constants1, tabline.formula);
                }
            }
        }
    }
    else {
        // No digest available; process all tableau lines
        for (size_t i = upto; i < tableau.size(); ++i) {
            tabline_t& tabline = tableau[i];
            // No need to check for null, as per design

            // Compute constants using node_get_constants
            if (!tabline.target && tabline.formula->is_implication()) {
                node_get_constants(tabline.constants1, tabline.formula->children[0]);
                node_get_constants(tabline.constants2, tabline.formula->children[1]);
            } else {
                node_get_constants(tabline.constants1, tabline.formula);
            }
        }
    }
}

void context_t::get_ltor() {
    // Check if the digest has been computed
    bool has_digest = !digest.empty();

    if (has_digest) {
        // Iterate through each digest entry (each vector of digest_item)
        for (const auto& digest_entry : digest) {
            // Iterate through each digest_item in the digest_entry
            for (const auto& digest_item : digest_entry) {
                size_t line_idx = digest_item.module_line_idx;
                // size_t cannot be negative, so no need to check line_idx < 0

                tabline_t& tabline = tableau[line_idx];

                // Compute constants using node_get_constants
                if (tabline.formula->is_implication() || tabline.formula->is_disjunction()) {
                    left_to_right(tabline.ltor, tabline.rtol, tabline.formula);
                }
            }
        }
    }
    else {
        // No digest available; process all tableau lines
        for (size_t i = upto; i < tableau.size(); ++i) {
            tabline_t& tabline = tableau[i];
            // No need to check for null, as per design

            // Compute constants using node_get_constants
            if (!tabline.target && (tabline.formula->is_implication())) {
                left_to_right(tabline.ltor, tabline.rtol, tabline.formula);
            }
        }
    }
}

std::optional<context_t*> context_t::find_module(const std::string& filename_stem) {
    for (auto& [fname, ctx] : modules) {
        if (fname == filename_stem) {
            return &ctx;
        }
    }
    return std::nullopt;
}

void context_t::get_tableau_constants(
    std::vector<std::string>& all_constants,
    std::vector<std::string>& target_constants,
    std::vector<size_t>& implication_indices,
    std::vector<size_t>& unit_indices) const
{
    // Iterate through each line in the tableau
    for (size_t i = 0; i < tableau.size(); ++i) {
        const tabline_t& tabline = tableau[i];
        
        // Skip inactive lines
        if (!tabline.active) {
            continue;
        }
        
        if (tabline.target) {
            // Accumulate constants from active target lines
            node_get_constants(target_constants, tabline.formula);
        }
        else if (!tabline.is_theorem() && !tabline.is_definition()) {
            // Accumulate constants from active lines that are neither theorems nor definitions
            node_get_constants(all_constants, tabline.formula);
            
            // Check if the formula is an implication
            if (tabline.formula->is_implication()) {
                implication_indices.push_back(i);
            }
            else {
                unit_indices.push_back(i);
            }
        }
        // Note: Targets cannot be theorems or definitions, so no further checks are needed
    }
}

void context_t::kill_duplicates(size_t start_index) {
    for (size_t i = start_index; i < tableau.size(); ++i) {
        tabline_t& current_line = tableau[i];

        // Only consider active lines that are hypotheses (not targets)
        if (!current_line.active || current_line.target) {
            continue;
        }

        // Iterate over all prior active hypotheses
        for (size_t j = 0; j < i; ++j) {
            const tabline_t& prior_line = tableau[j];

            // Only compare with active hypotheses
            if (!prior_line.active || prior_line.target) {
                continue;
            }

            // Check if formulas are equal
            if (!equal(current_line.formula, prior_line.formula)) {
                continue;
            }

            // Check assumptions
            bool assumptions_ok = false;
            if (current_line.assumptions.empty() && prior_line.assumptions.empty()) {
                assumptions_ok = true;
            }
            else if (prior_line.assumptions.empty()) {
                assumptions_ok = true;
            }
            else {
                // Check if all assumptions of prior_line are contained in current_line
                bool all_assumptions_contained = std::all_of(
                    prior_line.assumptions.begin(),
                    prior_line.assumptions.end(),
                    [&](int a) {
                        return std::find(current_line.assumptions.begin(), current_line.assumptions.end(), a) != current_line.assumptions.end();
                    }
                );
                if (all_assumptions_contained) {
                    assumptions_ok = true;
                }
            }

            if (!assumptions_ok) {
                continue; // Assumptions do not satisfy the criteria
            }

            // Check restrictions
            bool restrictions_ok = false;
            if (current_line.restrictions.empty() && prior_line.restrictions.empty()) {
                restrictions_ok = true;
            }
            else if (prior_line.restrictions.empty()) {
                restrictions_ok = true;
            }
            else {
                // Check if all restrictions of current_line are contained in prior_line
                bool all_restrictions_contained = std::all_of(
                    current_line.restrictions.begin(),
                    current_line.restrictions.end(),
                    [&](int r) {
                        return std::find(prior_line.restrictions.begin(), prior_line.restrictions.end(), r) != prior_line.restrictions.end();
                    }
                );
                if (all_restrictions_contained) {
                    restrictions_ok = true;
                }
            }

            if (!restrictions_ok) {
                continue; // Restrictions do not satisfy the criteria
            }

            // All conditions met, mark the current line as inactive and dead
            current_line.active = false;
            current_line.dead = true;

            // No need to check further prior lines for this current line
            break;
        }
    }

    if (!current_hydra.empty()) {
        // Access the current leaf hydra (last hydra in the current_hydra path)
        std::shared_ptr<hydra> current_leaf = current_hydra.back();

        // Extract target indices from the current leaf hydra
        std::vector<int> leaf_targets = current_leaf->target_indices;

        for (std::shared_ptr<hydra> past_hydra : current_hydra) {
            if (past_hydra != current_leaf) {
                std::vector<int> hydra_targets = past_hydra->target_indices;

                bool hydra_found = true;

                // Check each leaf target exists in hydra
                for (size_t leaf_tar_idx : leaf_targets) {
                    tabline_t& leaf_tabline = tableau[leaf_tar_idx];

                    bool leaf_tar_found = false;
                
                    for (size_t hydra_tar_idx : hydra_targets) {
                        tabline_t& hydra_tabline = tableau[hydra_tar_idx];

                        if (equal(leaf_tabline.formula, hydra_tabline.formula)) {
                            leaf_tar_found = true;
                            break;
                        }
                    }

                    if (!leaf_tar_found) {
                        hydra_found = false;
                        break;
                    }
                }

                if (hydra_found) {
                    std::shared_ptr<hydra> parent_hydra = current_hydra[current_hydra.size() - 2];

                    // Remove hydra_to_remove from parent_hydra's children
                    parent_hydra->children.erase(
                        std::remove(parent_hydra->children.begin(), parent_hydra->children.end(), current_leaf),
                            parent_hydra->children.end()
                        );

                    current_hydra.pop_back();
                    std::cout << "deleted" << std::endl;
                    select_targets();
                    break;
                }
            }
        }
    }
}

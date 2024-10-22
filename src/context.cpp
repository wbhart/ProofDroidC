// context.cpp

#include "context.h"
#include "node.h"      // Assuming node.h contains the definition for node and vars_used
#include <algorithm>
#include <unordered_set>
#include <functional>
#include <iostream>
#include <stdexcept>

void tabline_t::print_restrictions() {
    std::cout << "[";
    for (size_t i = 0; i < restrictions.size(); i++) {
        std::cout << restrictions[i];
        if (i < restrictions.size() - 1) {
            std::cout << ", ";
        }
    }
    std::cout << "]";
}

void tabline_t::print_assumptions() {
    std::cout << "[";
    for (size_t i = 0; i < assumptions.size(); i++) {
        std::cout << assumptions[i];
        if (i < assumptions.size() - 1) {
            std::cout << ", ";
        }
    }
    std::cout << "]";
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
            std::vector<std::vector<int>> prfs = hydra_graph->proved; // Correct reference

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

// Replaces target i with j in the current leaf hydra
void context_t::hydra_replace(int i, int j) {
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

    // Check if the current leaf hydra has only target j (after replacement)
    if (current_leaf->target_indices.size() == 1 && current_leaf->target_indices[0] == j) {
        // Attach the new hydra as the sole child of the current leaf
        current_leaf->add_child(new_hydra_ptr);

        // Make the new hydra the current leaf by appending it to current_hydra
        current_hydra.emplace_back(new_hydra_ptr);
    }
    else {
        // If there are multiple targets, partition the new hydra
        std::vector<std::shared_ptr<hydra>> partitioned_hydras = partition_hydra(*new_hydra_ptr);

        // Attach all partitioned hydras as children of the current leaf
        for (auto& hyd_ptr : partitioned_hydras) {
            current_leaf->add_child(hyd_ptr);
        }

        // Make the first partitioned hydra the new current leaf
        if (!partitioned_hydras.empty()) {
            current_hydra.emplace_back(partitioned_hydras[0]);
        }
    }
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

    // Create two new target lists by replacing i with j1 and j2
    std::vector<int> new_targets_j1 = current_leaf->target_indices;
    *std::find(new_targets_j1.begin(), new_targets_j1.end(), i) = j1;

    std::vector<int> new_targets_j2 = current_leaf->target_indices;
    *std::find(new_targets_j2.begin(), new_targets_j2.end(), i) = j2;

    // Function to check for duplicate hydras based on target_indices
    auto is_duplicate = [&](const std::vector<int>& new_targets) -> bool {
        for (const auto& child_ptr : current_leaf->children) {
            if (child_ptr->target_indices == new_targets) {
                return true;
            }
        }
        return false;
    };

    // Create and add new hydras for j1 and j2
    std::vector<std::shared_ptr<hydra>> new_children;

    // Replacement j1
    if (!is_duplicate(new_targets_j1)) {
        std::shared_ptr<hydra> new_hydra_ptr_j1 = std::make_shared<hydra>(new_targets_j1, current_leaf->proved);
        current_leaf->add_child(new_hydra_ptr_j1);
        new_children.push_back(new_hydra_ptr_j1);
    }

    // Replacement j2
    if (!is_duplicate(new_targets_j2)) {
        std::shared_ptr<hydra> new_hydra_ptr_j2 = std::make_shared<hydra>(new_targets_j2, current_leaf->proved);
        current_leaf->add_child(new_hydra_ptr_j2);
        new_children.push_back(new_hydra_ptr_j2);
    }

    if (new_children.empty()) {
        std::cerr << "Warning: Both hydra replacements resulted in duplicates. No new hydras added." << std::endl;
        return;
    }

    // Update current_hydra to point to the first new child hydra
    current_hydra.emplace_back(new_children[0]);
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

    // Partition the new hydra if necessary
    std::vector<std::shared_ptr<hydra>> partitioned_hydras = partition_hydra(*new_hydra_ptr);

    // Attach all partitioned hydras as children of the current leaf
    for (auto& hyd_ptr : partitioned_hydras) {
        current_leaf->add_child(hyd_ptr);
    }

    // Make the first partitioned hydra the new current leaf
    if (!partitioned_hydras.empty()) {
        current_hydra.emplace_back(partitioned_hydras[0]);
    }
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

// Helper Function: Partitions a hydra based on shared variables and creates new hydras
std::vector<std::shared_ptr<hydra>> context_t::partition_hydra(hydra& h) {
    // Map from variable to list of target indices that use it
    std::unordered_map<std::string, std::vector<int>> var_to_targets;

    // Iterate over targets to populate var_to_targets
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

        // Get variables used in the formula (assuming vars_used is defined elsewhere)
        std::set<std::string> variables;
        vars_used(variables, tabline.formula, false); // include_params = false

        for (const std::string& var : variables) {
            var_to_targets[var].push_back(target_idx);
        }
    }

    // Initialize Union-Find structure for partitioning
    std::unordered_map<int, int> parent;
    for (const int& target_idx : h.target_indices) {
        parent[target_idx] = target_idx;
    }

    // Lambda function for finding the root parent
    std::function<int(int)> find_set = [&](int x) -> int {
        if (parent[x] != x) {
            parent[x] = find_set(parent[x]); // Path compression
        }
        return parent[x];
    };

    // Lambda function for uniting two sets
    auto union_set = [&](int x, int y) -> void {
        int fx = find_set(x);
        int fy = find_set(y);
        if (fx != fy) {
            parent[fx] = fy;
        }
    };

    // Union targets that share the same variable
    for (const auto& [var, targets] : var_to_targets) {
        for (size_t k = 1; k < targets.size(); ++k) {
            union_set(targets[0], targets[k]);
        }
    }

    // Group targets by their root parent to form partitions
    std::unordered_map<int, std::vector<int>> partitions;
    for (const int& target_idx : h.target_indices) {
        int root = find_set(target_idx);
        partitions[root].push_back(target_idx);
    }

    // Create new hydras for each partition
    std::vector<std::shared_ptr<hydra>> new_hydras;
    new_hydras.reserve(partitions.size()); // Optional: reserve space to optimize

    for (const auto& [root, partition_targets] : partitions) {
        if (partition_targets.empty()) {
            continue; // Skip empty partitions
        }

        // Create a new hydra for this partition
        std::shared_ptr<hydra> new_hydra_ptr = std::make_shared<hydra>(
            partition_targets, // Passed by value; hydra constructor copies
            h.proved // Copy the 'proved' member from the original hydra
        );

        new_hydras.push_back(new_hydra_ptr);
    }

    // Attach all new hydras as children of the original hydra
    for (const auto& hyd_ptr : new_hydras) {
        h.add_child(hyd_ptr);
    }

    return new_hydras;
}

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
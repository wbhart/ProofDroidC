// context.h

#ifndef CONTEXT_H
#define CONTEXT_H

#include "node.h"
#include "hydra.h"
#include <unordered_map>
#include <string>
#include <iostream>
#include <vector>
#include <set>
#include <utility>
#include <unordered_set>
#include <algorithm>
#include <memory> // Include this for std::shared_ptr and std::make_shared

// Enumeration representing reasons for justifications in tableau lines
enum class Reason {
    ModusPonens,
    ModusTollens,
    Target,
    Hypothesis,
    ConjunctiveIdempotence,
    DisjunctiveIdempotence,
    SplitConjunction,
    SplitDisjunctiveImplication,
    SplitConjunctiveImplication,
    NegatedImplication,
    ConditionalPremise
};

// Represents a single line in the tableau
class tabline_t {
public:
    bool target = false;                           // Indicates if this line is a target
    bool active = true;                            // Indicates if this line is active
    bool dead = false;                             // Whether target proved or hypothesis no longer useful
    std::vector<int> assumptions;                  // Indices of assumptions
    std::vector<int> restrictions;                 // Indices of restrictions
    std::pair<Reason, std::vector<int>> justification; // How this was proved and from which lines
    node* formula = nullptr;                       // Pointer to the associated formula
    node* negation = nullptr;                      // Pointer to the negation of the formula
    std::vector<std::pair<int, int>> unifications; // List of pairs (i, j) where i unifies with j

    // Constructor Initializer Lists to Match Declaration Order
    tabline_t(node* form) 
        : target(false), active(true), dead(false), formula(form), negation(nullptr), unifications() {}
    
    tabline_t(node* form, node* neg) 
        : target(true), active(true), dead(false), formula(form), negation(neg), unifications() {}
};

class context_t {
public:
    context_t();

    // Whether free variables have already been made into parameters
    bool parameterized = false;

    // Retrieves and increments the next available index for a variable
    int get_next_index(const std::string& var_name);

    // Retrieves the current index of a variable without incrementing
    // Returns -1 if the variable does not exist
    int get_current_index(const std::string& var_name) const;

    // Resets the index of a variable to zero
    // Initializes the variable if it does not exist
    void reset_index(const std::string& var_name);

    // Checks if a variable exists in the context
    bool has_variable(const std::string& var_name) const;

    // Prints the current state of variable indices for debugging
    void print_context() const;

    // Array of tableau lines
    std::vector<tabline_t> tableau;

    // Hydra graph representing the target tree.
    // Initialized to be empty when a context_t instance is created.
    hydra hydra_graph;

    // Initializes hydras based on the tableau
    void initialize_hydras();

    // Sets current_hydra to the path from root to the first leaf in DFS
    // Now returns the list of target indices in the leaf hydra
    std::vector<int> get_hydra();

    // Path to current hydra (list of references to hydras along the way)
    std::vector<std::reference_wrapper<hydra>> current_hydra;
    
    // Lines already dealt with (used for incremental completion checking)
    int upto = 0;

    // Selects and activates/deactivates targets and hypotheses based on the provided list
    void select_targets(const std::vector<int>& targets);

    // Selects targets given by the current target hydra
    void select_targets();

    // Replaces target i with j in the current leaf hydra
    void hydra_replace(int i, int j);

    // Splits target i into j1 and j2 in the current leaf hydra
    void hydra_split(int i, int j1, int j2);

    // Replaces a list of targets with a single target j in the current leaf hydra
    void hydra_replace_list(const std::vector<int>& targets, int j);

private:
    // Maps variable base names to their latest index
    std::unordered_map<std::string, int> var_indices;

    // Helper Function: Partitions a hydra based on shared variables and creates new hydras
    std::vector<std::shared_ptr<hydra>> partition_hydra(hydra& h);
};

// Generates renaming pairs for common variables based on the context
std::vector<std::pair<std::string, std::string>> vars_rename_list(context_t& ctx, const std::set<std::string>& common_vars);

void print_reason(const context_t& context, int index);

// Combine a pair of restrictions into a single restriction
std::vector<int> combine_restrictions(const std::vector<int>& res1, const std::vector<int>& res2);

// Check if restrictions are compatible
bool check_restrictions(const std::vector<int>& res1, const std::vector<int>& res2);

// Combine a pair of assumptions into a single set of assumptions
std::vector<int> combine_assumptions(const std::vector<int>& assm1, const std::vector<int>& assm2);

// Check if assumptions are compatible
bool check_assumptions(const std::vector<int>& assm1, const std::vector<int>& assm2);

#endif // CONTEXT_H

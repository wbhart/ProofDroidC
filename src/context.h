// context.h

#ifndef CONTEXT_H
#define CONTEXT_H

#include "node.h"
#include "hydra.h"
#include "debug.h"
#include <unordered_map>
#include <string>
#include <iostream>
#include <vector>
#include <set>
#include <utility>
#include <unordered_set>
#include <algorithm>
#include <memory>
#include <optional>

// Define the LIBRARY enum to distinguish between Theorem and Definition
enum class LIBRARY {
    Theorem,
    Definition
};

// Structure representing an entry in the digest
struct digest_item {
    size_t module_line_idx;        // Line index in the module
    size_t main_tableau_line_idx;  // Line index in the main tableau (-1 if not loaded)
    LIBRARY kind;                   // Kind of fact: Theorem or Definition

    // Constructor for ease of initialization
    digest_item(size_t mod_idx, size_t main_idx, LIBRARY k)
        : module_line_idx(mod_idx), main_tableau_line_idx(main_idx), kind(k) {}
};

// Enumeration representing reasons for justifications in tableau lines
enum class Reason {
    ModusPonens,
    ModusTollens,
    Target,
    Hypothesis,
    ConjunctiveIdempotence,
    DisjunctiveIdempotence,
    SplitConjunction,
    SplitDisjunction,
    SplitDisjunctiveImplication,
    SplitConjunctiveImplication,
    NegatedImplication,
    MaterialEquivalence,
    ConditionalPremise,
    Theorem,
    Definition
};

// Represents a single line in the tableau
class tabline_t {
public:
    bool target = false;                           // Indicates if this line is a target
    bool active = true;                            // Indicates if this line is active
    bool dead = false;                             // Whether target proved or hypothesis no longer useful
    bool ltor = false;                             // For implications, whether it can be applied left-to-right
    bool rtol = false;                             // or right-to-left, without introducing new metavars
    std::vector<int> assumptions;                  // Indices of assumptions
    std::vector<int> restrictions;                 // Indices of restrictions
    std::pair<Reason, std::vector<int>> justification; // How this was proved and from which lines
    node* formula = nullptr;                       // Pointer to the associated formula
    node* negation = nullptr;                      // Pointer to the negation of the formula
    std::vector<std::pair<int, int>> unifications; // List of pairs (i, j) where i unifies with j
    std::vector<std::string> constants1;           // Constants for line or constants on left of implication line
    std::vector<std::string> constants2;           // Constants right of implication line
    std::vector<int> applied_units;                // Tracks applied target indices
    std::vector<std::pair<std::string, size_t>> lib_applied; // library (name, index) pairs already applied to this unit

    // Constructor Initializer Lists to Match Declaration Order
    tabline_t(node* form) 
        : target(false), active(true), dead(false), formula(form), negation(nullptr), unifications(),
                                                            constants1(), constants2(), applied_units(), lib_applied() {}
    
    tabline_t(node* form, node* neg) 
        : target(true), active(true), dead(false), formula(form), negation(neg), unifications(),
                                                            constants1(), constants2(), applied_units(), lib_applied() {}

    // Print a list of restrictions
    void print_restrictions() const;

    // Print a list of restrictions
    void print_assumptions() const;

    // Return whether the tabline stores a loaded theorem
    bool is_theorem() const {
        return justification.first == Reason::Theorem;
    }

    // Return whether the tabline stores a loaded definition
    bool is_definition() const {
        return justification.first == Reason::Definition;
    }
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

    // Purges all hypotheses that can only be used to prove dead targets
    void purge_dead();

    // Array of tableau lines
    std::vector<tabline_t> tableau;

    // Hydra graph representing the target tree.
    // Initialized to be empty when a context_t instance is created.
    std::shared_ptr<hydra> hydra_graph;

    // Initializes hydras based on the tableau
    void initialize_hydras();

    // Sets current_hydra to the path from root to the first leaf in DFS
    // Now returns the list of target indices in the leaf hydra
    std::vector<int> get_hydra();

    // Path to current hydra (list of references to hydras along the way)
    std::vector<std::shared_ptr<hydra>> current_hydra;

    // Digest for library thms/defns when ctx is storing a module
    // Digest item has library kind and pair of size_t's
    // Pair (i, j): i = line in this tableau, j = line in main
    // tableau if theorem/definition line loaded there, else -1
    std::vector<std::vector<digest_item>> digest;
    
    // Vector of loaded modules: pair of filename stem and their context
    std::vector<std::pair<std::string, context_t>> modules;

    // Lines already dealt with (used for incremental completion checking)
    size_t upto = 0;

    // Selects and activates/deactivates targets and hypotheses based on the provided list
    void select_targets(const std::vector<int>& targets);

    // Selects targets given by the current target hydra
    void select_targets();

    // Select hypotheses that can prove the given targets and which are compatible with the given assumptions
    void select_hypotheses(const std::vector<int>& targets, const std::vector<int>& assumptions);

    // Replaces target i with j in the current leaf hydra
    void hydra_replace(int i, int j);

    // Update all restrictions including i to include j as well
    void restrictions_replace(int i, int j);

    // Splits target i into j1 and j2 in the current leaf hydra
    void hydra_split(int i, int j1, int j2);

    // Update all restrictions including i to include j1 and j2 as well
    void restrictions_split(int i, int j1, int j2);

    // Replaces a list of targets with a single target j in the current leaf hydra
    void hydra_replace_list(const std::vector<int>& targets, int j);

    // Update all restrictions including a target in the list to include j as well
    void restrictions_replace_list(const std::vector<int>& targets, int j);

    // Print hydra graph
    void print_hydras();

    // Populate the constants field of the tablines in the tableau
    // If a digest is present, only lines listed in digest are dealt with
    void get_constants();

    // Function to find a module by filename stem
    std::optional<context_t*> find_module(const std::string& filename_stem);

    // Return constants used in active (non-thm/defn) lines of tableau and constants used in active targets
    // along with a list of all active implications and unit clauses
    void get_tableau_constants(std::vector<std::string>& all_constants,
                               std::vector<std::string>& target_constants,
                               std::vector<size_t>& implication_indices,
                               std::vector<size_t>& unit_indices) const;

    // kill all duplicate lines starting at the given start line
    void kill_duplicates(size_t start_index);

    // Determine whether lines representing implications can be applied left-to-right or right-to-left
    void get_ltor();
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

// Check if restrictions are compatible and print an error if not
bool check_restrictions(const std::vector<int>& res1, const std::vector<int>& res2);

// Return true if restrictions are compatible
bool restrictions_compatible(const std::vector<int>& res1, const std::vector<int>& res2);

// Combine a pair of assumptions into a single set of assumptions
std::vector<int> combine_assumptions(const std::vector<int>& assm1, const std::vector<int>& assm2);

// Merge two assumptions lists, assuming they are already checked for compatibility
std::vector<int> merge_assumptions(const std::vector<int>& assm1, const std::vector<int>& assm2);

// Check if assumptions are compatible and print an error if not
bool check_assumptions(const std::vector<int>& assm1, const std::vector<int>& assm2);

// Return true if assumptions are compatible
bool assumptions_compatible(const std::vector<int>& assm1, const std::vector<int>& assm2);

// Print the tableau, showing only active lines
void print_tableau(const context_t& tab_ctx);

#endif // CONTEXT_H

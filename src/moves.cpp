// moves.cpp

#include "moves.h"
#include <set>
#include <algorithm>
#include <vector>

// Define DEBUG_CLEANUP to enable debug traces for cleanup moves
#define DEBUG_CLEANUP 0

// Parameterize function: changes all free individual variables to parameters
node* parameterize(node* formula) {
    // If the node is a free individual variable, change it to parameter
    if (formula->type == VARIABLE) {
        if (!formula->vdata->bound && formula->vdata->var_kind == INDIVIDUAL) {
            formula->vdata->var_kind = PARAMETER;
        }
    }

    // Recursively parameterize child nodes
    for (auto child : formula->children) {
        parameterize(child);
    }

    return formula;
}

void parameterize_all(context_t& tab_ctx) {
    // Iterate through each tabline in the tableau
    if (!tab_ctx.parameterized) {
        for (auto& tabline : tab_ctx.tableau) {
            // Only process active formulas
            if (tabline.active) {
                // Apply parameterize to the formula
                if (tabline.target) {
                    // Delete existing negation to prevent memory leaks
                    delete tabline.formula;

                    parameterize(tabline.negation);
                    
                    // Replace existing formula
                    tabline.formula = negate_node(deep_copy(tabline.negation));
                } else {
                    parameterize(tabline.formula);
                }
            }
        }
    }

    // Ensure parameterization is only done once
    tab_ctx.parameterized = true;
}

// Skolemizes an existentially quantified formula by replacing the existential variable
// with a Skolem function dependent on the provided universally quantified variables.
// Original formula is modified
node* skolemize(context_t& ctx, node* formula, const std::vector<std::string>& universals, Substitution& subst) {
    // Extract the existential variable and the inner formula \phi(x)
    node* var_node = formula->children[0];
    node* phi = formula->children[1];

    std::string existential_var = var_node->name();

    // Collect variables used in \phi(x)
    std::set<std::string> used_vars;
    vars_used(used_vars, phi);

    // Determine which universals are actually used in \phi(x)
    std::vector<std::string> used_universals;
    for (const auto& u_var : universals) {
        if (used_vars.find(u_var) != used_vars.end()) {
            used_universals.push_back(u_var);
        }
    }

    // Determine the base name for the Skolem function
    std::string skolem_func_base = remove_subscript(existential_var);
    std::string skolem_func_name;

    // Check if the base name exists in the context
    int skolem_index = ctx.get_next_index(skolem_func_base);
    skolem_func_name = append_subscript(skolem_func_base, skolem_index);

    if (used_universals.empty()) {
        node* skolem_const = new node(VARIABLE, skolem_func_name);
        skolem_const->vdata->var_kind = PARAMETER;

        // Add the substitution: existential_var -> skolem_const
        subst[existential_var] = skolem_const;
    } else {
        // Create the Skolem function symbol node
        node* fn_sym = new node(VARIABLE, skolem_func_name);
        fn_sym->vdata->var_kind = FUNCTION;
        fn_sym->vdata->arity = static_cast<int>(used_universals.size());

        // Construct the arguments for the Skolem function
        std::vector<node*> children;
        children.push_back(fn_sym); // Skolem function symbol
        for (const auto& u : used_universals) {
            node* arg = new node(VARIABLE, u); // Arguments of Skolem function
            children.push_back(arg);
        }

        // Create the Skolem function node: skolem_func(y, z, ...)
        node* skolem_func = new node(APPLICATION, children);

        // Add the substitution: existential_var -> skolem_func
        subst[existential_var] = skolem_func;
    }

    // Detach children to prevent deletion when deleting the quantifier node
    formula->children.clear();

    // Delete the existential quantifier node and no longer used variable
    delete formula;
    delete var_node;

    // Return the modified formula \phi(skolem_func)
    return phi;
}

// skolemize an arbitrary formula
// original formula is destroyed
node* skolem_form(context_t& ctx, node* formula) {
    Substitution subst; // Initialize empty substitution map
    std::vector<std::string> universals; // List of universally quantified variables

    // Process quantifiers until the outermost node is no longer a quantifier
    while (formula->type == QUANTIFIER) {
        if (formula->symbol == SYMBOL_FORALL) {
            // Extract the universal variable
            node* var_node = formula->children[0];
            std::string u = var_node->name();
            universals.push_back(u);

            // Set formula to the inner formula \phi
            node* inner_formula = formula->children[1];

            // Unbind all occurrences of variable
            unbind_var(inner_formula, var_node->name());

            formula->children.clear(); // Detach children to prevent deletion
            delete var_node; // Delete no longer used variable
            delete formula; // Delete the forall node

            formula = inner_formula; // Update formula to inner formula
        }
        else if (formula->symbol == SYMBOL_EXISTS) {
            // Skolemize the existential quantifier with current universals
            node* inner_phi = skolemize(ctx, formula, universals, subst);
            formula = inner_phi; // Update formula to the result of skolemize
        }
        else {
            // Unsupported quantifier symbol; handle as needed
            break;
        }
    }

    if (!subst.empty()) {
        // Apply all accumulated substitutions to the formula
        node* skolemized_formula = substitute(formula, subst);
        cleanup_subst(subst);
        delete formula;

        return skolemized_formula;
    } else {
        return formula;
    }
}

bool skolemize_all(context_t& tab_ctx, size_t start) {
    bool moved = false; // whether any move occurred
    
    for (size_t i = start; i < tab_ctx.tableau.size(); ++i) {
        tabline_t& tabline = tab_ctx.tableau[i];

       // Only process active formulas
        if (tabline.active) {
            // Apply skolem_form to the formula
            node* skolemized = skolem_form(tab_ctx, tabline.formula);

            if (skolemized != tabline.formula) { // if anything changed
                moved = true;

                // If the formula is a target, re-negate it
                if (!tabline.target) {
                    // Replace the original formula with the skolemized formula
                    tabline.formula = disjunction_to_implication(skolemized);
                } else {
                    // Replace the original formula with the skolemized formula
                    tabline.formula = skolemized;

                    // Delete existing negation to prevent memory leaks
                    delete tabline.negation;

                    // Create a deep copy of the skolemized formula
                    node* formula_copy = deep_copy(tabline.formula);

                    // Negate the copied formula
                    node* negated = negate_node(formula_copy);
                    negated = disjunction_to_implication(negated);

                    // Assign the negated formula to the negation field
                    tabline.negation = negated;
                }
            }
        }
    }

    return moved;
}

void cleanup_conjuncts(std::vector<node*> conjuncts) {
    for (auto conjunct : conjuncts)
        delete conjunct;
    conjuncts.clear();
}

node* modus_ponens(context_t& ctx_var, node* implication, const std::vector<node*>& unit_clauses) {
    // 1. Verify that the first formula is an implication
    if (!(implication->is_implication())) {
        std::cerr << "Error: The first formula is not an implication." << std::endl;
        return nullptr;
    }

    // 2. Deep copy the implication formula
    node* implication_copy = deep_copy(implication);

    // 3. Extract antecedent and consequent from the implication
    node* antecedent = implication_copy->children[0];
    node* consequent = implication_copy->children[1];

    // 5. Collect all variables from conjuncts and unit clauses
    std::set<std::string> vars_conjuncts;
    vars_used(vars_conjuncts, implication);

    std::set<std::string> vars_units;
    for (const auto& unit : unit_clauses) {
        vars_used(vars_units, unit, false);
    }

    // 6. Find common variables
    std::set<std::string> common_vars;
    std::set_intersection(
        vars_conjuncts.begin(), vars_conjuncts.end(),
        vars_units.begin(), vars_units.end(),
        std::inserter(common_vars, common_vars.begin())
    );

    // If there are common variables, rename them in the entire implication_copy
    if (!common_vars.empty()) {
        // Create a rename list based on common variables
        std::vector<std::pair<std::string, std::string>> rename_list = vars_rename_list(ctx_var, common_vars);

        // Rename variables in the entire implication_copy
        rename_vars(implication_copy, rename_list);
    }

    // 4. Flatten antecedent into a list of conjuncts
    std::vector<node*> conjuncts;
    conjuncts = conjunction_to_list(antecedent);

    // 7. Verify that the number of unit clauses matches the number of conjuncts
    if (unit_clauses.size() != conjuncts.size()) {
        std::cerr << "Error: Number of unit clauses (" << unit_clauses.size()
                  << ") does not match number of antecedent conjuncts (" << conjuncts.size() << ")." << std::endl;
        cleanup_conjuncts(conjuncts);
        delete implication_copy;
        return nullptr;
    }

    // 8. Collect all substitutions
    Substitution combined_subst; // Assume Substitution is a map from variable names to node*

    for (size_t i = 0; i < conjuncts.size(); ++i) {
        node* conjunct = conjuncts[i];
        node* unit = unit_clauses[i];

        // e. Unify the renamed conjunct with the unit clause
        Substitution subst;
        std::optional<Substitution> maybe_subst = unify(conjunct, unit, subst);
        if (!maybe_subst.has_value()) {
            std::cerr << "Error: Unification failed between conjunct " << (i + 1) << " and unit clause." << std::endl;
            std::cerr << "Conjunct: " << conjunct->to_string(UNICODE) 
                      << " | Unit Clause: " << unit->to_string(UNICODE) << std::endl;
            cleanup_conjuncts(conjuncts);
            delete implication_copy;
            cleanup_subst(combined_subst);
            return nullptr;
        }

        // f. Combine substitutions, ensuring no conflicts
        for (const auto& [key, value] : maybe_subst.value()) {
            // Check if the variable already has a substitution
            auto it = combined_subst.find(key);
            if (it != combined_subst.end()) {
                if (it->second->to_string(REPR) != value->to_string(REPR)) {
                    std::cerr << "Error: Conflicting substitutions for variable '" << key << "'." << std::endl;
                    cleanup_conjuncts(conjuncts);
                    delete implication_copy;
                    cleanup_subst(combined_subst);
                    return nullptr;
                }
                // Else, same substitution; no action needed
            }
            else {
                // Add the substitution to the combined substitution
                node* value_copy = deep_copy(value); // Deep copy to own the substitution

                combined_subst[key] = value_copy; // Store the deep copy
            }
        }
    }

    // g. Clean up the conjuncts
    cleanup_conjuncts(conjuncts);

    // 9. Substitute the consequent with the combined substitution
    node* substituted_consequent = substitute(consequent, combined_subst);
    if (!substituted_consequent) {
        std::cerr << "Error: Substitution failed on the consequent." << std::endl;
        // Clean up and exit
        delete implication_copy;
        cleanup_subst(combined_subst);
        return nullptr;
    }
    
    // 10. Clean up combined_subst and implication_copy
    cleanup_subst(combined_subst);
    delete implication_copy;

    return substituted_consequent;
}

node* modus_tollens(context_t& ctx_var, node* implication, const std::vector<node*>& unit_clauses) {
    // 1. Negate the implication: A -> B becomes ¬B -> ¬A
    node* negated_implication = contrapositive(implication);

    // 2. Apply modus ponens with the negated implication and the provided unit clauses
    node* result = modus_ponens(ctx_var, negated_implication, unit_clauses);

    // 3. Clean up the negated implication
    delete negated_implication;

    // 4. Return the result from modus ponens
    return result;
}

bool move_mpt(context_t& ctx, int implication_line, const std::vector<int>& other_lines, bool ponens) {
    // Step 1: Validate implication_line is within bounds
    if (implication_line < 0 || implication_line >= static_cast<int>(ctx.tableau.size())) {
        std::cerr << "Error: implication line " << implication_line + 1 << " is out of bounds.\n";
        return false;
    }

    // Reference is taken before modifying the vector
    tabline_t& implication_tabline = ctx.tableau[implication_line];

    // Step 2: Ensure implication_line is a hypothesis
    if (implication_tabline.target) {
        std::cerr << "Error: Line " << implication_line + 1 << " is not a hypothesis.\n";
        return false;
    }

    node* implication = implication_tabline.formula;
    if (!implication->is_implication()) {
        std::cerr << "Error: Line " << implication_line + 1 << " does not contain a valid implication.\n";
        return false;
    }

    // Step 3: Validate other_lines are within bounds and determine their nature
    bool all_hypotheses = true;
    bool all_targets = true;

    for (int line : other_lines) {
        if (line < 0 || line >= static_cast<int>(ctx.tableau.size())) {
            std::cerr << "Error: line " << line + 1 << " is out of bounds.\n";
            return false;
        }

        tabline_t& current_tabline = ctx.tableau[line];
        if (current_tabline.target) {
            all_hypotheses = false;
        } else {
            all_targets = false;
        }

        if (!check_assumptions(implication_tabline.assumptions, current_tabline.assumptions)) {
            std::cerr << "Error: line " << line + 1 << " has incompatible assumptions.\n";
            return false;
        }

        if (!check_restrictions(implication_tabline.restrictions, current_tabline.restrictions)) {
            std::cerr << "Error: line " << line + 1 << " has incompatible target restrictions.\n";
            return false;
        }
    }

    // Step 4: Determine the direction based on the nature of other_lines
    bool forward;
    if (all_hypotheses && !all_targets) {
        forward = true; // Apply modus ponens
    }
    else if (all_targets && !all_hypotheses) {
        forward = false; // Apply modus tollens
    }
    else {
        std::cerr << "Error: antecedents must be all hypotheses or all targets.\n";
        return false;
    }
    
    // Step 5: Extract unit clauses from other_lines
    std::vector<node*> unit_clauses;
    for (int line : other_lines) {
        node* clause = ctx.tableau[line].formula;
        unit_clauses.push_back(clause);
    }

    // Step 6: Apply the appropriate inference rule
    node* result = nullptr;
    Reason justification_reason;

    if (forward ^ !ponens) {
        // Apply modus ponens
        result = modus_ponens(ctx, implication, unit_clauses);
    }
    else {
        // Apply modus tollens
        result = modus_tollens(ctx, implication, unit_clauses);
    }

    justification_reason = (ponens ? Reason::ModusPonens : Reason::ModusTollens);

    // Step 7: Check if the inference was successful
    if (result == nullptr) {
        std::cerr << "Error: modus " << (ponens ? "ponens" : "tollens") << " failed to infer a result.\n";
        return false;
    }

    // Step 8: Create a new tabline for the result
    tabline_t new_tabline(result);

    if (forward) {
        // Insert as a hypothesis
        new_tabline.target = false;
        new_tabline.formula = disjunction_to_implication(new_tabline.formula);
    }
    else {
        // Insert as a target
        new_tabline.target = true;

        // Negate the result and store in the negation field
        node* neg_result = negate_node(deep_copy(result), true);
        new_tabline.negation = neg_result;
    }

    // Step 9: Set the justification
    std::vector<int> justification_lines;
    justification_lines.push_back(implication_line);
    justification_lines.insert(justification_lines.end(), other_lines.begin(), other_lines.end());
    new_tabline.justification = { justification_reason, justification_lines };

    // Step 10: Set the assumptions and restrictions
    new_tabline.assumptions = implication_tabline.assumptions;
    for (int line : other_lines) {
        new_tabline.assumptions = combine_assumptions(new_tabline.assumptions,
                                                  ctx.tableau[line].assumptions);
    }

    new_tabline.restrictions = implication_tabline.restrictions;
    for (int line : other_lines) {
        new_tabline.restrictions = combine_restrictions(new_tabline.restrictions,
                                                  ctx.tableau[line].restrictions);
    }

    // Step 11: Append the new tabline to the tableau
    ctx.tableau.push_back(new_tabline);

    if (!forward) {
        ctx.hydra_replace_list(other_lines, ctx.tableau.size() - 1);
        ctx.restrictions_replace_list(other_lines, ctx.tableau.size() - 1);
        ctx.select_targets();
    }

    return true;
}

// Function to check for Disjunctive Idempotence: P ∨ P
bool disjunctive_idempotence(const node* formula) {
    // Ensure the formula is a logical binary operation with OR symbol
    if (!formula->is_disjunction())
        return false;

    // Since nodes have the correct number of children, no need to check size
    const node* left = formula->children[0];
    const node* right = formula->children[1];

    // Check if both operands are structurally equal (up to variable renaming)
    return equal(left, right);
}

// Function to check for Implicative Idempotence: ¬P -> P
bool implicative_idempotence(const node* formula) {
    // Ensure the formula is a logical binary operation with OR symbol
    if (!formula->is_implication())
        return false;

    // Since nodes have the correct number of children, no need to check size
    const node* left = formula->children[0];
    const node* right = formula->children[1];

    // Check if both operands are structurally equal (up to variable renaming)
    node* negation = negate_node(deep_copy(left));
    bool res = equal(negation, right);
    delete negation;
    return res;
}

// Function to check for Conjunctive Idempotence: P ∧ P
bool conjunctive_idempotence(const node* formula) {
    // Ensure the formula is a logical binary operation with AND symbol
    if (!formula->is_conjunction())
        return false;

    // Since nodes have the correct number of children, no need to check size
    const node* left = formula->children[0];
    const node* right = formula->children[1];

    // Check if both operands are structurally equal (up to variable renaming)
    return equal(left, right);
}

bool move_di(context_t& tab_ctx, size_t start) {
    bool moved = false;
    size_t i = start;

    while (i < tab_ctx.tableau.size()) {
        tabline_t& tabline = tab_ctx.tableau[i];

        if (!tabline.active) {
            i++;
            continue;
        }

        // Check if the formula is a disjunctive idempotent
        if ((tabline.target && conjunctive_idempotence(tabline.formula))
            || (!tabline.target && disjunctive_idempotence(tabline.formula))
            || (!tabline.target && implicative_idempotence(tabline.formula))) {
            // Formula is of the form P ∨ P or P ∧ P

            // **Critical Fix: Set flags before modifying the vector**
            // Mark the original conjunction/disjunction as inactive and dead
            tabline.active = false;
            tabline.dead = true;

            // Store the original formula node and its children
            node* original_formula = tabline.formula;
            node* P = original_formula->children[1];

            if (!tabline.target) {
                // Original is a hypothesis, new tablines are hypotheses
                tabline_t new_tabline_P(deep_copy(P));
                
                // Copy restrictions and assumptions
                new_tabline_P.assumptions = tabline.assumptions;
                new_tabline_P.restrictions = tabline.restrictions;
                
                // Set justification to DisjunctiveIdempotence or ConjunctiveIdempotence
                Reason justification = tabline.formula->is_disjunction() ? Reason::DisjunctiveIdempotence : Reason::ConjunctiveIdempotence;
                new_tabline_P.justification = { justification, { static_cast<int>(i) } };
                
                // Append new hypotheses to the tableau
                tab_ctx.tableau.push_back(new_tabline_P);
            }
            else {
                // Original is a target, new tablines are targets
                node* neg_P = negate_node(deep_copy(P), true);

                tabline_t new_tabline_P(deep_copy(P), neg_P);

                // Copy restrictions and assumptions
                new_tabline_P.assumptions = tabline.assumptions;
                new_tabline_P.restrictions = tabline.restrictions;
                
                // Set justification to DisjunctiveIdempotence or ConjunctiveIdempotence
                Reason justification = tabline.formula->is_disjunction() ? Reason::DisjunctiveIdempotence : Reason::ConjunctiveIdempotence;
                new_tabline_P.justification = { justification, { static_cast<int>(i) } };

                // Append new targets to the tableau
                tab_ctx.tableau.push_back(new_tabline_P);

                // Replace hydra
                tab_ctx.hydra_replace(i, tab_ctx.tableau.size() - 1);
                tab_ctx.restrictions_replace(i, tab_ctx.tableau.size() - 1);
                tab_ctx.select_targets();
            }

            moved = true;
        }

        i++;
    }

    return moved;
}

bool move_ci(context_t& tab_ctx, size_t start) {
    bool moved = false;
    size_t i = start;

    while (i < tab_ctx.tableau.size()) {
        tabline_t& tabline = tab_ctx.tableau[i];

        if (!tabline.active) {
            i++;
            continue;
        }

        // Check if the formula is a conjunctive idempotent
        if ((tabline.target && disjunctive_idempotence(tabline.formula))
            || (!tabline.target && conjunctive_idempotence(tabline.formula))) {
            // Formula is of the form P ∧ P or P ∨ P

            // **Critical Fix: Set flags before modifying the vector**
            // Mark the original conjunction/disjunction as inactive and dead
            tabline.active = false;
            tabline.dead = true;

            // Store the original formula node and its children
            node* original_formula = tabline.formula;
            node* P = original_formula->children[0];

            if (!tabline.target) {
                // Original is a hypothesis, new tablines are hypotheses
                tabline_t new_tabline_P(deep_copy(P));
                
                // Copy restrictions and assumptions
                new_tabline_P.assumptions = tabline.assumptions;
                new_tabline_P.restrictions = tabline.restrictions;
                
                // Set justification to ConjunctiveIdempotence or DisjunctiveIdempotence
                Reason justification = tabline.formula->is_conjunction() ? Reason::ConjunctiveIdempotence : Reason::DisjunctiveIdempotence;
                new_tabline_P.justification = { justification, { static_cast<int>(i) } };
                
                // Append new hypotheses to the tableau
                tab_ctx.tableau.push_back(new_tabline_P);
            }
            else {
                // Original is a target, new tablines are targets
                node* neg_P = negate_node(deep_copy(P), true);

                tabline_t new_tabline_P(deep_copy(P), neg_P);

                // Copy restrictions and assumptions
                new_tabline_P.assumptions = tabline.assumptions;
                new_tabline_P.restrictions = tabline.restrictions;
                
                // Set justification to ConjunctiveIdempotence or DisjunctiveIdempotence
                Reason justification = tabline.formula->is_conjunction() ? Reason::ConjunctiveIdempotence : Reason::DisjunctiveIdempotence;
                new_tabline_P.justification = { justification, { static_cast<int>(i) } };

                // Append new targets to the tableau
                tab_ctx.tableau.push_back(new_tabline_P);
                
                // Replace hydra
                tab_ctx.hydra_replace(i, tab_ctx.tableau.size() - 1);
                tab_ctx.restrictions_replace(i, tab_ctx.tableau.size() - 1);
                tab_ctx.select_targets();
            }

            moved = true;
        }

        i++;
    }

    return moved;
}

// Split conjunctions
bool move_sc(context_t& tab_ctx, size_t start) {
    bool moved = false;
    size_t i = start;

    // Optional: Reserve capacity to prevent reallocations
    size_t expected_additions = tab_ctx.tableau.size() - start;
    tab_ctx.tableau.reserve(tab_ctx.tableau.size() + expected_additions * 2); // Adjust based on expected splits

    while (i < tab_ctx.tableau.size()) {
        tabline_t& tabline = tab_ctx.tableau[i];

        // Check if the formula is active and a conjunction or disjunction
        if (tabline.active && ((!tabline.target && tabline.formula->is_conjunction()) ||
            (tabline.target && tabline.formula->is_disjunction()))) {
            
            // Mark the original conjunction as inactive and dead BEFORE modifying the vector
            tabline.active = false;
            tabline.dead = true;

            // Proceed with splitting the conjunction/disjunction
            node* P = tabline.formula->children[0];
            node* Q = tabline.formula->children[1];

            if (!tabline.target) {
                // Original is a hypothesis, new tablines are hypotheses
                tabline_t new_tabline_P(deep_copy(P));
                tabline_t new_tabline_Q(deep_copy(Q));

                new_tabline_P.assumptions = tabline.assumptions;
                new_tabline_P.restrictions = tabline.restrictions;
                new_tabline_Q.assumptions = tabline.assumptions;
                new_tabline_Q.restrictions = tabline.restrictions;
                
                // Set justification to SplitConjunction
                new_tabline_P.justification = { Reason::SplitConjunction, { static_cast<int>(i) } };
                new_tabline_Q.justification = { Reason::SplitConjunction, { static_cast<int>(i) } };

                // Append new hypotheses to the tableau
                tab_ctx.tableau.push_back(new_tabline_P);
                tab_ctx.tableau.push_back(new_tabline_Q);
            }
            else {
                // Original is a target, new tablines are targets
                node* neg_P = negate_node(deep_copy(P), true);
                node* neg_Q = negate_node(deep_copy(Q), true);

                tabline_t new_tabline_P(deep_copy(P), neg_P);
                tabline_t new_tabline_Q(deep_copy(Q), neg_Q);

                // Copy restrictions and assumptions
                new_tabline_P.assumptions = tabline.assumptions;
                new_tabline_P.restrictions = tabline.restrictions;
                new_tabline_Q.assumptions = tabline.assumptions;
                new_tabline_Q.restrictions = tabline.restrictions;
                
                // Set justification to SplitConjunction
                new_tabline_P.justification = { Reason::SplitConjunction, { static_cast<int>(i) } };
                new_tabline_Q.justification = { Reason::SplitConjunction, { static_cast<int>(i) } };

                // Append new targets to the tableau
                tab_ctx.tableau.push_back(new_tabline_P);
                tab_ctx.tableau.push_back(new_tabline_Q);

                // Split the hydra
                tab_ctx.hydra_split(i, tab_ctx.tableau.size() - 2, tab_ctx.tableau.size() - 1);
                tab_ctx.restrictions_split(i, tab_ctx.tableau.size() - 2, tab_ctx.tableau.size() - 1);

                // Select targets based on the updated hydra graph
                tab_ctx.select_targets();
            }

            // Indicate that a move was made
            moved = true;
        }

        i++;
    }

    return moved;
}

bool move_sdi(context_t& tab_ctx, size_t start) {
    bool moved = false;
    size_t i = start;

    while (i < tab_ctx.tableau.size()) {
        tabline_t& tabline = tab_ctx.tableau[i];

        // Skip inactive formulas
        if (!tabline.active) {
            i++;
            continue;
        }

        if (!tabline.target) {
            // **Hypothesis Case:** Look for formulas of the form (P ∨ Q) → R
            if (tabline.formula->is_implication()) {
                node* left = tabline.formula->children[0];  // (P ∨ Q)
                node* right = tabline.formula->children[1]; // R

                if (left->is_disjunction()) {
                    node* P = left->children[0];
                    node* Q = left->children[1];
                    node* R = right;

                    // Collect variables used in R, P, and Q using vars_used
                    std::set<std::string> vars_R, vars_P, vars_Q;
                    vars_used(vars_R, R);
                    vars_used(vars_P, P);
                    vars_used(vars_Q, Q);

                    // Check if all variables in R are used in P and in Q
                    bool valid = true;
                    for (const auto& var : vars_R) {
                        if (vars_P.find(var) == vars_P.end() || vars_Q.find(var) == vars_Q.end()) {
                            valid = false;
                            break;
                        }
                    }

                    if (valid) {
                        // **Critical Fix: Set flags before modifying the vector**
                        // Mark the original implication as inactive and dead
                        tabline.active = false;
                        tabline.dead = true;

                        // Deep copy P, Q, R
                        node* P_copy = deep_copy(P);
                        node* Q_copy = deep_copy(Q);
                        node* R_copy1 = deep_copy(R);
                        node* R_copy2 = deep_copy(R);

                        if (!equal(P_copy, R_copy1)) {
                            // Create new implication P → R
                            node* P_imp_R = new node(LOGICAL_BINARY, SYMBOL_IMPLIES, std::vector<node*>{ P_copy, R_copy1 });
                            
                            // Create new tabline as hypothesis
                            tabline_t new_tabline_P_imp(P_imp_R);
                            
                            // Copy restrictions and assumptions
                            new_tabline_P_imp.assumptions = tabline.assumptions;
                            new_tabline_P_imp.restrictions = tabline.restrictions;
                            
                            // Set justification
                            new_tabline_P_imp.justification = { Reason::SplitDisjunctiveImplication, { static_cast<int>(i) } };
                            
                            // Append new tabline to the tableau
                            tab_ctx.tableau.push_back(new_tabline_P_imp);
                        } else {
                            // clean up unused nodes
                            delete P_copy;
                            delete R_copy1;
                        }

                        if (!equal(Q_copy, R_copy2)) {
                            // Create new implication Q → R
                            node* Q_imp_R = new node(LOGICAL_BINARY, SYMBOL_IMPLIES, std::vector<node*>{ Q_copy, R_copy2 });

                            // Create new tabline as hypothesis
                            tabline_t new_tabline_Q_imp(Q_imp_R);

                            // Copy restrictions and assumptions
                            new_tabline_Q_imp.assumptions = tabline.assumptions;
                            new_tabline_Q_imp.restrictions = tabline.restrictions;
                    
                            // Set justification
                            new_tabline_Q_imp.justification = { Reason::SplitDisjunctiveImplication, { static_cast<int>(i) } };

                            // Append new tabline to the tableau
                            tab_ctx.tableau.push_back(new_tabline_Q_imp);
                        } else {
                            // clean up unused nodes
                            delete Q_copy;
                            delete R_copy2;
                        }

                        moved = true;
                    }
                }
            }
        }
        else {
            // **Target Case:** Look for formulas of the form (P ∨ Q) ∧ ¬R, which represent ¬((P ∨ Q) → R)
            if (tabline.formula->is_conjunction()) {
                node* left = tabline.formula->children[0];  // (P ∨ Q)
                node* right = tabline.formula->children[1]; // ¬R

                // Check if left is a disjunction and right is a negation
                if (left->is_disjunction()) {
                    node* P = left->children[0];
                    node* Q = left->children[1];
                    node* R = right->children[0];

                    // Collect variables used in R, P, and Q using vars_used
                    std::set<std::string> vars_R, vars_P, vars_Q;
                    vars_used(vars_R, R);
                    vars_used(vars_P, P);
                    vars_used(vars_Q, Q);

                    // Check if all variables in R are used in P and in Q
                    bool valid = true;
                    for (const auto& var : vars_R) {
                        if (vars_P.find(var) == vars_P.end() || vars_Q.find(var) == vars_Q.end()) {
                            valid = false;
                            break;
                        }
                    }

                    if (valid) {
                        bool tar1 = false; // whether to create target 1 and 2
                        bool tar2 = false;

                        // **Critical Fix: Set flags before modifying the vector**
                        // Mark the original target as inactive and dead
                        tabline.active = false;
                        tabline.dead = true;

                        // Deep copy P, Q, R
                        node* P_copy = deep_copy(P);
                        node* Q_copy = deep_copy(Q);
                        node* R_copy1 = deep_copy(R);
                        node* R_copy2 = deep_copy(R);

                        if (!equal(P_copy, R_copy1)) {
                            // Create new implication P → R
                            node* P_imp_R = new node(LOGICAL_BINARY, SYMBOL_IMPLIES, std::vector<node*>{ P_copy, R_copy1 });

                            // Create new tablines as target
                            tabline_t new_tabline_neg_P_imp(negate_node(deep_copy(P_imp_R)), P_imp_R);
                            
                            // Copy restrictions and assumptions
                            new_tabline_neg_P_imp.assumptions = tabline.assumptions;
                            new_tabline_neg_P_imp.restrictions = tabline.restrictions;
                            
                            // Set justification
                            new_tabline_neg_P_imp.justification = { Reason::SplitDisjunctiveImplication, { static_cast<int>(i) } };
                            
                            // Append new tabline to the tableau
                            tab_ctx.tableau.push_back(new_tabline_neg_P_imp);

                            // target 1 created
                            tar1 = true;
                        }
                        
                        if (!equal(Q_copy, R_copy2)) {
                            // Create new implication Q → R
                            node* Q_imp_R = new node(LOGICAL_BINARY, SYMBOL_IMPLIES, std::vector<node*>{ Q_copy, R_copy2 });

                            // Create new tablines as target
                            tabline_t new_tabline_neg_Q_imp(negate_node(deep_copy(Q_imp_R)), Q_imp_R);

                            // Copy restrictions and assumptions
                            new_tabline_neg_Q_imp.assumptions = tabline.assumptions;
                            new_tabline_neg_Q_imp.restrictions = tabline.restrictions;
                    
                            // Set justification
                            new_tabline_neg_Q_imp.justification = { Reason::SplitDisjunctiveImplication, { static_cast<int>(i) } };

                            // Append new tabline to the tableau
                            tab_ctx.tableau.push_back(new_tabline_neg_Q_imp);

                            // target 2 created
                            tar2 = true;
                        }

                        if (tar1 && tar2) // both targets created
                        {
                            // Split the hydra and select targets
                            tab_ctx.hydra_split(i, tab_ctx.tableau.size() - 2, tab_ctx.tableau.size() - 1);
                            tab_ctx.restrictions_split(i, tab_ctx.tableau.size() - 2, tab_ctx.tableau.size() - 1);
                            tab_ctx.select_targets();

                            moved = true;
                        } else if (tar1 || tar2) { // one target created
                            // Replace the hydra and select targets
                            tab_ctx.hydra_replace(i, tab_ctx.tableau.size() - 1);
                            tab_ctx.restrictions_replace(i, tab_ctx.tableau.size() - 1);
                            tab_ctx.select_targets();

                            moved = true;
                        }
                    }
                }
            }
        }

        i++;
    }

    return moved;
}

bool move_sci(context_t& tab_ctx, size_t start) {
    bool moved = false;
    size_t i = start;

    while (i < tab_ctx.tableau.size()) {
        tabline_t& tabline = tab_ctx.tableau[i];

        // Skip inactive formulas
        if (!tabline.active) {
            i++;
            continue;
        }

        if (!tabline.target) {
            // **Hypothesis Case:** Look for formulas of the form P → (Q ∧ R)
            if (tabline.formula->is_implication()) {
                node* antecedent = tabline.formula->children[0];  // P
                node* consequent = tabline.formula->children[1];  // (Q ∧ R)

                if (consequent->is_conjunction()) {
                    node* Q = consequent->children[0];
                    node* R = consequent->children[1];

                    // Collect variables used in Q, R, and P using vars_used
                    std::set<std::string> vars_Q, vars_R, vars_P;
                    vars_used(vars_Q, Q, true);
                    vars_used(vars_R, R, true);
                    vars_used(vars_P, antecedent, true);

                    // Check if all variables in Q and R are used in P
                    bool valid = true;
                    for (const auto& var : vars_Q) {
                        if (vars_P.find(var) == vars_P.end()) {
                            valid = false;
                            break;
                        }
                    }
                    if (valid) {
                        for (const auto& var : vars_R) {
                            if (vars_P.find(var) == vars_P.end()) {
                                valid = false;
                                break;
                            }
                        }
                    }

                    if (valid) {
                        // **Critical Fix: Set flags before modifying the vector**
                        // Mark the original implication as inactive and dead
                        tabline.active = false;
                        tabline.dead = true;

                        // Deep copy P, Q, R
                        node* P_copy1 = deep_copy(antecedent);
                        node* P_copy2 = deep_copy(antecedent);
                        node* Q_copy = deep_copy(Q);
                        node* R_copy = deep_copy(R);

                        if (!equal(P_copy1, Q_copy)) {
                            // Create new implications P → Q
                            node* P_imp_Q = new node(LOGICAL_BINARY, SYMBOL_IMPLIES, std::vector<node*>{ P_copy1, Q_copy });
                            
                            // Create new tabline as hypothesis
                            tabline_t new_tabline_P_imp_Q(P_imp_Q);
                            
                            // Copy restrictions and assumptions
                            new_tabline_P_imp_Q.assumptions = tabline.assumptions;
                            new_tabline_P_imp_Q.restrictions = tabline.restrictions;
                            
                            // Set justification
                            new_tabline_P_imp_Q.justification = { Reason::SplitConjunctiveImplication, { static_cast<int>(i) } };
                            
                            // Append new tabline to the tableau
                            tab_ctx.tableau.push_back(new_tabline_P_imp_Q);
                        } else {
                            // clean up unused nodes
                            delete P_copy1;
                            delete Q_copy;
                        }

                        if (!equal(P_copy2, R_copy)) {
                            // Create new implications P → R
                            node* P_imp_R = new node(LOGICAL_BINARY, SYMBOL_IMPLIES, std::vector<node*>{ P_copy2, R_copy });

                            // Create new tabline as hypothesis
                            tabline_t new_tabline_P_imp_R(P_imp_R);

                            // Copy restrictions and assumptions
                            new_tabline_P_imp_R.assumptions = tabline.assumptions;
                            new_tabline_P_imp_R.restrictions = tabline.restrictions;
                
                            // Set justification
                            new_tabline_P_imp_R.justification = { Reason::SplitConjunctiveImplication, { static_cast<int>(i) } };

                            // Append new tabline to the tableau
                            tab_ctx.tableau.push_back(new_tabline_P_imp_R);
                        } else {
                            // clean up unused nodes
                            delete P_copy2;
                            delete R_copy;
                        }

                        // Indicate that a move was made
                        moved = true;
                    }
                }
            }
        }
        else {
            // **Target Case:** Look for formulas of the form P ∧ (Q ∨ R)
            if (tabline.formula->is_conjunction()) {
                node* P = tabline.formula->children[0];             // P
                node* disjunct = tabline.formula->children[1];      // (Q ∨ R)

                if (disjunct->is_disjunction()) {
                    node* Q = disjunct->children[0];
                    node* R = disjunct->children[1];

                    // Collect variables used in Q, R, and P using vars_used
                    std::set<std::string> vars_Q, vars_R, vars_P;
                    vars_used(vars_Q, Q, true);
                    vars_used(vars_R, R, true);
                    vars_used(vars_P, P, true);

                    // Check if all variables in Q and R are used in P
                    bool valid = true;
                    for (const auto& var : vars_Q) {
                        if (vars_P.find(var) == vars_P.end()) {
                            valid = false;
                            break;
                        }
                    }
                    if (valid) {
                        for (const auto& var : vars_R) {
                            if (vars_P.find(var) == vars_P.end()) {
                                valid = false;
                                break;
                            }
                        }
                    }

                    if (valid) {
                        bool tar1 = false; // whether to create targets 1 and 2
                        bool tar2 = false;

                        // **Critical Fix: Set flags before modifying the vector**
                        // Mark the original target as inactive and dead
                        tabline.active = false;
                        tabline.dead = true;

                        // Deep copy P, Q, R
                        node* P_copy1 = deep_copy(P);
                        node* P_copy2 = deep_copy(P);
                        node* Q_copy = deep_copy(Q);
                        node* R_copy = deep_copy(R);

                        if (!equal(P_copy1, Q_copy)) {
                            // Create new conjunction P ∧ Q
                            node* P_and_Q = new node(LOGICAL_BINARY, SYMBOL_AND, std::vector<node*>{ P_copy1, Q_copy });
                            
                            // negate node
                            node* neg_P_and_Q = negate_node(deep_copy(P_and_Q), true);
                            
                            // Create new tabline as targets
                            tabline_t new_tabline_neg_P_and_Q(P_and_Q, neg_P_and_Q);
                            
                            // Copy restrictions and assumptions
                            new_tabline_neg_P_and_Q.assumptions = tabline.assumptions;
                            new_tabline_neg_P_and_Q.restrictions = tabline.restrictions;
                            
                            // Set justification
                            new_tabline_neg_P_and_Q.justification = { Reason::SplitConjunctiveImplication, { static_cast<int>(i) } };
                            
                            // Append new tabline to the tableau
                            tab_ctx.tableau.push_back(new_tabline_neg_P_and_Q);

                            // target 1 created
                            tar1 = true;
                        }
                        
                        if (!equal(P_copy2, R_copy)) {
                            // Create new conjunction P ∧ R
                            node* P_and_R = new node(LOGICAL_BINARY, SYMBOL_AND, std::vector<node*>{ P_copy2, R_copy });

                            // negate node
                            node* neg_P_and_R = negate_node(deep_copy(P_and_R), true);

                            // Create new tabline as targets
                            tabline_t new_tabline_neg_P_and_R(P_and_R, neg_P_and_R);

                            // Copy restrictions and assumptions
                            new_tabline_neg_P_and_R.assumptions = tabline.assumptions;
                            new_tabline_neg_P_and_R.restrictions = tabline.restrictions;
                    
                            // Set justification
                            new_tabline_neg_P_and_R.justification = { Reason::SplitConjunctiveImplication, { static_cast<int>(i) } };

                            // Append new tabline to the tableau
                            tab_ctx.tableau.push_back(new_tabline_neg_P_and_R);

                            // target 2 created
                            tar2 = true;
                        }

                        if (tar1 && tar2) // both targets created
                        {
                            // Split the hydra and select targets
                            tab_ctx.hydra_split(i, tab_ctx.tableau.size() - 2, tab_ctx.tableau.size() - 1);
                            tab_ctx.restrictions_split(i, tab_ctx.tableau.size() - 2, tab_ctx.tableau.size() - 1);
                            tab_ctx.select_targets();

                            moved = true;
                        } else if (tar1 || tar2) { // one target created
                            // Replace the hydra and select targets
                            tab_ctx.hydra_replace(i, tab_ctx.tableau.size() - 1);
                            tab_ctx.restrictions_replace(i, tab_ctx.tableau.size() - 1);
                            tab_ctx.select_targets();

                            moved = true;
                        }
                    }
                }
            }
        }

        i++;
    }

    return moved;
}

bool move_ni(context_t& tab_ctx, size_t start) {
    bool moved = false;
    size_t i = start;

    while (i < tab_ctx.tableau.size()) {
        tabline_t& tabline = tab_ctx.tableau[i];

        // Skip inactive formulas
        if (!tabline.active) {
            i++;
            continue;
        }

        if (!tabline.target) {
            // **Hypothesis Case:** Look for formulas of the form ¬(P → Q)
            if (tabline.formula->is_negation()) {
                node* inner = tabline.formula->children[0];
                if (inner->is_implication()) {
                    node* P = inner->children[0];
                    node* Q = inner->children[1];

                    // Collect variables used in Q, P
                    std::set<std::string> vars_Q, vars_P;
                    vars_used(vars_Q, Q, true);
                    vars_used(vars_P, P, true);

                    // Check if all variables in Q are used in P
                    bool valid = true;
                    for (const auto& var : vars_Q) {
                        if (vars_P.find(var) == vars_P.end()) {
                            valid = false;
                            break;
                        }
                    }

                    if (valid) {
                        // **Critical Fix: Set flags before modifying the vector**
                        // Mark the original negated implication as inactive and dead
                        tabline.active = false;
                        tabline.dead = true;

                        // Deep copy P, Q
                        node* P_copy = deep_copy(P);
                        node* Q_copy = deep_copy(Q);
                        node* neg_Q_copy = negate_node(deep_copy(Q));

                        // Create new hypothesis P
                        tabline_t new_hypothesis_P(P_copy);
                        new_hypothesis_P.target = false;
                        new_hypothesis_P.active = true;
                        new_hypothesis_P.justification = { Reason::NegatedImplication, { static_cast<int>(i) } };

                        // Create new target Q
                        tabline_t new_target_Q(neg_Q_copy, Q_copy); // Assuming constructor takes formula and negation
                        new_target_Q.target = true;
                        new_target_Q.active = true;
                        new_target_Q.justification = { Reason::NegatedImplication, { static_cast<int>(i) } };

                        // Copy restrictions and assumptions
                        new_hypothesis_P.assumptions = tabline.assumptions;
                        new_hypothesis_P.restrictions = tabline.restrictions;
                        new_target_Q.assumptions = tabline.assumptions;
                        new_target_Q.restrictions = tabline.restrictions;
                    
                        // Append new hypothesis and target to the tableau
                        tab_ctx.tableau.push_back(new_hypothesis_P);
                        tab_ctx.tableau.push_back(new_target_Q);

                        // Add restriction to the new hypothesis
                        new_hypothesis_P.restrictions.push_back(static_cast<int>(tab_ctx.tableau.size() - 1));

                        moved = true;
                    }
                }
            }
        }
        else {
            // **Target Case:** Look for formulas of the form P → Q
            if (tabline.formula->is_implication()) {
                node* P = tabline.formula->children[0];
                node* Q = tabline.formula->children[1];

                // Deep copy P and Q
                node* P_copy = deep_copy(P);
                node* Q_copy = deep_copy(Q);
                P_copy = disjunction_to_implication(P_copy);
                node* neg_P_copy = negate_node(deep_copy(P));
                node* neg_Q_copy = negate_node(deep_copy(Q), true);

                // Create new target tablines
                tabline_t new_tabline_neg_P(neg_P_copy, P_copy); // Assuming constructor takes formula and negation
                tabline_t new_tabline_neg_Q(Q_copy, neg_Q_copy);

                // Copy restrictions and assumptions
                new_tabline_neg_P.assumptions = tabline.assumptions;
                new_tabline_neg_P.restrictions = tabline.restrictions;
                new_tabline_neg_Q.assumptions = tabline.assumptions;
                new_tabline_neg_Q.restrictions = tabline.restrictions;
                
                // Set justifications
                new_tabline_neg_P.justification = { Reason::NegatedImplication, { static_cast<int>(i) } };
                new_tabline_neg_Q.justification = { Reason::NegatedImplication, { static_cast<int>(i) } };

                // Append new target tablines to the tableau
                tab_ctx.tableau.push_back(new_tabline_neg_P);
                tab_ctx.tableau.push_back(new_tabline_neg_Q);

                // **Critical Fix: Set flags before hydra operations**
                // Already set flags before modifying the vector

                // Mark the original implication as inactive and dead
                tabline.active = false;
                tabline.dead = true;

                // Split the hydra and select targets
                tab_ctx.hydra_split(i, tab_ctx.tableau.size() - 2, tab_ctx.tableau.size() - 1);
                tab_ctx.restrictions_split(i, tab_ctx.tableau.size() - 2, tab_ctx.tableau.size() - 1);
                tab_ctx.select_targets();

                moved = true;
            }
        }

        i++;
    }

    return moved;
}

bool move_me(context_t& tab_ctx, size_t start) {
    bool moved = false;
    size_t i = start;

    while (i < tab_ctx.tableau.size()) {
        tabline_t& tabline = tab_ctx.tableau[i];

        // Skip inactive formulas
        if (!tabline.active) {
            i++;
            continue;
        }

        if (!tabline.target) {
            // **Target Case:** Look for formulas of the form P ↔ Q
            if (tabline.formula->is_equivalence()) {
                node* P = tabline.formula->children[0];
                node* Q = tabline.formula->children[1];

                // Deep copy P and Q
                node* P_copy1 = deep_copy(P);
                node* P_copy2 = deep_copy(P);
                node* Q_copy1 = deep_copy(Q);
                node* Q_copy2 = deep_copy(Q);
                std::vector<node*> children1, children2;
                children1.push_back(P_copy1);
                children1.push_back(Q_copy1);
                children2.push_back(Q_copy2);
                children2.push_back(P_copy2);
                node* P_implies_Q = new node(LOGICAL_BINARY, SYMBOL_IMPLIES, children1);
                node* Q_implies_P = new node(LOGICAL_BINARY, SYMBOL_IMPLIES, children2);
                
                // Create new target tablines
                tabline_t new_tabline_P_implies_Q(P_implies_Q);
                tabline_t new_tabline_Q_implies_P(Q_implies_P);
                
                // Copy restrictions and assumptions
                new_tabline_P_implies_Q.assumptions = tabline.assumptions;
                new_tabline_P_implies_Q.restrictions = tabline.restrictions;
                new_tabline_Q_implies_P.assumptions = tabline.assumptions;
                new_tabline_Q_implies_P.restrictions = tabline.restrictions;
                
                // Set justifications
                new_tabline_P_implies_Q.justification = { Reason::MaterialEquivalence, { static_cast<int>(i) } };
                new_tabline_Q_implies_P.justification = { Reason::MaterialEquivalence, { static_cast<int>(i) } };

                // Mark the original equivalence as inactive and dead
                tabline.active = false;
                tabline.dead = true;

                // Append new target tablines to the tableau
                tab_ctx.tableau.push_back(new_tabline_P_implies_Q);
                tab_ctx.tableau.push_back(new_tabline_Q_implies_P);

                moved = true;
            }
        }
        else {
            // **Target Case:** Look for formulas of the form P ↔ Q
            if (tabline.negation->is_equivalence()) {
                node* P = tabline.negation->children[0];
                node* Q = tabline.negation->children[1];

                // Deep copy P and Q
                node* P_copy1 = deep_copy(P);
                node* P_copy2 = deep_copy(P);
                node* Q_copy1 = deep_copy(Q);
                node* Q_copy2 = deep_copy(Q);
                std::vector<node*> children1, children2;
                children1.push_back(P_copy1);
                children1.push_back(Q_copy1);
                children2.push_back(Q_copy2);
                children2.push_back(P_copy2);
                node* P_implies_Q = new node(LOGICAL_BINARY, SYMBOL_IMPLIES, children1);
                node* Q_implies_P = new node(LOGICAL_BINARY, SYMBOL_IMPLIES, children2);

                std::set<std::string> common_vars = find_common_variables(P_implies_Q, Q_implies_P);

                // If there are common variables, rename them in the entire implication_copy
                if (!common_vars.empty()) {
                    // Create a rename list based on common variables
                    std::vector<std::pair<std::string, std::string>> rename_list = vars_rename_list(tab_ctx, common_vars);

                    // Rename variables in the entire implication_copy
                     rename_vars(Q_implies_P, rename_list);
                }

                node* neg1 = negate_node(deep_copy(P_implies_Q));
                node* neg2 = negate_node(deep_copy(Q_implies_P));

                // Create new target tablines
                tabline_t new_tabline_P_implies_Q(neg1, P_implies_Q);
                tabline_t new_tabline_Q_implies_P(neg2, Q_implies_P);
                
                // Copy restrictions and assumptions
                new_tabline_P_implies_Q.assumptions = tabline.assumptions;
                new_tabline_P_implies_Q.restrictions = tabline.restrictions;
                new_tabline_Q_implies_P.assumptions = tabline.assumptions;
                new_tabline_Q_implies_P.restrictions = tabline.restrictions;
                
                // Set justifications
                new_tabline_P_implies_Q.justification = { Reason::MaterialEquivalence, { static_cast<int>(i) } };
                new_tabline_Q_implies_P.justification = { Reason::MaterialEquivalence, { static_cast<int>(i) } };

                // **Critical Fix: Set flags before hydra operations**
                // Already set flags before modifying the vector

                // Mark the original equivalence as inactive and dead
                tabline.active = false;
                tabline.dead = true;

                // Append new target tablines to the tableau
                tab_ctx.tableau.push_back(new_tabline_P_implies_Q);
                tab_ctx.tableau.push_back(new_tabline_Q_implies_P);
                
                // Split the hydra and select targets
                tab_ctx.hydra_split(i, tab_ctx.tableau.size() - 2, tab_ctx.tableau.size() - 1);
                tab_ctx.restrictions_split(i, tab_ctx.tableau.size() - 2, tab_ctx.tableau.size() - 1);
                tab_ctx.select_targets();

                moved = true;
            }
        }

        i++;
    }

    return moved;
}

bool conditional_premise(context_t& tab_ctx, int index) {
    // Check if index is within bounds
    if (index < 0 || index >= static_cast<int>(tab_ctx.tableau.size())) {
        std::cerr << "Error: Index out of bounds." << std::endl;
        return false;
    }

    tabline_t& tabline = tab_ctx.tableau[index];

    if (!tabline.target) {
        std::cerr << "Error: Selected formula is not a target." << std::endl;
        return false;
    }

    // Check if the negated field is an implication
    if (!tabline.negation->is_implication()) {
        std::cerr << "Error: The target is not an implication." << std::endl;
        return false;
    }

    // Extract P and Q from the implication
    node* implication = tabline.negation; // Since it's a negated formula
    node* P = implication->children[0];
    node* Q = implication->children[1];

    // Deep copy P and Q
    node* P_copy = deep_copy(P);
    node* Q_copy = deep_copy(Q);
    P_copy = disjunction_to_implication(P_copy);
    Q_copy = disjunction_to_implication(Q_copy);
    node* neg_Q_copy = negate_node(deep_copy(Q)); // For the new target Q

    // Create new hypothesis P
    tabline_t new_hypothesis(P_copy);
    new_hypothesis.target = false;
    new_hypothesis.active = true;
    new_hypothesis.justification = { Reason::ConditionalPremise, { static_cast<int>(index) } };
    // The new target Q will be at index size(), zero-based

    // Create new target Q
    tabline_t new_target(neg_Q_copy, Q_copy); // Assuming constructor takes formula and negation
    new_target.target = true;
    new_target.active = true;
    new_target.justification = { Reason::ConditionalPremise, { static_cast<int>(index) } };

    // Copy restrictions and assumptions
    new_hypothesis.assumptions = tabline.assumptions;
    new_hypothesis.restrictions = tabline.restrictions;
    new_target.assumptions = tabline.assumptions;
    new_target.restrictions = tabline.restrictions;
                
    // Add restriction to the new hypothesis
    new_hypothesis.restrictions.push_back(static_cast<int>(tab_ctx.tableau.size() + 1));
    
    // Append new hypothesis and target to the tableau
    tab_ctx.tableau.push_back(new_hypothesis);
    tab_ctx.tableau.push_back(new_target);
            
    // Replace hydra
    tab_ctx.hydra_replace(index, tab_ctx.tableau.size() - 1);
    tab_ctx.restrictions_replace(index, tab_ctx.tableau.size() - 1);
    tab_ctx.select_targets();

    // Deactivate the original formula
    tabline.active = false;

    return true;
}

bool move_cp(context_t& tab_ctx, size_t start) {
    bool moved = false;

    for (size_t i = start; i < tab_ctx.tableau.size(); ++i) {
        tabline_t& tabline = tab_ctx.tableau[i];

        if (tabline.active && tabline.target) {
            // Ensure the negation field exists and is an implication
            if (tabline.negation && tabline.negation->is_implication()) {
                // **Critical Fix: Set flags before modifying the vector**
                // Mark the original target as inactive and dead
                tabline.active = false;
                tabline.dead = true;

                // Apply conditional_premise
                bool success = conditional_premise(tab_ctx, i);
                if (success) {
                    moved = true;
                }
            }
        }
    }

    return moved;
}

bool move_sd(context_t& tab_ctx, size_t line) {
    tabline_t& tabline = tab_ctx.tableau[line];

    if (tabline.target) {
        std::cerr << "Error: formula is not a hypothesis." << std::endl;
        return false;
    }

    if (!tabline.formula->is_implication()) {
        std::cerr << "Error: formula is not a disjunction." << std::endl;
        return false;
    }

    // get disjunction node
    node* disjunction = tabline.formula;

    std::set<std::string> common_vars;
    common_vars = find_common_variables(disjunction->children[0], disjunction->children[1]);
    if (!common_vars.empty()) {
        std::cerr << "Error: disjunction has shared variables." << std::endl;
        return false;
    }
    
    // make copies of P and Q
    node* P_copy = negate_node(deep_copy(disjunction->children[0]));
    node* P_neg = deep_copy(disjunction->children[0]);
    node* Q_copy = deep_copy(disjunction->children[1]);

    // make new tablines
    tabline_t hyp1(P_copy);
    tabline_t hyp2a(P_neg);
    tabline_t hyp2b(Q_copy);

    // start with hyp1 active
    hyp1.active = true;
    hyp2a.active = false;
    hyp2b.active = false;

    // assign justifications
    hyp1.justification = { Reason::SplitDisjunction, { static_cast<int>(line) } };
    hyp2a.justification = { Reason::SplitDisjunction, { static_cast<int>(line) } };
    hyp2b.justification = { Reason::SplitDisjunction, { static_cast<int>(line) } };

    // assign assumptions
    hyp1.assumptions = tabline.assumptions;
    hyp2a.assumptions = tabline.assumptions;
    hyp2b.assumptions = tabline.assumptions;
    hyp1.assumptions.push_back(static_cast<int>(line) + 1);
    hyp2a.assumptions.push_back(-static_cast<int>(line) - 1);
    hyp2b.assumptions.push_back(-static_cast<int>(line) - 1);

    // assign restrictions
    hyp1.restrictions = tabline.restrictions;
    hyp2a.restrictions = tabline.restrictions;
    hyp2b.restrictions = tabline.restrictions;
    
    // Append new hypothesises to the tableau
    tab_ctx.tableau.push_back(hyp1);
    tab_ctx.tableau.push_back(hyp2a);
    tab_ctx.tableau.push_back(hyp2b);

    // Deactivate the original formula
    tabline.active = false;

    return true;
}

bool cleanup_moves(context_t& tab_ctx, size_t start_line) {
    bool moved = false, moved1;
    size_t start = start_line;
    size_t current_size = tab_ctx.tableau.size();
            
    while (start < current_size) {
        // Apply moves in the specified order

        // 1. skolemize_all
        moved1 = false;
        if (skolemize_all(tab_ctx, start)) {
            moved = moved1 = true;
        }

#if DEBUG_CLEANUP
        if (moved1) {
            std::cout << "skolemize:" << std::endl;
            print_tableau(tab_ctx);
            std::cout << std::endl;
        }
#endif

        // 2. move_me
        moved1 = false;
        if (move_me(tab_ctx, start)) {
            moved = moved1 = true;
        }

 #if DEBUG_CLEANUP
        if (moved1) {
            std::cout << "material equivalence:" << std::endl;
            print_tableau(tab_ctx);
            std::cout << std::endl;
        }
#endif
        // 3. move_cp
        moved1 = false;
        if (move_cp(tab_ctx, start)) {
            moved = moved1 = true;
        }

#if DEBUG_CLEANUP
        if (moved1) {
            std::cout << "conditional premise:" << std::endl;
            print_tableau(tab_ctx);
            std::cout << std::endl;
        }
#endif

        // 4. move_sc
        moved1 = false;
        if (move_sc(tab_ctx, start)) {
            moved = moved1 = true;
        }

#if DEBUG_CLEANUP
        if (moved1) {
            std::cout << "split conjunctions:" << std::endl;
            print_tableau(tab_ctx);
            std::cout << std::endl;
        }
#endif

        // 5. move_ni
        moved1 = false;
        if (move_ni(tab_ctx, start)) {
            moved = moved1 = true;
        }

#if DEBUG_CLEANUP
        if (moved1) {
            std::cout << "negated implication:" << std::endl;
            print_tableau(tab_ctx);
            std::cout << std::endl;
        }
#endif

        // 6. move_sdi
        moved1 = false;
        if (move_sdi(tab_ctx, start)) {
            moved = moved1 = true;
        }

#if DEBUG_CLEANUP
        if (moved1) {
            std::cout << "split disjunctive implication:" << std::endl;
            print_tableau(tab_ctx);
            std::cout << std::endl;
        }
#endif

        // 7. move_sci
        moved1 = false;
        if (move_sci(tab_ctx, start)) {
            moved = moved1 = true;
        }

#if DEBUG_CLEANUP
        if (moved1) {
            std::cout << "split conjunctive implication:" << std::endl;
            print_tableau(tab_ctx);
            std::cout << std::endl;
        }
#endif

        // 8. move_di
        moved1 = false;
        if (move_di(tab_ctx, start)) {
            moved = moved1 = true;
        }

#if DEBUG_CLEANUP
        if (moved1) {
            std::cout << "disjunctive idempotence:" << std::endl;
            print_tableau(tab_ctx);
            std::cout << std::endl;
        }
#endif

        // 9. move_ci
        moved1 = false;
        if (move_ci(tab_ctx, start)) {
            moved = moved1 = true;
        }

#if DEBUG_CLEANUP
        if (moved1) {
            std::cout << "conjunctive idempotence:" << std::endl;
            print_tableau(tab_ctx);
            std::cout << std::endl;
        }
#endif

        // Update start_line to previous current_size before moves were applied
        start = current_size;
        // Update current_size to the new size of the tableau
        current_size = tab_ctx.tableau.size();
    }
    
    return moved;
}

bool cleanup_definition(context_t& tab_ctx, size_t start_line) {
    bool moved = false, moved1;
    size_t start = start_line;
    size_t current_size = tab_ctx.tableau.size();
            
    while (start < current_size) {
        // Apply moves in the specified order

        // 1. skolemize_all
        moved1 = false;
        if (skolemize_all(tab_ctx, start)) {
            moved = moved1 = true;
        }

#if DEBUG_CLEANUP
        if (moved1) {
            std::cout << "skolemize:" << std::endl;
            print_tableau(tab_ctx);
            std::cout << std::endl;
        }
#endif

        // 2. move_me
        moved1 = false;
        if (move_me(tab_ctx, start)) {
            moved = moved1 = true;
        }

 #if DEBUG_CLEANUP
        if (moved1) {
            std::cout << "material equivalence:" << std::endl;
            print_tableau(tab_ctx);
            std::cout << std::endl;
        }
#endif

        // Update start_line to previous current_size before moves were applied
        start = current_size;
        // Update current_size to the new size of the tableau
        current_size = tab_ctx.tableau.size();
    }
    
    return moved;
}

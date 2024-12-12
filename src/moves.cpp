// moves.cpp

#include "moves.h"
#include <set>
#include <algorithm>
#include <vector>
#include <stack>

// Define DEBUG_CLEANUP to enable debug traces for cleanup moves
#define DEBUG_CLEANUP 1

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

    // Return the modified formula \phi(skolem_func)
    return phi;
}

// skolemize an arbitrary formula
// original formula is destroyed
node* skolem_form(context_t& ctx, node* formula) {
    Substitution subst; // Initialize empty substitution map
    std::vector<std::string> universals; // List of universally quantified variables
    node* special_implications = nullptr; // implications coming from special quantifiers
    node* inner_node = nullptr; // current inside implication
    node* inner_formula; // inner formula \phi
            
    if (formula->type == QUANTIFIER) {
        // Increment count of cleanup moves
        ctx.cleanup++;
    }

    // Process quantifiers until the outermost node is no longer a quantifier
    while (formula->type == QUANTIFIER) {
        node* var_node = formula->children[0];
        if (formula->symbol == SYMBOL_FORALL) {
            // Extract the universal variable
            std::string u = var_node->name();
            universals.push_back(u);
        } else if (formula->symbol == SYMBOL_EXISTS) {
            // Skolemize the existential quantifier with current universals
            skolemize(ctx, formula, universals, subst);
        } else {
            // Unsupported quantifier symbol; handle as needed
            break;
        }
        
        // Peel off any special implications and
        // set formula to the inner formula \phi
        if (formula->is_special_binder()) {
            node* implication = formula->children[1];
            
            // add implication to special_implications
            if (special_implications == nullptr) {
                special_implications = implication;
            } else {
                inner_node->children.pop_back();
                inner_node->children.push_back(implication);
            }
            inner_node = implication;
            
            inner_formula = implication->children[1];
        } else {
            inner_formula = formula->children[1];
        }

        // Unbind all occurrences of variable
        unbind_var(formula, var_node->name());

        formula->children.clear(); // Detach children to prevent deletion
        delete var_node; // Delete no longer used variable
        delete formula; // Delete the forall node

        formula = inner_formula; // Update formula to inner formula
    }

    // Reattach all the special implications
    if (special_implications != nullptr) {
        inner_node->children.pop_back();
        inner_node->children.push_back(formula);
        formula = special_implications;
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
        if (tabline.active && !tabline.is_theorem() && !tabline.is_definition()) {
            bool quantified = unwrap_special(tabline.formula)->type == QUANTIFIER;
            
            // Apply skolem_form to the formula
            node* skolemized = skolem_form(tab_ctx, tabline.formula);
            
            if (quantified) { // if anything changed
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

node* modus_ponens(Substitution& combined_subst, context_t& ctx_var, node* implication, const std::vector<node*>& unit_clauses, bool silent) {
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
    vars_used(vars_conjuncts, implication, false);

    std::set<std::string> vars_units;
    for (const auto& unit : unit_clauses) {
        vars_used(vars_units, unit, true);
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
        if (!silent) {
            std::cerr << "Error: Number of unit clauses (" << unit_clauses.size()
                  << ") does not match number of antecedent conjuncts (" << conjuncts.size() << ")." << std::endl;
        }
        cleanup_conjuncts(conjuncts);
        delete implication_copy;
        return nullptr;
    }

    // 8. Collect all substitutions
    for (size_t i = 0; i < conjuncts.size(); ++i) {
        node* conjunct = conjuncts[i];
        node* unit = unit_clauses[i];

        // e. Unify the renamed conjunct with the unit clause
        Substitution subst;
        std::optional<Substitution> maybe_subst = unify(conjunct, unit, subst);
        if (!maybe_subst.has_value()) {
            if (!silent) {
                std::cerr << "Error: Unification failed between conjunct " << (i + 1) << " and unit clause." << std::endl;
                std::cerr << "Conjunct: " << conjunct->to_string(UNICODE) 
                          << " | Unit Clause: " << unit->to_string(UNICODE) << std::endl;
            }
            cleanup_conjuncts(conjuncts);
            delete implication_copy;
            return nullptr;
        }

        // f. Combine substitutions, ensuring no conflicts
        for (const auto& [key, value] : maybe_subst.value()) {
            // Check if the variable already has a substitution
            auto it = combined_subst.find(key);
            if (it != combined_subst.end()) {
                if (it->second->to_string(REPR) != value->to_string(REPR)) {
                    if (!silent) {
                        std::cerr << "Error: Conflicting substitutions for variable '" << key << "'." << std::endl;
                    }
                    cleanup_conjuncts(conjuncts);
                    delete implication_copy;
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
        if (!silent) {
            std::cerr << "Error: Substitution failed on the consequent." << std::endl;
        }
        // Clean up and exit
        delete implication_copy;
        return nullptr;
    }
    
    // 10. Clean up combined_subst and implication_copy
    delete implication_copy;

    return substituted_consequent;
}

node* modus_tollens(Substitution& combined_subst, context_t& ctx_var, node* implication, const std::vector<node*>& unit_clauses, bool silent) {
    // 1. Negate the implication: A -> B becomes ¬B -> ¬A
    node* negated_implication = contrapositive(implication);

    // 2. Apply modus ponens with the negated implication and the provided unit clauses
    node* result = modus_ponens(combined_subst, ctx_var, negated_implication, unit_clauses, silent);

    // 3. Clean up the negated implication
    delete negated_implication;

    // 4. Return the result from modus ponens
    return result;
}

bool move_mpt(context_t& ctx, int implication_line, const std::vector<int>& other_lines, const std::vector<size_t>& special_lines, bool ponens, bool silent) {
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

    // list of special predicates peeled off implication and units
    std::vector<node*> special_predicates;

    node* implication = split_special(special_predicates, implication_tabline.formula);
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

        if (!assumptions_compatible(implication_tabline.assumptions, current_tabline.assumptions)) {
            if (!silent) {
                std::cerr << "Error: line " << line + 1 << " has incompatible assumptions.\n";
            }
            return false;
        }

        if (!restrictions_compatible(implication_tabline.restrictions, current_tabline.restrictions)) {
            if (!silent) {
                std::cerr << "Error: line " << line + 1 << " has incompatible target restrictions.\n";
            }
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
        node* clause = split_special(special_predicates, ctx.tableau[line].formula);
        unit_clauses.push_back(clause);
    }

    // Step 6: Apply the appropriate inference rule
    node* result = nullptr;
    Reason justification_reason;
    Substitution subst;

    if (forward ^ !ponens) {
        // Apply modus ponens
        result = modus_ponens(subst, ctx, implication, unit_clauses, silent);
    }
    else {
        // Apply modus tollens
        result = modus_tollens(subst, ctx, implication, unit_clauses, silent);
    }

    justification_reason = (ponens ? Reason::ModusPonens : Reason::ModusTollens);

    // Step 7: Check if the inference was successful
    if (result == nullptr) {
        if (!silent) {
                std::cerr << "Error: modus " << (ponens ? "ponens" : "tollens") << " failed to infer a result.\n";
        }
        special_predicates.clear();
        cleanup_subst(subst);
        return false;
    }

    // Step 8: Apply substitutions to special predicates
    for (size_t i = 0; i < special_predicates.size(); ++i) {
        special_predicates[i] = substitute(deep_copy(special_predicates[i]), subst);
    }
    
    // Step 9: Check special predicates against supplied list
    bool special_found = true;
    for (node* special : special_predicates) {
        special_found = false;
        
        for (int special_line : special_lines) {
            Substitution special_subst;
            tabline_t& special_tabline = ctx.tableau[special_line];
            
            // Try to unify with tableau special predicate
            std::optional<Substitution> maybe_subst = unify(special_tabline.formula, special, special_subst, false);

            // If unification succeeded we found the special predicate in the tableau
            if (maybe_subst.has_value()) {
                special_found = true;
                break;
            }
        }

        if (!special_found) {
            break;
        }
    }

    if (!special_found) {
        if (!silent) {
            std::cerr << "Error: predicated structure constraints are not satisfied in modus " << (ponens ? "ponens" : "tollens") << ".\n";
        }
        
        // Clean up
        for (node* special : special_predicates) {
            delete special;
        }
        special_predicates.clear();
        cleanup_subst(subst);

        return false;
    }
    
    // Step 10: Wrap the result with the new special implications
    std::set<std::string> vars;
    vars_used(vars, result, false, false); // Get all unbound variables used
    std::set<std::string> special_strings; // Strings of special predicates we already included

    while (!special_predicates.empty()) {
        node* special = special_predicates.back();
        special_predicates.pop_back();

        // Delete any special predicates that are not applied to variables
        if (!special->children[1]->is_variable()) {
            delete special;
            continue;
        }

        // Check this variable is actually used
        if (vars.find(special->children[1]->vdata->name) == vars.end()) {
            delete special;
            continue;
        }

        std::string special_str = special->to_string(UNICODE);

        // Check we don't already have this special predicate
        if (special_strings.find(special_str) != vars.end()) {
            delete special;
            continue;
        } else {
            // We don't have this special predicate
            special_strings.insert(special_str); // insert the string for this one

            // Add special implication to formula
            std::vector<node*> children;
            children.push_back(special);
            children.push_back(result);
            result = new node(LOGICAL_BINARY, SYMBOL_IMPLIES, children);
        }
    }

    // Step 11: Create a new tabline for the result
    tabline_t new_tabline(result);

    if (forward) {
        // Insert as a hypothesis
        new_tabline.target = false;
        new_tabline.formula = disjunction_to_implication(new_tabline.formula);
    } else {
        // Insert as a target
        new_tabline.target = true;

        // Negate the result and store in the negation field
        node* neg_result = negate_node(deep_copy(result), true);
        new_tabline.negation = neg_result;
    }

    // Step 12: Set the justification
    std::vector<int> justification_lines;
    justification_lines.push_back(implication_line);
    justification_lines.insert(justification_lines.end(), other_lines.begin(), other_lines.end());
    new_tabline.justification = { justification_reason, justification_lines };

    // Step 13: Set the assumptions and restrictions
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

    if (forward) {
        implication_tabline.split = true; // Ensure this line is not split, as it has been used
    }

    // Step 14: Append the new tabline to the tableau
    ctx.tableau.push_back(new_tabline);

    if (!forward) {
        ctx.hydra_replace_list(other_lines, ctx.tableau.size() - 1);
        ctx.restrictions_replace_list(other_lines, ctx.tableau.size() - 1);
        ctx.select_targets();
    }

    // Increment count of reasoning moves
    ctx.reasoning++;

    // Clean up
    cleanup_subst(subst);

    return true;
}

// Recursive function to traverse the formula tree, find a subformula that unifies with P,
// apply the substitution, replace it with Q_prime, and merge the substitution into combined_subst.
// Returns true if a replacement was made; otherwise, false.
bool rewrite(Substitution& combined_subst, node*& current, node* P, node* Q) {
    // Local substitution for the current unification attempt
    Substitution local_subst;

    // Attempt to unify P with the current node
    if (unify(P, current, local_subst)) {
        // Apply substitution to Q to get Q_prime
        node* Q_prime = substitute(Q, local_subst);

        // Replace the current node with Q_prime
        delete current;       // Free the memory of the current node
        current = Q_prime;    // Assign Q_prime to the current node pointer

        // Merge local_subst into combined_subst
        for (const auto& [key, value] : local_subst) {
            combined_subst[key] = value;
        }

        return true;          // Indicate that a replacement has been made
    }

    // Recursively traverse the children
    for (size_t i = 0; i < current->children.size(); ++i) {
        if (rewrite(combined_subst, current->children[i], P, Q)) {
            return true; // Replacement done; no need to continue
        }
    }

    return false; // No replacement made in this subtree
}

// Function to apply a rewrite move in the tableau.
// It rewrites a formula (hypothesis or target) using a rewrite rule of the form P = Q.
bool move_rewrite(context_t& ctx, int formula_line, int rewrite_line, bool silent) {
    // Step 1: Validate line indices.
    if (formula_line < 0 || formula_line >= static_cast<int>(ctx.tableau.size())) {
        std::cerr << "Error: formula_line " << (formula_line + 1) << " is out of bounds.\n";
        return false;
    }

    if (rewrite_line < 0 || rewrite_line >= static_cast<int>(ctx.tableau.size())) {
        std::cerr << "Error: rewrite_line " << (rewrite_line + 1) << " is out of bounds.\n";
        return false;
    }

    tabline_t& formula_tabline = ctx.tableau[formula_line];
    tabline_t& rewrite_tabline = ctx.tableau[rewrite_line];

    // Step 2: Ensure the formula line is active.
    if (!formula_tabline.active) {
        std::cerr << "Error: formula_line " << (formula_line + 1) << " is not active.\n";
        return false;
    }

    // Step 3: Ensure the rewrite line is a hypothesis.
    if (rewrite_tabline.target) {
        std::cerr << "Error: rewrite_line " << (rewrite_line + 1) << " is not a hypothesis.\n";
        return false;
    }

    // Step 4: Ensure the rewrite formula is an equality P = Q.
    node* rewrite_formula = rewrite_tabline.formula;
    if (!rewrite_formula->is_equality()) { // Assuming is_equality() checks for P = Q
        std::cerr << "Error: rewrite_line " << (rewrite_line + 1) << " does not contain an equality formula P = Q.\n";
        return false;
    }

    node* P = rewrite_formula->children[1];
    node* Q = rewrite_formula->children[2];

    // Step 5: Check if assumptions and restrictions are compatible.
    if (!assumptions_compatible(formula_tabline.assumptions, rewrite_tabline.assumptions)) {
        if (!silent) {
            std::cerr << "Error: formula_line and rewrite_line have incompatible assumptions.\n";
        }
        return false;
    }

    if (!restrictions_compatible(formula_tabline.restrictions, rewrite_tabline.restrictions)) {
        if (!silent) {
            std::cerr << "Error: formula_line and rewrite_line have incompatible restrictions.\n";
        }
        return false;
    }

    // Step 6: Create a deep copy of the formula to be rewritten.
    node* formula_copy = deep_copy(formula_tabline.formula);
    if (!formula_copy) {
        std::cerr << "Error: Failed to copy the formula from formula_line " << (formula_line + 1) << ".\n";
        return false;
    }

    // Step 7: Determine shared variables between formula_copy and rewrite_formula.
    std::set<std::string> vars_formula, vars_rewrite;
    vars_used(vars_formula, formula_copy);
    vars_used(vars_rewrite, rewrite_formula);

    std::set<std::string> common_vars;
    std::set_intersection(vars_formula.begin(), vars_formula.end(),
                          vars_rewrite.begin(), vars_rewrite.end(),
                          std::inserter(common_vars, common_vars.begin()));

    // Step 8: Rename shared variables in formula_copy to avoid conflicts.
    if (!common_vars.empty()) {
        std::vector<std::pair<std::string, std::string>> rename_list = vars_rename_list(ctx, common_vars);
        rename_vars(formula_copy, rename_list);
    }

    // Steps 9-11: Optimized Traversal, Unification, Substitution, and Replacement

    // Initialize a combined substitution
    Substitution combined_subst;

    // Traverse formula_copy, find a subformula that unifies with P, apply substitution, and replace it with Q'.
    bool replaced = rewrite(combined_subst, formula_copy, P, Q);

    if (!replaced) {
        if (!silent) {
            std::cerr << "Error: No subformula in formula_line " << (formula_line + 1)
                      << " unifies with the left side of the rewrite rule.\n";
        }
        delete formula_copy;
        return false;
    }

    // Step 12: Create a new tabline with the rewritten formula.
    tabline_t new_tabline(formula_copy);

    if (formula_tabline.target) {
        // The formula is a target.
        // Negate the rewritten formula and apply disjunction to implication.
        node* negated_formula = negate_node(deep_copy(formula_copy));
        negated_formula = disjunction_to_implication(negated_formula);

        // Place negation in target
        new_tabline.negation = negated_formula;
    }

    // Step 13: Update assumptions and restrictions.
    new_tabline.assumptions = combine_assumptions(formula_tabline.assumptions, rewrite_tabline.assumptions);
    new_tabline.restrictions = combine_restrictions(formula_tabline.restrictions, rewrite_tabline.restrictions);
    new_tabline.justification = { Reason::EqualitySubst, { formula_line, rewrite_line } };

    // Step 14: Add the new tabline to the tableau.
    ctx.tableau.push_back(new_tabline);

    // Step 15: Increment rewrite move count.
    ctx.rewrite++;

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

        if (!tabline.active || tabline.is_theorem() || tabline.is_definition()) {
            i++;
            continue;
        }

        std::vector<node*> special_predicates;
        node* formula = split_special(special_predicates, tabline.formula);
    
        // Check if the formula is a disjunctive idempotent
        if ((tabline.target && conjunctive_idempotence(formula))
            || (!tabline.target && disjunctive_idempotence(formula))
            || (!tabline.target && implicative_idempotence(formula))) {
            // Formula is of the form P ∨ P or P ∧ P

            // Increment count of cleanup moves
            tab_ctx.cleanup++;
        
            // **Critical Fix: Set flags before modifying the vector**
            // Mark the original conjunction/disjunction as inactive and dead
            tabline.active = false;
            tabline.dead = true;

            // Store the original formula node and its children
            node* original_formula = formula;
            node* P = original_formula->children[1];

            if (!tabline.target) {
                P = reapply_special(special_predicates, deep_copy(P));

                // Original is a hypothesis, new tablines are hypotheses
                tabline_t new_tabline_P(P);
                
                // Copy restrictions and assumptions
                new_tabline_P.assumptions = tabline.assumptions;
                new_tabline_P.restrictions = tabline.restrictions;
                
                // Set justification to DisjunctiveIdempotence or ConjunctiveIdempotence
                Reason justification = formula->is_disjunction() ? Reason::DisjunctiveIdempotence : Reason::ConjunctiveIdempotence;
                new_tabline_P.justification = { justification, { static_cast<int>(i) } };
                
                // Append new hypotheses to the tableau
                tab_ctx.tableau.push_back(new_tabline_P);
            }
            else {
                // Original is a target, new tablines are targets
                node* neg_P = negate_node(deep_copy(P), true);
                neg_P = reapply_special(special_predicates, neg_P);
                node* new_P = reapply_special(special_predicates, deep_copy(P));
                
                tabline_t new_tabline_P(new_P, neg_P);

                // Copy restrictions and assumptions
                new_tabline_P.assumptions = tabline.assumptions;
                new_tabline_P.restrictions = tabline.restrictions;
                
                // Set justification to DisjunctiveIdempotence or ConjunctiveIdempotence
                Reason justification = formula->is_disjunction() ? Reason::DisjunctiveIdempotence : Reason::ConjunctiveIdempotence;
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

        special_predicates.clear();

        i++;
    }

    return moved;
}

bool move_ci(context_t& tab_ctx, size_t start) {
    bool moved = false;
    size_t i = start;

    while (i < tab_ctx.tableau.size()) {
        tabline_t& tabline = tab_ctx.tableau[i];

        if (!tabline.active || tabline.is_theorem() || tabline.is_definition()) {
            i++;
            continue;
        }

        std::vector<node*> special_predicates;
        node* formula = split_special(special_predicates, tabline.formula);
    
        // Check if the formula is a conjunctive idempotent
        if ((tabline.target && disjunctive_idempotence(formula))
            || (!tabline.target && conjunctive_idempotence(formula))) {
            // Formula is of the form P ∧ P or P ∨ P

            // Increment count of cleanup moves
            tab_ctx.cleanup++;
            
            // **Critical Fix: Set flags before modifying the vector**
            // Mark the original conjunction/disjunction as inactive and dead
            tabline.active = false;
            tabline.dead = true;

            // Store the original formula node and its children
            node* original_formula = formula;
            node* P = original_formula->children[0];

            if (!tabline.target) {
                P = reapply_special(special_predicates, deep_copy(P));

                // Original is a hypothesis, new tablines are hypotheses
                tabline_t new_tabline_P(P);
                
                // Copy restrictions and assumptions
                new_tabline_P.assumptions = tabline.assumptions;
                new_tabline_P.restrictions = tabline.restrictions;
                
                // Set justification to ConjunctiveIdempotence or DisjunctiveIdempotence
                Reason justification = formula->is_conjunction() ? Reason::ConjunctiveIdempotence : Reason::DisjunctiveIdempotence;
                new_tabline_P.justification = { justification, { static_cast<int>(i) } };
                
                // Append new hypotheses to the tableau
                tab_ctx.tableau.push_back(new_tabline_P);
            }
            else {
                // Original is a target, new tablines are targets
                node* new_P = reapply_special(special_predicates, deep_copy(P));
                node* neg_P = negate_node(deep_copy(P), true);
                neg_P = reapply_special(special_predicates, neg_P);

                tabline_t new_tabline_P(new_P, neg_P);

                // Copy restrictions and assumptions
                new_tabline_P.assumptions = tabline.assumptions;
                new_tabline_P.restrictions = tabline.restrictions;
                
                // Set justification to ConjunctiveIdempotence or DisjunctiveIdempotence
                Reason justification = formula->is_conjunction() ? Reason::ConjunctiveIdempotence : Reason::DisjunctiveIdempotence;
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

        special_predicates.clear();

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

        // Skip theorems and definitions
        if (tabline.is_theorem() || tabline.is_definition()) {
            i++;
            continue;
        }

        std::vector<node*> special_predicates;
        node* formula = split_special(special_predicates, tabline.formula);

        // Check if the formula is active and a conjunction or disjunction
        if (tabline.active && ((!tabline.target && formula->is_conjunction()) ||
            (tabline.target && formula->is_disjunction()))) {
            
            // Increment count of cleanup moves
            tab_ctx.cleanup++;
        
            // Mark the original conjunction as inactive and dead BEFORE modifying the vector
            tabline.active = false;
            tabline.dead = true;

            // Proceed with splitting the conjunction/disjunction
            node* P = formula->children[0];
            node* Q = formula->children[1];

            if (!tabline.target) {
                // Original is a hypothesis, new tablines are hypotheses
                P = reapply_special(special_predicates, deep_copy(P));
                Q = reapply_special(special_predicates, deep_copy(Q));

                tabline_t new_tabline_P(P);
                tabline_t new_tabline_Q(Q);

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

                neg_P = reapply_special(special_predicates, neg_P);
                neg_Q = reapply_special(special_predicates, neg_Q);

                node* new_P = reapply_special(special_predicates, deep_copy(P));
                node* new_Q = reapply_special(special_predicates, deep_copy(Q));

                tabline_t new_tabline_P(new_P, neg_P);
                tabline_t new_tabline_Q(new_Q, neg_Q);

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

        special_predicates.clear();

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
        if (!tabline.active || tabline.is_theorem() || tabline.is_definition()) {
            i++;
            continue;
        }

        std::vector<node*> special_predicates;
        node* formula = split_special(special_predicates, tabline.formula);
    
        if (!tabline.target) {
            // **Hypothesis Case:** Look for formulas of the form (P ∨ Q) → R
            if (formula->is_implication()) {
                node* left = formula->children[0];  // (P ∨ Q)
                node* right = formula->children[1]; // R

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
                        // Increment count of cleanup moves
                        tab_ctx.cleanup++;

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
                            
                            P_imp_R = reapply_special(special_predicates, P_imp_R);

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

                            Q_imp_R = reapply_special(special_predicates, Q_imp_R);
                            
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
            if (formula->is_conjunction()) {
                node* left = formula->children[0];  // (P ∨ Q)
                node* right = formula->children[1]; // ¬R

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
                        // Increment count of cleanup moves
                        tab_ctx.cleanup++;

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
                            node* neg_P_imp_R = negate_node(deep_copy(P_imp_R));
                            P_imp_R = reapply_special(special_predicates, P_imp_R);
                            neg_P_imp_R = reapply_special(special_predicates, neg_P_imp_R);
                            
                            // Create new tablines as target
                            tabline_t new_tabline_neg_P_imp(neg_P_imp_R, P_imp_R);
                            
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
                            node* neg_Q_imp_R = negate_node(deep_copy(Q_imp_R));
                            Q_imp_R = reapply_special(special_predicates, Q_imp_R);
                            neg_Q_imp_R = reapply_special(special_predicates, neg_Q_imp_R);
                            
                            // Create new tablines as target
                            tabline_t new_tabline_neg_Q_imp(neg_Q_imp_R, Q_imp_R);

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

        special_predicates.clear();

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
        if (!tabline.active || tabline.is_theorem() || tabline.is_definition()) {
            i++;
            continue;
        }

        std::vector<node*> special_predicates;
        node* formula = split_special(special_predicates, tabline.formula);
    
        if (!tabline.target) {
            // **Hypothesis Case:** Look for formulas of the form P → (Q ∧ R)
            if (formula->is_implication()) {
                node* antecedent = formula->children[0];  // P
                node* consequent = formula->children[1];  // (Q ∧ R)

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
                        // Increment count of cleanup moves
                        tab_ctx.cleanup++;

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
                            P_imp_Q = reapply_special(special_predicates, P_imp_Q);
                            
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
                            P_imp_R = reapply_special(special_predicates, P_imp_R);
                            
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
            if (formula->is_conjunction()) {
                node* P = formula->children[0];             // P
                node* disjunct = formula->children[1];      // (Q ∨ R)

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
                        // Increment count of cleanup moves
                        tab_ctx.cleanup++;

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
                            node* neg_P_and_Q = negate_node(deep_copy(P_and_Q), true);
                            P_and_Q = reapply_special(special_predicates, P_and_Q);
                            neg_P_and_Q = reapply_special(special_predicates, neg_P_and_Q);
                            
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
                            node* neg_P_and_R = negate_node(deep_copy(P_and_R), true);
                            P_and_R = reapply_special(special_predicates, P_and_R);
                            neg_P_and_R = reapply_special(special_predicates, neg_P_and_R);

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

        special_predicates.clear();

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
        if (!tabline.active || tabline.is_theorem() || tabline.is_definition()) {
            i++;
            continue;
        }

        std::vector<node*> special_predicates;
        node* formula = split_special(special_predicates, tabline.formula);
    
        if (!tabline.target) {
            // **Hypothesis Case:** Look for formulas of the form ¬(P → Q)
            if (formula->is_negation()) {
                node* inner = formula->children[0];
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
                        // Increment count of cleanup moves
                        tab_ctx.cleanup++;

                        // **Critical Fix: Set flags before modifying the vector**
                        // Mark the original negated implication as inactive and dead
                        tabline.active = false;
                        tabline.dead = true;

                        // Deep copy P, Q
                        node* P_copy = deep_copy(P);
                        node* Q_copy = deep_copy(Q);
                        node* neg_Q_copy = negate_node(deep_copy(Q));
                        P_copy = reapply_special(special_predicates, P_copy);
                        Q_copy = reapply_special(special_predicates, Q_copy);
                        neg_Q_copy = reapply_special(special_predicates, neg_Q_copy);
                            
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
            if (formula->is_implication()) {
                // Increment count of cleanup moves
                tab_ctx.cleanup++;
        
                node* P = formula->children[0];
                node* Q = formula->children[1];

                // Deep copy P and Q
                node* P_copy = deep_copy(P);
                node* Q_copy = deep_copy(Q);
                P_copy = disjunction_to_implication(P_copy);
                node* neg_P_copy = negate_node(deep_copy(P));
                node* neg_Q_copy = negate_node(deep_copy(Q), true);
                P_copy = reapply_special(special_predicates, P_copy);
                Q_copy = reapply_special(special_predicates, Q_copy);
                neg_P_copy = reapply_special(special_predicates, neg_P_copy);
                neg_Q_copy = reapply_special(special_predicates, neg_Q_copy);
                        
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

        special_predicates.clear();

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
        if (!tabline.active || tabline.is_theorem() || tabline.is_definition()) {
            i++;
            continue;
        }

        if (!tabline.target) {
            std::vector<node*> special_predicates;
            node* formula = split_special(special_predicates, tabline.formula);
    
            // **Target Case:** Look for formulas of the form P ↔ Q
            if (formula->is_equivalence()) {
                // Increment count of cleanup moves
                tab_ctx.cleanup++;

                node* P = formula->children[0];
                node* Q = formula->children[1];

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
                P_implies_Q = reapply_special(special_predicates, P_implies_Q);
                Q_implies_P = reapply_special(special_predicates, Q_implies_P);
                
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

            special_predicates.clear();
        }
        else {
            std::vector<node*> special_predicates;
            node* negation = split_special(special_predicates, tabline.negation);
            
            // **Target Case:** Look for formulas of the form P ↔ Q
            if (negation->is_equivalence()) {
                // Increment count of cleanup moves
                tab_ctx.cleanup++;
        
                node* P = negation->children[0];
                node* Q = negation->children[1];

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
                P_implies_Q = reapply_special(special_predicates, P_implies_Q);
                Q_implies_P = reapply_special(special_predicates, Q_implies_P);
                neg1 = reapply_special(special_predicates, neg1);
                neg2 = reapply_special(special_predicates, neg2);
                
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

            special_predicates.clear();
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

    std::vector<node*> special_predicates;
    node* negation = split_special(special_predicates, tabline.negation);
    
    // Check if the negated field is an implication
    if (!negation->is_implication()) {
        std::cerr << "Error: The target is not an implication." << std::endl;
        
        special_predicates.clear();
        
        return false;
    }

    // Extract P and Q from the implication
    node* implication = negation; // Since it's a negated formula
    node* P = implication->children[0];
    node* Q = implication->children[1];

    // Get shared variables
    std::set<std::string> shared_vars = find_common_variables(P, Q);
    bool shared = !shared_vars.empty();

    // Deep copy P and Q
    node* P_copy = deep_copy(P);
    node* Q_copy = deep_copy(Q);
    P_copy = disjunction_to_implication(P_copy);
    Q_copy = disjunction_to_implication(Q_copy);
    node* neg_Q_copy = negate_node(deep_copy(Q)); // For the new target Q
    P_copy = reapply_special(special_predicates, P_copy);
    Q_copy = reapply_special(special_predicates, Q_copy);
    neg_Q_copy = reapply_special(special_predicates, neg_Q_copy);
                
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

    // Deactivate the original formula (must be done before invalidating tableau with push_backs)
    tabline.active = false;

    // Append new hypothesis and target to the tableau
    tab_ctx.tableau.push_back(new_hypothesis);
    tab_ctx.tableau.push_back(new_target);
            
    // Replace hydra
    tab_ctx.hydra_replace(index, tab_ctx.tableau.size() - 1, shared);
    tab_ctx.restrictions_replace(index, tab_ctx.tableau.size() - 1);
    tab_ctx.select_targets();

    special_predicates.clear();

    return true;
}

bool move_cp(context_t& tab_ctx, size_t start) {
    bool moved = false;

    for (size_t i = start; i < tab_ctx.tableau.size(); ++i) {
        tabline_t& tabline = tab_ctx.tableau[i];

        if (tabline.active && tabline.target) {
            // Ensure the negation field exists and is an implication
            if (tabline.negation && unwrap_special(tabline.negation)->is_implication()) {
                // Increment count of cleanup moves
                tab_ctx.cleanup++;

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

    std::vector<node*> special_predicates;
    node* formula = split_special(special_predicates, tabline.formula);
        
    if (!formula->is_implication()) {
        std::cerr << "Error: formula is not a disjunction." << std::endl;
        
        special_predicates.clear();

        return false;
    }

    // get disjunction node
    node* disjunction = formula;

    std::set<std::string> common_vars;
    common_vars = find_common_variables(disjunction->children[0], disjunction->children[1]);
    if (!common_vars.empty()) {
        std::cerr << "Error: disjunction has shared variables." << std::endl;
        
        special_predicates.clear();

        return false;
    }
    
    // Increment count of cleanup moves
    tab_ctx.split++;

    // make copies of P and Q
    node* P_copy = negate_node(deep_copy(disjunction->children[0]));
    node* P_neg = deep_copy(disjunction->children[0]);
    node* Q_copy = deep_copy(disjunction->children[1]);
    P_copy = reapply_special(special_predicates, P_copy);
    P_neg = reapply_special(special_predicates, P_neg);
    Q_copy = reapply_special(special_predicates, Q_copy);
    
    // make new tablines
    tabline_t hyp1(P_copy);
    tabline_t hyp2a(P_neg);
    tabline_t hyp2b(Q_copy);

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
    
    // Deactivate the original formula
    tabline.active = false;
    tabline.split = true;

    // Append new hypothesises to the tableau
    tab_ctx.tableau.push_back(hyp1);
    tab_ctx.tableau.push_back(hyp2a);
    tab_ctx.tableau.push_back(hyp2b);

    special_predicates.clear();

    return true;
}

bool cleanup_moves(context_t& tab_ctx, size_t start_line) {
    bool moved = false, moved1;
    size_t start = start_line;
    size_t current_size = tab_ctx.tableau.size();

    tab_ctx.kill_duplicates(start);
    tab_ctx.get_ltor();

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

        tab_ctx.kill_duplicates(start);
        tab_ctx.get_ltor();
    
        // Update start_line to previous current_size before moves were applied
        start = current_size;
        // Update current_size to the new size of the tableau
        current_size = tab_ctx.tableau.size();
    }
    
    // Updates constants fields of all lines, starting at upto
    tab_ctx.get_constants();

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

bool cleanup_rewrite(context_t& tab_ctx, size_t start_line) {
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

        // Update start_line to previous current_size before moves were applied
        start = current_size;
        // Update current_size to the new size of the tableau
        current_size = tab_ctx.tableau.size();
    }
    
    return moved;
}

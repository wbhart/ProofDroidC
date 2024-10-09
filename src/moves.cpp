// moves.cpp

#include "moves.h"
#include <set>
#include <algorithm>
#include <vector>

// turn disjunction into implication
node* hide_disjunction(node* formula) {
   if (formula->is_disjunction()) {
       node* antecedent = formula->children[0];
       node* negated = negate_node(antecedent);

       std::vector<node*> children;
       children.push_back(negated);
       children.push_back(formula->children[1]);

       node* impl = new node(LOGICAL_BINARY, SYMBOL_IMPLIES, children);

       formula->children.clear();

       return impl;
   } else
       return formula;
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
    if (ctx.has_variable(skolem_func_base)) {
        // Base name exists; append a subscript for uniqueness
        int skolem_index = ctx.get_next_index(skolem_func_base);
        skolem_func_name = append_subscript(skolem_func_base, skolem_index);
    } else {
        // Base name does not exist; use the original name without appending
        skolem_func_name = skolem_func_base;
        ctx.reset_index(skolem_func_base); // Initialize the index in the context
    }

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

    // Apply all accumulated substitutions to the formula
    node* skolemized_formula = substitute(formula, subst);

    return skolemized_formula;
}

void skolemize_all(context_t& tab_ctx) {
    for (auto& tabline : tab_ctx.tableau) {
        // Only process active formulas
        if (tabline.active && tabline.formula != nullptr) {
            // Apply skolem_form to the formula
            node* skolemized = skolem_form(tab_ctx, tabline.formula);

            // Replace the original formula with the skolemized formula
            tabline.formula = skolemized;

            // If the formula is a target, re-negate it
            if (tabline.target) {
                // Delete existing negation to prevent memory leaks
                delete tabline.negation;

                // Create a deep copy of the skolemized formula
                node* formula_copy = deep_copy(tabline.formula);

                // Negate the copied formula
                node* negated = negate_node(formula_copy);
                negated = hide_disjunction(negated);

                // Assign the negated formula to the negation field
                tabline.negation = negated;
            }
        }
    }
}

// Parameterize function: changes all free individual variables to parameters
node* parameterize(node* formula) {
    // If the node is a free individual variable, change it to parameter
    if (formula->type == VARIABLE && formula->vdata != nullptr) {
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
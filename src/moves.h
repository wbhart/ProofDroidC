#ifndef MOVES_H
#define MOVES_H

#include "node.h"
#include "context.h"
#include "substitute.h"
#include "unify.h"
#include <vector>
#include <string>

// Changes all free individual variables to parameters in the formula
node* parameterize(node* formula);

// Applies parameterize to all active formulas in the tableau
void parameterize_all(context_t& tab_ctx);

// Skolemizes an existentially quantified formula of the form \exists x (\phi(x))
// It also takes a list of universally quantified variable names in scope and the
// context managing variable indices.

// Returns the Skolemized formula \phi(x(y, z, ...)) and updates the Substitution
// with the new substitution that must be performed by the caller
node* skolemize(context_t& ctx, node* formula, const std::vector<std::string>& universals, Substitution& subst);

// Skolemizes an arbitrary formula, returning the result
node* skolem_form(context_t& ctx, node* formula);

// Skolemizes all active formulas
void skolemize_all(context_t& tab_ctx);

// Applies modus ponens to the given implication and unit clauses.
// Parameters:
// - implication: The implication formula P ∧ Q ∧ ... ∧ R → S.
// - unit_clauses: The unit clause formula list P_1, Q_1, ..., R_1.
// - ctx_var: The context for variable indexing and renaming.
// Returns:
// - A new node representing the result of modus ponens, or nullptr if unification fails.
node* modus_ponens(context_t& ctx_var, node* implication, const std::vector<node*>& unit_clauses);

// Applies modus tollens to the given implication and unit clauses.
// Parameters:
// - implication: The implication formula ¬S → ¬P ∨ ¬Q ∨ ... ∨ ¬R.
// - unit_clauses: The unit clause formula list P_1, Q_1, ..., R_1.
// - ctx_var: The context for variable indexing and renaming.
// Returns:
// - A new node representing the result of modus ponens, or nullptr if unification fails.
node* modus_tollens(context_t& ctx_var, node* implication, const std::vector<node*>& unit_clauses);

#endif // MOVES_H


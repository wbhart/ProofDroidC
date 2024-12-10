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
bool skolemize_all(context_t& tab_ctx, size_t start = 0);

// Applies modus ponens to the given implication and unit clauses.
// Parameters:
// - implication: The implication formula P ∧ Q ∧ ... ∧ R → S.
// - unit_clauses: The unit clause formula list P_1, Q_1, ..., R_1.
// - ctx_var: The context for variable indexing and renaming.
// - combined_subst: a presumably empty Substitution structure, which will be assigned
//   with all substitutions made by modus_ponens
// Returns:
// - A new node representing the result of modus ponens, or nullptr if unification fails.
node* modus_ponens(Substitution& combined_subst, context_t& ctx_var, node* implication, const std::vector<node*>& unit_clauses, bool silent=false);

// Applies modus tollens to the given implication and unit clauses.
// Parameters:
// - implication: The implication formula ¬S → ¬P ∨ ¬Q ∨ ... ∨ ¬R.
// - unit_clauses: The unit clause formula list P_1, Q_1, ..., R_1.
// - ctx_var: The context for variable indexing and renaming.
// - combined_subst: a presumably empty Substitution structure, which will be assigned
//   with all substitutions made by modus_ponens
// Returns:
// - A new node representing the result of modus ponens, or nullptr if unification fails.
node* modus_tollens(Substitution& combined_subst, context_t& ctx_var, node* implication, const std::vector<node*>& unit_clauses, bool silent=false);

// Performs modus ponens\tollens on specified lines where implication_line is the line
// number of the implication, other_lines are the line numbers of the unit lines,
// special lines is a list of lines containing special predicates, which do not need
// to correspond to anything, e.g. it can be a list of *all* active, special predicates
// in the tableau. Reasoning is performed under special implications and those are then
// checked after unification by this function
bool move_mpt(context_t& ctx, int implication_line, const std::vector<int>& other_lines, const std::vector<size_t>& special_lines, bool ponens, bool silent=false);

// Rewrite the formula with index formula_line using the rewrite rule with index
// rewrite_line in the tableau
bool move_rewrite(context_t& ctx, int formula_line, int rewrite_line, bool silent=false);

// Function to apply disjunctive idempotence: P ∨ P -> P
bool move_di(context_t& tab_ctx, size_t start = 0);

// Function to apply conjunctive idempotence: P ∧ P -> P
bool move_ci(context_t& tab_ctx, size_t start = 0);

// Split conjunctions P ∧ Q into P and Q
bool move_sc(context_t& tab_ctx, size_t start = 0);

// Split disjunctions P ∨ Q by splitting tableau
bool move_sd(context_t& tab_ctx, size_t line);

// Split lines (P ∨ Q) -> R into P -> R and Q -> R
bool move_sdi(context_t& tab_ctx, size_t start = 0);

// Split lines P -> (Q ∧ R) into P -> Q and P -> R
bool move_sci(context_t& tab_ctx, size_t start = 0);

// Split lines ¬(P -> Q) into P and ¬Q
bool move_ni(context_t& tab_ctx, size_t start = 0);

// Split P <-> Q into P -> Q and Q -> P
bool move_me(context_t& tab_ctx, size_t start);

// Conditional premise move: P -> Q to conditional hypothesis P and target Q
bool conditional_premise(context_t& tab_ctx, int index);

// Apply conditional premise to all lines
bool move_cp(context_t& tab_ctx, size_t start = 0);

// Apply all cleanup moves
bool cleanup_moves(context_t& tab_ctx, size_t start_line = 0);

// Apply only skolemize and move_me for definitions
bool cleanup_definition(context_t& tab_ctx, size_t start_line);

// Apply only skolemize for rewrites
bool cleanup_rewrite(context_t& tab_ctx, size_t start_line);

#endif // MOVES_H
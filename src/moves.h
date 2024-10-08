#ifndef MOVES_H
#define MOVES_H

#include "node.h"
#include "context.h"
#include "substitute.h"
#include <vector>
#include <string>

// Skolemizes an existentially quantified formula of the form \exists x (\phi(x))
// It also takes a list of universally quantified variable names in scope and the
// context managing variable indices.

// Returns the Skolemized formula \phi(x(y, z, ...)) and updates the Substitution
// with the new substitution that must be performed by the caller
node* skolemize(context_t& ctx, node* formula, const std::vector<std::string>& universals, Substitution& subst);

// Skolemizes an arbitrary formula, returning the result
node* skolem_form(context_t& ctx, node* formula);

// Changes all free individual variables to parameters in the formula
node* parameterize(node* formula);

#endif // MOVES_H


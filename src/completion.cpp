// completion.cpp

#include "completion.h"
#include "unify.h"
#include "substitute.h"
#include <iostream>

// Implementation of check_done
void check_done(context_t& ctx) {
    // Step 1: Negate formulas of all non-target lines starting from 'upto'
    for (int j = ctx.upto; j < static_cast<int>(ctx.tableau.size()); ++j) {
        tabline_t& current_line = ctx.tableau[j];
        if (!current_line.target) {
            current_line.negation = negate_node(current_line.formula);
        }
    }

    // Step 2: Unify negated formulas with previous non-proved lines
    for (int j = ctx.upto; j < static_cast<int>(ctx.tableau.size()); ++j) {
        tabline_t& current_line = ctx.tableau[j];
        if (!current_line.target) {
            for (int i = 0; i < j; ++i) {
                tabline_t& previous_line = ctx.tableau[i];
                if (!previous_line.dead) {
                    Substitution subst;
                    std::optional<Substitution> result = unify(current_line.negation, previous_line.formula, subst);
                    if (result.has_value()) {
                        // Add the pair (i, j) to unifications
                        current_line.unifications.emplace_back(i, j);
                    }
                }
            }
        }
    }

    // Step 3: Update 'upto' to the current length of the tableau
    ctx.upto = static_cast<int>(ctx.tableau.size());
}

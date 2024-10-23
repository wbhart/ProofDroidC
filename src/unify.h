#ifndef UNIFY_H
#define UNIFY_H

#include "substitute.h"
#include <optional>

std::optional<Substitution> unify(node* node1, node* node2, Substitution& subst, bool smgu=false);

#endif // UNIFY_H

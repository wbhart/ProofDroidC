#ifndef SUBSTITUTE_H
#define SUBSTITUTE_H

#include "node.h"
#include <unordered_map>
#include <optional>

// UnificationResult can be represented as a substitution map where a variable is mapped to its value
using Substitution = std::unordered_map<std::string, node*>;

node* substitute(node* formula, const Substitution& subst);

void cleanup_subst(Substitution& subst);

#endif // SUBSTITUTE_H


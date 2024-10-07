#ifndef SUBSTITUTE_H
#define SUBSTITUTE_H

#include "node.h"
#include <unordered_map>
#include <optional>

// UnificationResult can be represented as a substitution map where a variable is mapped to its value
using Substitution = std::unordered_map<std::string, node*>;

// Function declarations (you may add additional ones here if needed)
std::optional<Substitution> unify(node* node1, node* node2, Substitution& subst);

node* substitute(node* formula, const Substitution& subst);

#endif // SUBSTITUTE_H


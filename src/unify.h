#ifndef UNIFY_H
#define UNIFY_H

std::optional<Substitution> unify(node* node1, node* node2, Substitution& subst);

#endif // UNIFY_H

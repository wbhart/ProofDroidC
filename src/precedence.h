#ifndef PRECEDENCE_H
#define PRECEDENCE_H

#include "symbol_enum.h"
#include "node.h"
#include <string>
#include <map>
#include <iostream>

// Enum for associativity
enum class Associativity {
    LEFT,
    RIGHT,
    NONE
};

// Enum for fixity (infix, functional, none)
enum class Fixity {
    INFIX,
    FUNCTIONAL,
    NONE
};

// Define a structure to hold precedence information
struct PrecedenceInfo {
    int precedence;
    Associativity associativity;
    Fixity fixity;
    std::string repr;    // Representation for re-parsing
    std::string unicode; // Unicode representation for user display
};

// Define the precedence table as a map from symbol_enum to PrecedenceInfo
const std::map<symbol_enum, PrecedenceInfo> precedenceTable = {
    { SYMBOL_FORALL, {0, Associativity::NONE, Fixity::NONE, "\\forall", "∀"} },
    { SYMBOL_EXISTS, {0, Associativity::NONE, Fixity::NONE, "\\exists", "∃"} },
    { SYMBOL_IFF, {5, Associativity::RIGHT, Fixity::INFIX, "\\iff", "↔"} },
    { SYMBOL_IMPLIES, {5, Associativity::NONE, Fixity::INFIX, "\\implies", "→"} },
    { SYMBOL_AND, {5, Associativity::LEFT, Fixity::INFIX, "\\wedge", "∧"} },
    { SYMBOL_OR, {5, Associativity::LEFT, Fixity::INFIX, "\\vee", "∨"} },
    { SYMBOL_NOT, {0, Associativity::NONE, Fixity::FUNCTIONAL, "\\neg", "¬"} },
    { SYMBOL_TOP, {0, Associativity::NONE, Fixity::NONE, "\\top", "⊤"} },
    { SYMBOL_BOT, {0, Associativity::NONE, Fixity::NONE, "\\bot", "⊥"} },
    { SYMBOL_EQUALS, {4, Associativity::NONE, Fixity::INFIX, "=", "="} },
    { SYMBOL_LEQ, {4, Associativity::NONE, Fixity::INFIX, "\\leq", "≤"} },
    { SYMBOL_LT, {4, Associativity::NONE, Fixity::INFIX, "<", "<"} },
    { SYMBOL_SUBSET, {3, Associativity::NONE, Fixity::INFIX, "\\subset", "⊂"} },
    { SYMBOL_SUBSETEQ, {3, Associativity::NONE, Fixity::INFIX, "\\subseteq", "⊆"} },
    { SYMBOL_ELEM, {3, Associativity::NONE, Fixity::INFIX, "\\in", "∈"}},
    { SYMBOL_CAP, {2, Associativity::LEFT, Fixity::INFIX, "\\cap", "∩"} },
    { SYMBOL_CUP, {2, Associativity::LEFT, Fixity::INFIX, "\\cup", "∪"} },
    { SYMBOL_SETMINUS, {2, Associativity::LEFT, Fixity::INFIX, "\\setminus", "∖"} },
    { SYMBOL_TIMES, {2, Associativity::LEFT, Fixity::INFIX, "\\times", "×"} },
    { SYMBOL_ADD, {3, Associativity::LEFT, Fixity::INFIX, "+", "+"} },
    { SYMBOL_MUL, {2, Associativity::LEFT, Fixity::INFIX, "*", "*"} },
    { SYMBOL_EXP, {1, Associativity::RIGHT, Fixity::INFIX, "^", "^"} },
    { SYMBOL_POWERSET, {0, Associativity::NONE, Fixity::FUNCTIONAL, "\\mathcal{P}", "𝒫"} },
    { SYMBOL_EMPTYSET, {0, Associativity::NONE, Fixity::NONE, "\\emptyset", "∅"} },
    { SYMBOL_ONE, {0, Associativity::NONE, Fixity::NONE, "1", "1"} },
    { SYMBOL_MONE, {0, Associativity::NONE, Fixity::NONE, "-1", "-1"} }
};

// Function to retrieve precedence information based on the enum
inline PrecedenceInfo getPrecedenceInfo(symbol_enum sym) {
    auto it = precedenceTable.find(sym);
    if (it != precedenceTable.end()) {
        return it->second;
    }
    return {0, Associativity::NONE, Fixity::FUNCTIONAL, "", ""}; // Default for unknown symbols
}

#endif // PRECEDENCE_H

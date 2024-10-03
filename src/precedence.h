#ifndef PRECEDENCE_H
#define PRECEDENCE_H

#include "symbol_enum.h"
#include <string>
#include <map>

// Define a structure to hold precedence information
struct PrecedenceInfo {
    int precedence;
    std::string associativity;
    std::string fixity;
    std::string repr;    // Representation for re-parsing
    std::string unicode; // Unicode representation for user display
};

// Define the precedence table as a map from symbol_enum to PrecedenceInfo
const std::map<symbol_enum, PrecedenceInfo> precedenceTable = {
    { SYMBOL_FORALL, {0, "none", "none", "\\forall", "∀"} },
    { SYMBOL_EXISTS, {0, "none", "none", "\\exists", "∃"} },
    { SYMBOL_AND, {4, "left", "infix", "\\wedge", "∧"} },
    { SYMBOL_OR, {4, "left", "infix", "\\vee", "∨"} },
    { SYMBOL_SHEFFER, {4, "left", "infix", "\\uparrow", "↑"} },
    { SYMBOL_NOT, {0, "none", "prefix", "\\neg", "¬"} },
    { SYMBOL_TOP, {0, "none", "none", "\\top", "⊤"} },
    { SYMBOL_BOT, {0, "none", "none", "\\bot", "⊥"} },
    { SYMBOL_EQUALS, {3, "none", "infix", "=", "="} },
    { SYMBOL_SUBSET, {3, "none", "infix", "\\subset", "⊂"} },
    { SYMBOL_SUBSETEQ, {3, "none", "infix", "\\subseteq", "⊆"} },
    { SYMBOL_CAP, {2, "left", "infix", "\\cap", "∩"} },
    { SYMBOL_CUP, {2, "left", "infix", "\\cup", "∪"} },
    { SYMBOL_SETMINUS, {2, "left", "infix", "\\setminus", "∖"} },
    { SYMBOL_TIMES, {2, "left", "infix", "\\times", "×"} },
    { SYMBOL_POWERSET, {0, "none", "functional", "\\mathcal{P}", "𝒫"} },
    { SYMBOL_EMPTYSET, {0, "none", "none", "\\emptyset", "∅"} }
};

// Function to retrieve precedence information based on the enum
inline PrecedenceInfo getPrecedenceInfo(symbol_enum sym) {
    auto it = precedenceTable.find(sym);
    if (it != precedenceTable.end()) {
        return it->second;
    }
    return {0, "none", "none", "", ""}; // Default for unknown symbols
}

#endif // PRECEDENCE_H

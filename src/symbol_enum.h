#ifndef SYMBOL_ENUM_H
#define SYMBOL_ENUM_H

// Enum for representing various operators and constants in the AST
typedef enum {
    SYMBOL_NONE,
    SYMBOL_FORALL,
    SYMBOL_EXISTS,
    SYMBOL_IMPLIES,
    SYMBOL_IFF,
    SYMBOL_AND,
    SYMBOL_OR,
    SYMBOL_NOT,
    SYMBOL_EQUALS,
    SYMBOL_SUBSET,
    SYMBOL_SUBSETEQ,
    SYMBOL_ELEM,
    SYMBOL_TOP,
    SYMBOL_BOT,
    SYMBOL_POWERSET,
    SYMBOL_CAP,
    SYMBOL_CUP,
    SYMBOL_TIMES,
    SYMBOL_SETMINUS,
    SYMBOL_EMPTYSET
} symbol_enum;

#endif // SYMBOL_ENUM_H

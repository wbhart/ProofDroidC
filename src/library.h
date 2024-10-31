#ifndef LIBRARY_H
#define LIBRARY_H

#include <vector>
#include <string>

// Existing includes and declarations
#include "context.h"

// Function declaration for loading library
bool library_load(context_t& context, const std::string& base_str);

#endif // LIBRARY_H

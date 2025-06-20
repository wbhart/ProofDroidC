#ifndef AUTOMATION_H
#define AUTOMATION_H

#include "completion.h"
#include "context.h"
#include "hydra.h"
#include "moves.h"
#include "debug.h"
#include <vector>
#include <string>
#include <memory>
#include <algorithm>
#include <optional>

// Automation using a waterfall
bool automate(context_t& ctx);

#endif // AUTOMATION_H

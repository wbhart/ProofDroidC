// hydra.cpp

#include "hydra.h"
#include <algorithm>
#include <unordered_set>

#include "hydra.h"

// Initialize static member
int hydra::id_counter = 0;

// Default constructor implementation
hydra::hydra() : id(id_counter++) {
}

void hydra::print_targets() const {
    std::cout << "{";
    for (size_t i = 0; i < target_indices.size(); ++i) {
        if (i != 0) std::cout << ", ";
        std::cout << target_indices[i];
    }
    std::cout << "}";
}

// Adds a target index to the node
void hydra::add_target(int target) {
    target_indices.push_back(target);
}

// Helper function to check for conflict between two assumptions
bool hydra::find_conflict(const std::vector<int>& existing, const std::vector<int>& incoming, int& conflicting_n) const {
    if (existing.size() != incoming.size()) {
        return false;
    }

    // Create unordered sets for efficient lookup
    std::unordered_set<int> existing_set(existing.begin(), existing.end());
    std::unordered_set<int> incoming_set(incoming.begin(), incoming.end());

    // Find differences: elements in existing not in incoming and vice versa
    std::vector<int> existing_not_in_incoming;
    for (const int& num : existing_set) {
        if (incoming_set.find(num) == incoming_set.end()) {
            existing_not_in_incoming.push_back(num);
        }
    }

    std::vector<int> incoming_not_in_existing;
    for (const int& num : incoming_set) {
        if (existing_set.find(num) == existing_set.end()) {
            incoming_not_in_existing.push_back(num);
        }
    }

    // We are only interested in cases where:
    // - Exactly one element is missing in existing (which is -n of an element in incoming)
    // - Exactly one element is missing in incoming (which is n of an element in existing)
    if (existing_not_in_incoming.size() == 1 && incoming_not_in_existing.size() == 1) {
        int missing_in_incoming = existing_not_in_incoming[0];
        int missing_in_existing = incoming_not_in_existing[0];

        // Check if they are n vs -n
        if (missing_in_incoming == -missing_in_existing) {
            conflicting_n = missing_in_existing; // n is in incoming, -n is in existing
            return true;
        }
    }

    return false;
}

// Checks to see if an assumption is already in hydra
bool hydra::assumption_exists(const std::vector<int>& new_assumption) {
    // Create a sorted copy of the new assumption for consistent comparison
    std::vector<int> sorted_new = new_assumption;
    std::sort(sorted_new.begin(), sorted_new.end());

    // Check if the new assumption already exists in 'proved'
    for (const auto& existing_assm : proved) {
        if (existing_assm.size() != sorted_new.size()) {
            continue; // Different sizes cannot be identical
        }

        // Create sorted copies for comparison
        std::vector<int> sorted_existing = existing_assm;
        std::sort(sorted_existing.begin(), sorted_existing.end());

        if (sorted_existing == sorted_new) {
            // Assumption already exists, do not add
            return true;
        }
    }

    return false;
}

// Adds a proved assumption to the node with conflict handling
int hydra::add_assumption(const std::vector<int>& new_assumption) {
    if (new_assumption.empty()) { // Proved without assumptions
        proved.clear();
        proved.push_back(new_assumption);
        return 1;
    }
        
    // Create a sorted copy of the new assumption for consistent comparison
    std::vector<int> sorted_new = new_assumption;
    std::sort(sorted_new.begin(), sorted_new.end());

    // Iterate through existing proved assumptions to find conflicts
    for (auto it = proved.begin(); it != proved.end(); ) {
        // Check if we already have a more general condition
        if (std::includes((sorted_new).begin(), sorted_new.end(), (*it).begin(), (*it).end())) {
            // *it is a subset of sorted_new, so we return false immediately
            return -1;
        }
        int conflicting_n = 0;
        if (find_conflict(*it, sorted_new, conflicting_n)) {
            // Conflict detected where existing has -n and new has n

            // Modify the existing assumption: remove -n and add n
            std::vector<int> modified_assumption = *it;

            auto pos = std::find(modified_assumption.begin(), modified_assumption.end(), -conflicting_n);
            if (pos != modified_assumption.end()) {
                modified_assumption.erase(pos);          // Remove -n
            }

            // Remove the existing assumption from 'proved'
            proved.erase(it);

            // Recursively add the modified assumption
            // This handles any further conflicts that may arise
            return add_assumption(modified_assumption);
        } else {
            ++it; // No conflict with this assumption, move to the next
        }
    }

    // No conflict found, add the new assumption
    proved.push_back(new_assumption);
    return 0; // Returns true if assumptions is empty, false otherwise
}

// Adds a child hydra node
void hydra::add_child(const std::shared_ptr<hydra>& child) {
    children.emplace_back(child);
}

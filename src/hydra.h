#ifndef HYDRA_H
#define HYDRA_H

#include "debug.h"
#include <vector>
#include <memory>
#include <algorithm>
#include <iostream>
#include <string>

class hydra : public std::enable_shared_from_this<hydra> {
public:
    // Static counter for unique IDs
    static int id_counter;
    int id;

    // Public Members
    std::vector<int> target_indices;                       // List of target indices
    std::vector<std::vector<int>> proved;                  // Renamed from assumptions
    std::vector<std::shared_ptr<hydra>> children;          // List of child hydra nodes

    // Constructors
    hydra(); // Default constructor
    hydra(const std::vector<int>& targets, const std::vector<std::vector<int>>& proved_assumptions)
        : id(id_counter++), target_indices(targets), proved(proved_assumptions) {}

    // Destructor
    ~hydra() {}

    // Deleted copy constructor and assignment operator to prevent accidental copies
    hydra(const hydra&) = delete;
    hydra& operator=(const hydra&) = delete;

    // Member Functions
    void add_target(int target);
    int add_assumption(const std::vector<int>& new_assumption); // Return true if new_assumption already exists in hydra
    bool assumption_exists(const std::vector<int>& new_assumption); // Returns true if 'proved' is empty after adding
    void add_child(const std::shared_ptr<hydra>& child);
    bool find_conflict(const std::vector<int>& existing, const std::vector<int>& incoming, int& conflicting_n) const;
    void print_targets() const;
};

#endif // HYDRA_H

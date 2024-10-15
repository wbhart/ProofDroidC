// hydra.h

#ifndef HYDRA_H
#define HYDRA_H

#include <vector>
#include <memory>
#include <algorithm>

class hydra {
public:
    // Constructors
    hydra(); // Default constructor
    hydra(const std::vector<int>& targets, const std::vector<std::vector<int>>& proved_assumptions); // Parameterized constructor

    // Destructor
    ~hydra();

    // Member Functions
    void add_target(int target);
    bool add_assumption(const std::vector<int>& assumption); // Returns true if 'proved' is empty after adding
    void add_child(const std::shared_ptr<hydra>& child);
    bool find_conflict(const std::vector<int>& existing, const std::vector<int>& incoming, int& conflicting_n) const;

    // Public Members
    std::vector<int> target_indices;                       // List of target indices
    std::vector<std::vector<int>> proved;                  // Renamed from assumptions
    std::vector<std::shared_ptr<hydra>> children;          // List of child hydra nodes
};

#endif // HYDRA_H

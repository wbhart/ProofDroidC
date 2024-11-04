#ifndef DEBUG_H
#define DEBUG_H

#include <iostream>
#include <vector>

template <typename T>
void print_list(std::vector<T> const& list) {
    std::cout << "[";
    for (size_t i = 0; i < list.size(); ++i) {
        std::cout << list[i] + 1;
        if (i < list.size() - 1) {
            std::cout << ", ";
        }
    }
    std::cout << "]";
}

#endif

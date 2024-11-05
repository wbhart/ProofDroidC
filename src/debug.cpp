#include "debug.h"

void print_list(std::vector<std::string> const& list) {
    std::cout << "[";
    for (size_t i = 0; i < list.size(); ++i) {
        std::cout << list[i];
        if (i < list.size() - 1) {
            std::cout << ", ";
        }
    }
    std::cout << "]";
}

void print_list(std::vector<size_t> const& list) {
    std::cout << "[";
    for (size_t i = 0; i < list.size(); ++i) {
        std::cout << list[i] + 1;
        if (i < list.size() - 1) {
            std::cout << ", ";
        }
    }
    std::cout << "]";
}

void print_list(std::vector<int> const& list) {
    std::cout << "[";
    for (size_t i = 0; i < list.size(); ++i) {
        std::cout << list[i] + 1;
        if (i < list.size() - 1) {
            std::cout << ", ";
        }
    }
    std::cout << "]";
}

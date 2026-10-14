#include "../optimal.hpp"
#include "../workload.hpp"

#include <algorithm>
#include <cassert>
#include <iostream>
#include <random>
#include <string>
#include <vector>

// Slow Belady simulation: scan the remaining trace on each eviction.
size_t reference_hits(const std::vector<int>& keys, size_t capacity) {
    if (capacity == 0) {
        return 0;
    }

    std::vector<int> resident;
    size_t hits = 0;
    for (size_t i = 0; i < keys.size(); ++i) {
        if (std::find(resident.begin(), resident.end(), keys[i]) != resident.end()) {
            ++hits;
            continue;
        }
        if (resident.size() == capacity) {
            size_t victim = 0;
            size_t furthest = 0;
            for (size_t j = 0; j < resident.size(); ++j) {
                size_t next = i + 1;
                while (next < keys.size() && keys[next] != resident[j]) {
                    ++next;
                }
                if (next > furthest) {
                    furthest = next;
                    victim = j;
                }
            }
            resident.erase(resident.begin() + victim);
        }
        resident.push_back(keys[i]);
    }
    return hits;
}

void check(const std::vector<int>& keys, size_t capacity, size_t expected) {
    const benchmark::workload<int> workload(keys);
    assert(workload.size() == keys.size());
    assert(workload.optimal_hits(capacity) == expected);
    assert(workload.optimal_hits(0) == 0);
    assert(workload.optimal_hits(capacity) == expected);
    assert(benchmark::optimal_hits<int>(std::span<const int>(keys), capacity) == expected);
    for (size_t i = 0; i < keys.size(); ++i) {
        assert(workload[i] == keys[i]);
    }
}

int main() {
    check({}, 0, 0);
    check({}, 3, 0);
    check({1, 1, 1}, 0, 0);
    check({1, 1, 1}, 1, 2);
    check({1, 2, 3, 1, 2, 3}, 2, 2);
    check({1, 2, 3, 1, 2, 3}, 3, 3);
    check({1, 2, 3, 1, 4, 2, 1}, 3, 3);

    // Equal next-use positions (never used again) and oversized capacity.
    check({1, 2, 3, 4, 5}, 2, 0);
    check({1, 2, 3, 1, 4, 5, 1}, 2, 2);
    check({1, 2, 3, 1, 2, 3}, 100, 3);

    const benchmark::workload<std::string> words({"a", "b", "a", "c", "b"});
    assert(words.optimal_hits(2) == 2);

    std::mt19937 generator(42);
    std::uniform_int_distribution<int> length(0, 20);
    std::uniform_int_distribution<int> key(0, 5);
    for (size_t trial = 0; trial < 1000; ++trial) {
        std::vector<int> keys(length(generator));
        for (int& value : keys) {
            value = key(generator);
        }
        for (size_t capacity = 0; capacity <= 8; ++capacity) {
            check(keys, capacity, reference_hits(keys, capacity));
        }
    }

    std::cout << "optimal_hits tests passed\n";
}

#pragma once

#include <algorithm>
#include <cstddef>
#include <limits>
#include <utility>
#include <vector>

#include "optimal.hpp"

namespace benchmark {

template <typename KeyT>
class workload {
private:
    std::vector<KeyT> data;

public:
    explicit workload() = default;
    explicit workload(workload&& other) = default;
    explicit workload(std::vector<KeyT> keys) : data(std::move(keys)) {}
    workload(const workload&) = default;
    workload& operator=(const workload&)  = default;
    workload& operator=(workload&& other) = default;
    ~workload() = default;

    const KeyT& operator[] (size_t idx) const { return data.at(idx); };
    size_t size() const { return data.size(); };

    template <typename CacheT>
    void run(CacheT& cache, size_t count = std::numeric_limits<size_t>::max()) const {
        const size_t limit = std::min(count, data.size());
        for (size_t i = 0; i < limit; ++i) {
            cache.lookup_update(data[i]);
        }
    }

    size_t optimal_hits(size_t capacity) const {
        return benchmark::optimal_hits<KeyT>(std::span<const KeyT>(data), capacity);
    }
};

}

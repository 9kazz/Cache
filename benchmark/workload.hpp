#pragma once

#include <cstddef>
#include <utility>
#include <vector>

namespace benchmark {

template <typename KeyT>
class workload {
private:
    std::vector<KeyT> data;

public:
    explicit workload() = default;
    explicit workload(workload&& other) = default;
    explicit workload(std::vector<KeyT>&& keys) : data(std::move(keys)) {}
    workload(const workload&) = default;
    workload& operator=(const workload&) = default;
    workload& operator=(const workload&& other) = default;
    ~workload() = default;

    const KeyT& operator[] (size_t idx) { return data.at(idx); };
    size_t size() { return data.size(); };
};

}
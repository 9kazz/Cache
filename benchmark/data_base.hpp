#pragma once

#include <unordered_map>
#include <utility>

namespace benchmark {

template <typename T, typename KeyT>
class data_base {
private:
    std::unordered_map<KeyT, T> data;

public:
    explicit data_base() = default;
    explicit data_base(data_base&& other) = default;
    explicit data_base(std::unordered_map<KeyT, T>&& pages) : data(std::move(pages)) {}
    data_base(const data_base&) = delete;
    ~data_base() = default;
    data_base& operator=(const data_base&) = delete;
    data_base& operator=(const data_base&& other) = default;

    size_t size() { return data.size(); };
    std::pair<T, bool> get_page(const KeyT& key) const;
};

template <typename T, typename KeyT>
std::pair<T, bool> data_base<T, KeyT>::get_page(const KeyT& key) const {
    return {data.at(key), true};
}

}
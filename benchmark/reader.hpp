#pragma once

#include <fstream>
#include <stdexcept>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

#include "third_party/nlohmann/json.hpp"

namespace benchmark {

template <typename T, typename KeyT>
class reader {
private:
    std::unordered_map<KeyT, T> all_pages;
    std::vector<KeyT> access_seq;

public:
    void get_data(const char* file_name);
    std::unordered_map<KeyT, T>& pages() { return all_pages; }
    std::vector<KeyT>& sequence()        { return access_seq; }
    const std::unordered_map<KeyT, T>& pages() const    { return all_pages; }
    const std::vector<KeyT>&  sequence() const          { return access_seq; }
};

template <typename T, typename KeyT>
void reader<T, KeyT>::get_data(const char* file_name) {
    std::ifstream file(file_name);
    if (!file) {
        throw std::invalid_argument("reader::get_data: null stream");
    }

    const auto document = nlohmann::json::parse(file);
    if (!document.is_object() || !document.contains("pages") || !document.at("pages").is_array()) {
        throw std::invalid_argument("reader::get_data: expected a pages array");
    }

    const auto& pages = document.at("pages");
    std::unordered_map<KeyT, T> temp_pages; // fill temp_pages to save original data clear in failure case
    temp_pages.reserve(pages.size());

    for (const auto& page : pages) {
        if (!page.is_object() || !page.contains("key") || !page.contains("content")) {
            throw std::invalid_argument("reader::get_data: page requires key and content");
        }

        const auto& key     = page.at("key");
        const auto& content = page.at("content");

        // compile-time check for integer keys only
        if constexpr (std::is_integral_v<KeyT> && !std::is_same_v<KeyT, bool>) {
            if (!key.is_number_integer()) {
                throw std::invalid_argument("reader::get_data: expected an integer key");
            }
            const bool fits = key.is_number_unsigned()
                ? std::in_range<KeyT>(key.get<nlohmann::json::number_unsigned_t>())
                : std::in_range<KeyT>(key.get<nlohmann::json::number_integer_t>());
            if (!fits) {
                throw std::out_of_range("reader::get_data: key out of range");
            }
        }

        auto real_key     = key.get<KeyT>();
        auto real_content = content.get<T>();

        bool page_is_new = temp_pages.emplace(std::move(real_key), std::move(real_content)).second;
        if (!page_is_new) {
            throw std::invalid_argument("reader::get_data: duplicate page key");
        }
    }

    if (!document.is_object() || !document.contains("workload") || !document.at("workload").is_array()) {
        throw std::invalid_argument("reader::get_data: expected a keys array");
    }

    const auto& keys = document.at("workload");
    std::vector<KeyT> temp_seq; // fill temp_seq to save original data clear in failure case
    temp_seq.reserve(keys.size());

    for (const auto& key : keys) {
        // compile-time check for integer keys only
        if constexpr (std::is_integral_v<KeyT> && !std::is_same_v<KeyT, bool>) {
            if (!key.is_number_integer()) {
                throw std::invalid_argument("reader::get_data: expected an integer key");
            }
            const bool fits = key.is_number_unsigned()
                ? std::in_range<KeyT>(key.get<nlohmann::json::number_unsigned_t>())
                : std::in_range<KeyT>(key.get<nlohmann::json::number_integer_t>());
            if (!fits) {
                throw std::out_of_range("reader::get_data: key out of range");
            }
        }

        auto real_key = key.get<KeyT>();
        temp_seq.push_back(std::move(real_key));
    }
    all_pages.swap(temp_pages);
    access_seq.swap(temp_seq);
}

}

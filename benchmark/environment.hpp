#pragma once

#include <fstream>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>
#include <iostream>
#include "third_party/nlohmann/json.hpp"

namespace benchmark {

template <typename T, typename KeyT>
class environment {
public:
    enum CACHE_TYPE {
        LRU  = 1,
        LFU  = 2,
        QQ   = 3,
        ARC  = 4,
        LIRS = 5
    };
    struct cache_level {
        CACHE_TYPE type;
        size_t size;
    };

private:
    static const char* cache_name(CACHE_TYPE type) {
        switch (type) {
        case LRU:  return "LRU";
        case LFU:  return "LFU";
        case QQ:   return "QQ";
        case ARC:  return "ARC";
        case LIRS: return "LIRS";
        default:   throw std::invalid_argument("environment: unknown cache type");
        }
    }

    // need for convertion cache names from JSON to CACHE_TYPE
    inline static const std::unordered_map<std::string, CACHE_TYPE> cache_types {
        {"LRU",  LRU},
        {"LFU",  LFU},
        {"QQ",   QQ},
        {"ARC",  ARC},
        {"LIRS", LIRS}
    };

    std::vector<cache_level> hierarchy_;
    std::unordered_map<KeyT, T> all_pages_;
    std::vector<KeyT> access_seq_;

public:
    explicit environment()  = default;
    ~environment() = default;
    environment(const environment&) = delete;
    environment& operator=(const environment&) = delete;

    void get_data(const char* file_name);
    void get_config(const char* file_name);
    
    void   append_level(CACHE_TYPE type, size_t size);
    size_t cnt_levels()  const { return hierarchy_.size(); }
    void   print_cache() const;

    std::vector<cache_level>&       hierarchy()       { return hierarchy_; }
    const std::vector<cache_level>& hierarchy() const { return hierarchy_; }
    std::unordered_map<KeyT, T>&        pages()       { return all_pages_; }
    const std::unordered_map<KeyT, T>&  pages() const { return all_pages_; }
    std::vector<KeyT>&               sequence()       { return access_seq_; }
    const std::vector<KeyT>&         sequence() const { return access_seq_; }
};

template <typename T, typename KeyT>
void environment<T, KeyT>::append_level(CACHE_TYPE type, size_t size) {
    cache_name(type); // validate the type before modifying the hierarchy
    if (size == 0) {
        throw std::invalid_argument("environment::append_level: cache size must be positive");
    }
    hierarchy_.push_back(cache_level{type, size});
}

template <typename T, typename KeyT>
void environment<T, KeyT>::print_cache() const {
    std::cout << "Cache hierarchy, levels: " << cnt_levels() << '\n';
    for (size_t i = 0; i < hierarchy_.size(); ++i) {
        const auto& level = hierarchy_[i];
        std::cout << "L" << i + 1 << ": " << cache_name(level.type)
                  << ", " << level.size << " pages\n";
    }
}

template <typename T, typename KeyT>
void environment<T, KeyT>::get_data(const char* file_name) {
    std::ifstream file(file_name);
    if (!file.is_open()) {
        throw std::invalid_argument("environment::get_data: fail to open file");
    }

    // parse pages
    const auto document = nlohmann::json::parse(file);
    if (!document.is_object() || !document.contains("pages") || !document.at("pages").is_array()) {
        throw std::invalid_argument("environment::get_data: expected a pages array");
    }

    const auto& pages = document.at("pages");
    std::unordered_map<KeyT, T> temp_pages; // fill temp_pages to save original data clear in failure case
    temp_pages.reserve(pages.size());

    for (const auto& page : pages) {
        if (!page.is_object() || !page.contains("key") || !page.contains("content")) {
            throw std::invalid_argument("environment::get_data: page requires key and content");
        }

        const auto& key     = page.at("key");
        const auto& content = page.at("content");

        // compile-time check for integer keys only
        if constexpr (std::is_integral_v<KeyT> && !std::is_same_v<KeyT, bool>) {
            if (!key.is_number_integer()) {
                throw std::invalid_argument("environment::get_data: expected an integer key");
            }
            const bool fits = key.is_number_unsigned()
                ? std::in_range<KeyT>(key.get<nlohmann::json::number_unsigned_t>())
                : std::in_range<KeyT>(key.get<nlohmann::json::number_integer_t>());
            if (!fits) {
                throw std::out_of_range("environment::get_data: key out of range");
            }
        }

        auto real_key     = key.get<KeyT>();
        auto real_content = content.get<T>();

        bool page_is_new = temp_pages.emplace(std::move(real_key), std::move(real_content)).second;
        if (!page_is_new) {
            throw std::invalid_argument("environment::get_data: duplicate page key");
        }
    }

    // parse workload
    if (!document.is_object() || !document.contains("workload") || !document.at("workload").is_array()) {
        throw std::invalid_argument("environment::get_data: expected a keys array");
    }

    const auto& keys = document.at("workload");
    std::vector<KeyT> temp_seq; // fill temp_seq to save original data clear in failure case
    temp_seq.reserve(keys.size());

    for (const auto& key : keys) {
        // compile-time check for integer keys only
        if constexpr (std::is_integral_v<KeyT> && !std::is_same_v<KeyT, bool>) {
            if (!key.is_number_integer()) {
                throw std::invalid_argument("environment::get_data: expected an integer key");
            }
            const bool fits = key.is_number_unsigned()
                ? std::in_range<KeyT>(key.get<nlohmann::json::number_unsigned_t>())
                : std::in_range<KeyT>(key.get<nlohmann::json::number_integer_t>());
            if (!fits) {
                throw std::out_of_range("environment::get_data: key out of range");
            }
        }

        auto real_key = key.get<KeyT>();
        temp_seq.push_back(std::move(real_key));
    }
    all_pages_.swap(temp_pages);
    access_seq_.swap(temp_seq);
}

template <typename T, typename KeyT>
void environment<T, KeyT>::get_config(const char* file_name) {
    std::ifstream file(file_name);
    if (!file.is_open()) {
        throw std::invalid_argument("environment::get_config: fail to open file");
    }

    // parse count_of_levels
    const auto document = nlohmann::json::parse(file);
    if (!document.is_object() || !document.contains("count_of_levels")) {
        throw std::invalid_argument("environment::get_config: expected count_of_levels");
    }

    const auto& count = document.at("count_of_levels");
    if (!count.is_number_integer() || count <= 0) {
        throw std::invalid_argument("environment::get_config: count_of_levels must be a positive integer");
    }
    // validate cnt_levels
    const bool fits = count.is_number_unsigned()
        ? std::in_range<size_t>(count.get<nlohmann::json::number_unsigned_t>())
        : std::in_range<size_t>(count.get<nlohmann::json::number_integer_t>());
    if (!fits) {
        throw std::out_of_range("environment::get_config: count_of_levels out of range");
    }

    const auto cnt_levels = count.get<size_t>();

    // parse cache hierarchy
    if (!document.contains("hierarchy") || !document.at("hierarchy").is_array()) {
        throw std::invalid_argument("environment::get_config: expected a cache levels array in hierarchy");
    }

    const auto& all_cache = document.at("hierarchy");
    if (all_cache.size() != cnt_levels) {
        throw std::invalid_argument("environment::get_config: count_of_levels must match hierarchy size");
    }

    std::vector<cache_level> temp_hierarchy; // keep the original hierarchy on failure
    temp_hierarchy.reserve(cnt_levels);

    for (const auto& one_cache : all_cache) {
        // parse cache type
        if (!one_cache.is_object() || !one_cache.contains("type") || !one_cache.contains("size")) {
            throw std::invalid_argument("environment::get_config: each cache level requires type and size");
        }
        const auto& type = one_cache.at("type");
        if (!type.is_string()) {
            throw std::invalid_argument("environment::get_config: cache type must be a string");
        }
        const auto name = type.get<std::string>();
        const auto cache_type = cache_types.find(name);
        if (cache_type == cache_types.end()) {
            throw std::invalid_argument("environment::get_config: unknown cache type '" + name + "'");
        }
        
        // parse cache size
        const auto& size = one_cache.at("size");
        if (!size.is_number_integer() || size <= 0) {
            throw std::invalid_argument("environment::get_config: cache size must be a positive integer (pages)");
        }
        const bool size_fits = size.is_number_unsigned()
            ? std::in_range<size_t>(size.get<nlohmann::json::number_unsigned_t>())
            : std::in_range<size_t>(size.get<nlohmann::json::number_integer_t>());
        if (!size_fits) {
            throw std::out_of_range("environment::get_config: cache size out of range");
        }
        temp_hierarchy.push_back({cache_type->second, size.get<size_t>()});
    }

    hierarchy_.swap(temp_hierarchy);
}

}

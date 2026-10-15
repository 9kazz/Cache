#pragma once

#include <fstream>
#include <set>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>
#include <iostream>
#include <getopt.h>
#include "third_party/nlohmann/json.hpp"
#include "dataset_metadata.hpp"

namespace benchmark {

template <typename T, typename KeyT>
class environment {
public:
    using cache_level  = std::pair<std::string, size_t>;
    using cache_levels = std::vector<cache_level>;

private:
    inline static const std::set<std::string> cache_types {
        "lru", "lfu", "2q", "arc", "lirs"
    };

    std::string data_;
    std::string config_; 

    cache_levels hierarchy_;
    std::unordered_map<KeyT, T> all_pages_;
    std::vector<KeyT> access_seq_;
    dataset_metadata metadata_;

    static KeyT read_key(const nlohmann::json& key) {
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
        return key.get<KeyT>();
    }

public:
    explicit environment()  = default;
    ~environment() = default;
    environment(const environment&) = delete;
    environment& operator=(const environment&) = delete;

    bool setup      (int argc, char* argv[]);
    bool setup      (const char* data_file, const char* config_file);
    void get_data   (const std::string file_name);
    void get_data   ();
    void get_config (const std::string file_name);
    void get_config ();
    void get_opt    (int argc, char* argv[]);
    
    void   append_level(std::string type, size_t size);
    size_t cnt_levels()  const { return hierarchy_.size(); }

    cache_levels&                   hierarchy()       { return hierarchy_; }
    const cache_levels&             hierarchy() const { return hierarchy_; }
    std::unordered_map<KeyT, T>&        pages()       { return all_pages_; }
    const std::unordered_map<KeyT, T>&  pages() const { return all_pages_; }
    std::vector<KeyT>&               sequence()       { return access_seq_; }
    const std::vector<KeyT>&         sequence() const { return access_seq_; }
    const dataset_metadata&         metadata()  const { return metadata_; }
};

template <typename T, typename KeyT>
bool environment<T, KeyT>::setup(int argc, char* argv[]) {
    try {
        get_opt(argc, argv);
        get_data();
        get_config();
    } catch (const std::exception& error) {
        std::cerr << "Environment was not set up: " << "\"" << error.what() << "\"\n";
        return false;
    }
    
    return true;
}

template <typename T, typename KeyT>
bool environment<T, KeyT>::setup(const char* data_file, const char* config_file) {
    try {
        get_data(data_file);
        get_config(config_file);
    } catch (const std::exception& error) {
        std::cerr << "Environment was not set up: " << "\"" << error.what() << "\"\n";
        return false;
    }
    
    return true;
}

template <typename T, typename KeyT>
void environment<T, KeyT>::append_level(std::string type, size_t size) {
    // validate the type before modifying the hierarchy
    if (!cache_types.contains(type)) {
        throw std::invalid_argument("environment: unknown cache type");
    }
    if (size == 0) {
        throw std::invalid_argument("environment::append_level: cache size must be positive");
    }
    hierarchy_.push_back(cache_level{type, size});
}

template <typename T, typename KeyT>
void environment<T, KeyT>::get_data() {
    if (data_.empty()) {
        throw std::invalid_argument("environment::get_data: data file name is empty");
    }
    get_data(data_);
}

template <typename T, typename KeyT>
void environment<T, KeyT>::get_data(const std::string file_name) {
    if (file_name.empty()) {
        throw std::invalid_argument("environment::get_data: empty string file_name");
    }    
    
    std::ifstream file(file_name);
    if (!file.is_open()) {
        throw std::invalid_argument("environment::get_data: fail to open file");
    }

    const auto document = nlohmann::json::parse(file);
    if (!document.is_object()) {
        throw std::invalid_argument("environment::get_data: expected a JSON object");
    }

    // parse matadata
    dataset_metadata temp_metadata;
    temp_metadata.name = document.value("name", std::string{});
    temp_metadata.description = document.value("description", std::string{});
    temp_metadata.params = document.value("params", nlohmann::json::object());
    temp_metadata.stats = document.value("stats", nlohmann::json::object());
    if (!temp_metadata.params.is_object() || !temp_metadata.stats.is_object()) {
        throw std::invalid_argument("environment::get_data: params and stats must be objects");
    }

    // parse pages
    if (!document.contains("pages") || !document.at("pages").is_array()) {
        throw std::invalid_argument("environment::get_data: expected a pages array");
    }

    const auto& pages = document.at("pages");
    // keep the original data and metadata on failure
    std::unordered_map<KeyT, T> temp_pages;
    temp_pages.reserve(pages.size());

    for (const auto& page : pages) {
        if (!page.is_object() || !page.contains("key") || !page.contains("content")) {
            throw std::invalid_argument("environment::get_data: page requires key and content");
        }

        auto real_key     = read_key(page.at("key"));
        auto real_content = page.at("content").get<T>();

        bool page_is_new = temp_pages.emplace(std::move(real_key), std::move(real_content)).second;
        if (!page_is_new) {
            throw std::invalid_argument("environment::get_data: duplicate page key");
        }
    }

    // parse workload
    if (!document.contains("workload") || !document.at("workload").is_array()) {
        throw std::invalid_argument("environment::get_data: expected a keys array");
    }

    const auto& keys = document.at("workload");
    std::vector<KeyT> temp_seq;
    temp_seq.reserve(keys.size());

    for (const auto& key : keys) {
        temp_seq.push_back(read_key(key));
    }
    all_pages_.swap(temp_pages);
    access_seq_.swap(temp_seq);
    std::swap(metadata_, temp_metadata);
}

template <typename T, typename KeyT>
void environment<T, KeyT>::get_config() {
    if (config_.empty()) {
        throw std::invalid_argument("environment::get_config: config file name is empty");
    }
    get_config(config_);
}

template <typename T, typename KeyT>
void environment<T, KeyT>::get_config(const std::string file_name) {
    if (file_name.empty()) {
        throw std::invalid_argument("environment::get_config: empty string file_name");
    }    

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
        if (!cache_types.contains(name)) {
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
        temp_hierarchy.push_back(cache_level{name, size.get<size_t>()});
    }

    hierarchy_.swap(temp_hierarchy);
}

template <typename T, typename KeyT>
void environment<T, KeyT>::get_opt(int argc, char* argv[]) {
    if (!argv) {
        throw std::invalid_argument("environment::get_opt: null pointer to argv");
    }

    const struct option long_opt[] = {
        {"config", required_argument, 0, 'c'},
        {"data",   required_argument, 0, 'd'},
        {0,        0,                 0,  0 }
    };

    std::string temp_config;
    std::string temp_data;

    while (true) {
        const int opt = getopt_long(argc, argv, "c:d:", long_opt, nullptr);

        if (opt == -1)
            break;

        switch (opt) {
        case 'c':
            temp_config = optarg;
            break;

        case 'd':
            temp_data = optarg;
            break;

        case '?':
            throw std::runtime_error("environment::get_opt: invalid option");

        default:
            throw std::runtime_error("environment::get_opt: unexpected getopt_long result: 0");
        }
    }

    if (optind < argc) {
        throw std::runtime_error("environment::get_opt: unexpected positional argument (may exist some options without required argument)");
    }

    data_.swap(temp_data);
    config_.swap(temp_config);
}

}

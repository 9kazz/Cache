#pragma once

#include <cctype>
#include <cstddef>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "../caches/cache_hierarchy.hpp"
#include "dataset_metadata.hpp"

namespace benchmark {

template <typename T, typename KeyT>
void print_statistics(const caches::cache_hierarchy<T, KeyT>& hierarchy,
                      const std::vector<std::pair<std::string, size_t>>& levels,
                      const dataset_metadata& metadata = {},
                      std::ostream& output = std::cout) {
    if (levels.size() != hierarchy.levels()) {
        throw std::invalid_argument("print_statistics: level count does not match hierarchy");
    }
    const auto& first = hierarchy.level(0);
    const size_t requests = first.hits() + first.misses();
    const size_t misses = hierarchy.level(hierarchy.levels() - 1).misses();
    const size_t hits = requests - misses;
    const double hit_rate = requests != 0 ? 100.0 * hits / requests : 0.0;
    const double miss_rate = requests != 0 ? 100.0 * misses / requests : 0.0;

    std::ostringstream report;
    report << std::fixed << std::setprecision(2)
           << "========================================================================\n"
           << "Benchmark data information\n";
    if (!metadata.name.empty()) {
        report << "Workload: " << metadata.name << '\n';
    }
    if (!metadata.description.empty()) {
        report << "Description: " << metadata.description << '\n';
    }
    if (!metadata.params.empty()) {
        report << "Params: " << metadata.params.dump() << '\n';
    }
    if (!metadata.stats.empty()) {
        report << "Workload stats: " << metadata.stats.dump() << '\n';
    }
    if (!metadata.name.empty() || !metadata.description.empty() ||
        !metadata.params.empty() || !metadata.stats.empty()) {
        report << '\n';
    }
    report << "------------------------------------------------------------------------\n"
           << "Cache statistics\n"
           << "Requests: " << requests << '\n'
           << "Hits:     " << hits << " (" << hit_rate << "%)\n"
           << "Misses:   " << misses << " (" << miss_rate << "%, storage reads)\n\n"
           << std::left << std::setw(6) << "Level"
           << std::setw(6) << "Type" << std::right
           << std::setw(10) << "Capacity"
           << std::setw(12) << "Requests"
           << std::setw(12) << "Hits"
           << std::setw(12) << "Misses"
           << std::setw(12) << "Hit rate" << '\n';

    for (size_t i = 0; i < hierarchy.levels(); ++i) {
        std::string algorithm = levels[i].first;
        for (char& c : algorithm) {
            c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
        }
        const auto& level = hierarchy.level(i);
        const size_t level_requests = level.hits() + level.misses();
        const double level_hit_rate = level_requests != 0
            ? 100.0 * level.hits() / level_requests : 0.0;

        report << std::left << std::setw(6) << "L" + std::to_string(i + 1)
               << std::setw(6) << algorithm << std::right
               << std::setw(10) << level.capacity()
               << std::setw(12) << level_requests
               << std::setw(12) << level.hits()
               << std::setw(12) << level.misses()
               << std::setw(11) << level_hit_rate << "%\n";
    }
    report << "========================================================================\n";
    
    output << report.str();
}

}

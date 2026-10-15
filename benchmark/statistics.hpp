#pragma once

#include <cstddef>
#include <iomanip>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>

#include "../caches/cache_hierarchy.hpp"
#include "dataset_metadata.hpp"

namespace benchmark {

template <typename T, typename KeyT>
void print_statistics(const caches::cache_hierarchy<T, KeyT>& hierarchy,
                      std::optional<size_t> optimal_hits = std::nullopt,
                      const dataset_metadata& metadata = {},
                      std::ostream& output = std::cout) 
{
    const auto& first = hierarchy.level(0);
    const size_t requests = first.hits() + first.misses();
    const size_t misses = hierarchy.level(hierarchy.levels() - 1).misses();
    const size_t hits = requests - misses;
    const double hit_rate = requests != 0 ? 100.0 * hits / requests : 0.0;
    const double miss_rate = requests != 0 ? 100.0 * misses / requests : 0.0;

    std::ostringstream report;
    report << std::fixed << std::setprecision(2)
           << "========================================================================\n"
           << "BENCHMARK DATA INFORMATION\n\n";
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
    report << "------------------------------------------------------------------------\n"
           << "CACHE STATISTIC\n\n"
           << "Requests: \t" << requests << '\n'
           << "Hits:     \t" << hits << " (" << hit_rate << "%)\n";
    if (optimal_hits) {
        const auto optimal_rate = requests != 0 ? 100.0 * (*optimal_hits) / requests : 0.0;
        report << "Optimal hits: \t" << *optimal_hits << " (" << optimal_rate << "%)\n";
    }
    report << "Misses:   \t" << misses << " (" << miss_rate << "%, storage reads)\n\n"
           << std::left << std::setw(6) << "Level"
           << std::setw(6) << "Type" << std::right
           << std::setw(10) << "Capacity"
           << std::setw(12) << "Requests"
           << std::setw(12) << "Hits"
           << std::setw(12) << "Misses"
           << std::setw(12) << "Hit rate" << '\n';

    const auto arch = hierarchy.architecture();
    for (size_t i = 0; i < hierarchy.levels(); ++i) {
        const auto& level = hierarchy.level(i);
        const size_t level_requests = level.hits() + level.misses();
        const double level_hit_rate = level_requests != 0
            ? 100.0 * level.hits() / level_requests : 0.0;

        report << std::left << std::setw(6) << "L" + std::to_string(i + 1)
               << std::setw(6) << arch[i].first << std::right
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

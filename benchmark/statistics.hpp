#pragma once

#include <cstddef>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>

#include "../caches/cache_hierarchy.hpp"

namespace benchmark {

template <typename T, typename KeyT>
void print_statistics(const caches::cache_hierarchy<T, KeyT>& hierarchy,
                      std::ostream& output = std::cout) {
    const auto& first = hierarchy.level(0);
    const size_t requests = first.hits() + first.misses();
    const size_t misses = hierarchy.level(hierarchy.levels() - 1).misses();
    const size_t hits = requests - misses;
    const double hit_rate = requests != 0 ? 100.0 * hits / requests : 0.0;
    const double miss_rate = requests != 0 ? 100.0 * misses / requests : 0.0;

    std::ostringstream report;
    report << std::fixed << std::setprecision(2)
           << "--------------------------------------------------------------------\n"
           << "Cache statistics\n"
           << "Requests: " << requests << '\n'
           << "Hits:     " << hits << " (" << hit_rate << "%)\n"
           << "Misses:   " << misses << " (" << miss_rate << "%, storage reads)\n\n"
           << std::left << std::setw(8) << "Level" << std::right
           << std::setw(10) << "Capacity"
           << std::setw(12) << "Requests"
           << std::setw(12) << "Hits"
           << std::setw(12) << "Misses"
           << std::setw(12) << "Hit rate" << '\n';

    for (size_t i = 0; i < hierarchy.levels(); ++i) {
        const auto& level = hierarchy.level(i);
        const size_t level_requests = level.hits() + level.misses();
        const double level_hit_rate = level_requests != 0
            ? 100.0 * level.hits() / level_requests : 0.0;

        report << std::left << std::setw(8) << "L" + std::to_string(i + 1) << std::right
               << std::setw(10) << level.capacity()
               << std::setw(12) << level_requests
               << std::setw(12) << level.hits()
               << std::setw(12) << level.misses()
               << std::setw(11) << level_hit_rate << "%\n";
    }
    report << "--------------------------------------------------------------------\n";
    
    output << report.str();
}

}

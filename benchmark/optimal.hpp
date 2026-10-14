#pragma once

#include <cstddef>
#include <iterator>
#include <map>
#include <span>
#include <unordered_map>
#include <vector>

namespace benchmark {

// returns optimal hits statistics for given workload. relates model of "perfect cache" with given capacity
template <typename KeyT>
size_t optimal_hits(std::span<const KeyT> keys, size_t capacity)
{
    if (capacity == 0 || keys.empty()) {
        return 0;
    } 
    const size_t count = keys.size();
    std::vector<size_t> next_use(count, count); 
    std::unordered_map<KeyT, size_t> last_seen;

    // count means the page will never be requested again
    for (size_t it = count; it > 0;) {
        --it;
        const auto seen = last_seen.find(keys[it]);
        if (seen != last_seen.end()) {
            next_use[it] = seen->second;
        }
        last_seen[keys[it]] = it;
    }

    using next_map_t = std::multimap<size_t, KeyT>;
    using resident_map_t = std::unordered_map<KeyT, typename next_map_t::iterator>;

    next_map_t usage_prior; 
    resident_map_t resident; // hash map to fast check if page is resident 
    size_t hits = 0;

    for (size_t it = 0; it < count; ++it) {
        const KeyT& key  = keys[it];
        const auto found = resident.find(key);
        if (found != resident.end()) {
            // cache hit
            ++hits;
            usage_prior.erase(found->second);
            found->second = usage_prior.emplace(next_use[it], key);
        } else {
            // cache miss
            if (resident.size() == capacity) {
                // full cache
                const auto victim = std::prev(usage_prior.end()); // evict element which will be required later than other resident elements
                resident.erase(victim->second);
                usage_prior.erase(victim);
            }
            const auto position = usage_prior.emplace(next_use[it], key);
            resident.emplace(key, position);
        }
    }

    return hits;
}

}

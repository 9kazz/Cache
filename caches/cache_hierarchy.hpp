#pragma once

// TODO-----------------------------------------------------------------
// [small]  level(i) uses caches_.at(i), which throws std::out_of_range.
//          Compare with operator[] - when is the check worth it?
// [small]  make_cache() is the only place that knows all strategy names.
//          The JSON config from the project plan should go through it too.
// [medium] Levels are created bottom-up because a level needs the level
//          below it in its constructor. Why does this make a cycle
//          (L1 -> L2 -> L1) impossible to build?
// [medium] Every level keeps a raw pointer next_ to the level below. The
//          levels live in caches_ as unique_ptr, so they never move. What if
//          caches_ were std::vector<lru_cache> by value and it grew?
// [medium] Inclusive on the way up: a page found in L2 is also stored in L1,
//          so the same page may sit in several levels. An exclusive hierarchy
//          would move it instead - what would the base class need for that
//          (hint: an erase/extract method)?
// [big]    Statistics: level i only sees the misses of level i - 1. What does
//          "hit rate of L2" mean then? Storage reads = misses of the last level.
// [big]    Google tests: a 1-level hierarchy gives the same hits as the plain
//          strategy; L2 hits == the L2 strategy run on L1's miss stream;
//          storage reads == misses of the last level.
// --------------------------------------------------------------------------

#include <cstddef>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "cache.hpp"
#include "2q_cache.hpp"
#include "arc_cache.hpp"
#include "lfu_cache.hpp"
#include "lirs_cache.hpp"
#include "lru_cache.hpp"

namespace caches {

// create a strategy by its name; next / storage as in the cache constructor
template <typename T, typename KeyT>
std::unique_ptr<cache<T, KeyT>> make_cache(const std::string& name, size_t size,
                                           cache<T, KeyT>* next, slow_get_page_t<T, KeyT> storage) {
    if (name == "lru")
        return std::make_unique<lru_cache<T, KeyT>>(size, storage, next);
    if (name == "lfu")
        return std::make_unique<lfu_cache<T, KeyT>>(size, storage, next);
    if (name == "arc")
        return std::make_unique<arc_cache<T, KeyT>>(size, storage, next);
    if (name == "2q")
        return std::make_unique<qq_cache<T, KeyT>>(size, storage, next);
    if (name == "lirs")
        return std::make_unique<lirs_cache<T, KeyT>>(size, storage, next);
    throw std::invalid_argument("unknown strategy: " + name);
}

// Multi-level cache:
//
//   request -> L1 --miss--> L2 --miss--> ... --miss--> Ln --miss--> storage
//
// Every level is an ordinary strategy. A level gets the level below it (or,
// for the last one, the slow storage) in its constructor; on a miss its
// slow_get_page() calls lookup_update of that level. On the way back every
// level that missed stores the page, because that is what lookup_update does.
template <typename T, typename KeyT>
class cache_hierarchy {
private:
    // types
    using cache_t = cache<T, KeyT>;

    // members
    std::vector<std::unique_ptr<cache_t>> caches_;  // [0] = L1, the fastest

public:
    // levels: {strategy, size} from L1 down to Ln
    cache_hierarchy(const std::vector<std::pair<std::string, size_t>>& levels,
                    slow_get_page_t<T, KeyT> storage);
    cache_hierarchy(const cache_hierarchy&) = delete;
    cache_hierarchy& operator=(const cache_hierarchy&) = delete;

    size_t         levels()        const {return caches_.size();}
    const cache_t& level(size_t i) const {return *caches_.at(i);}

    // the request enters L1; the bool is "hit in L1"
    std::pair<T, bool> lookup_update(const KeyT& key) {return caches_.front()->lookup_update(key);}
};

template <typename T, typename KeyT>
cache_hierarchy<T, KeyT>::cache_hierarchy(const std::vector<std::pair<std::string, size_t>>& levels,
                                          slow_get_page_t<T, KeyT> storage)
{
    if (levels.empty())
        throw std::invalid_argument("cache_hierarchy: at least one level is needed");

    // bottom-up: the last level reads from the storage,
    // every other level reads from the level right below it
    const size_t n = levels.size();
    caches_.resize(n);
    caches_[n - 1] = make_cache<T, KeyT>(levels[n - 1].first, levels[n - 1].second, nullptr, storage);
    for (size_t i = n - 1; i > 0; --i)
        caches_[i - 1] = make_cache<T, KeyT>(levels[i - 1].first, levels[i - 1].second,
                                             caches_[i].get(), nullptr);
}

}

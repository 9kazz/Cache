#pragma once

#include <cstddef>
#include <utility>
#include <stdexcept>
#include <string>

namespace caches {

// the slow storage under the last level of a hierarchy
template <typename T, typename KeyT>
using slow_get_page_t = std::pair<T, bool> (*)(const KeyT& key);

template <typename T, typename KeyT>
class cache {
private:
    cache*                   next_;         // next (slower) level, nullptr for the last one
    slow_get_page_t<T, KeyT> storage_get_;  // slow storage, nullptr if next_ is set

protected:
    size_t n_hits_   = 0;
    size_t n_misses_ = 0;

    // what a strategy calls on a miss: lookup_update of the next level,
    // or the slow storage if this is the last level
    std::pair<T, bool> slow_get_page(const KeyT& key) {
        return next_ ? next_->lookup_update(key) : storage_get_(key);
    }

public:
    // exactly one of them: the next level of a hierarchy or the slow storage
    cache(slow_get_page_t<T, KeyT> storage, cache* next = nullptr);
    virtual ~cache() = default;

    virtual size_t capacity()  const = 0; 
    virtual size_t size()      const = 0; 
    virtual std::string type() const = 0;

    size_t hits()      const {return n_hits_;}
    size_t misses()    const {return n_misses_;}
    bool   is_full()   const {return size() == capacity();}    

    virtual std::pair<T, bool> lookup_update(const KeyT& key) = 0;
};

template <typename T, typename KeyT>
cache<T, KeyT>::cache(slow_get_page_t<T, KeyT> storage, cache* next)
    : next_(next),
      storage_get_(storage)
{
    if ((next_ == nullptr) == (storage_get_ == nullptr))
        throw std::invalid_argument("cache: pass exactly one of next level / slow storage");
}

}
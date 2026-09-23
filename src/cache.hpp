#pragma once

#include <cstddef>
#include <utility>

namespace caches {

template <typename T, typename KeyT>
class cache {
protected:
    size_t n_hits_   = 0;
    size_t n_misses_ = 0;

public:
    virtual ~cache() = default;

    virtual size_t capacity() const = 0; 
    virtual size_t size()     const = 0; 

    size_t hits()     const {return n_hits_;}
    size_t misses()   const {return n_misses_;}
    bool   is_full()  const {return size() == capacity();}    

    virtual std::pair<T, bool> lookup_update(const KeyT& key, std::pair<T, bool> (*slow_get_page)(const KeyT& key)) = 0;
};

}
#pragma once

#include <cstddef>
#include <utility>
#include <iostream>
#include <iomanip>

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

    size_t hits()      const {return n_hits_;}
    size_t misses()    const {return n_misses_;}
    void   stat()      const;
    bool   is_full()   const {return size() == capacity();}    

    virtual std::pair<T, bool> lookup_update(const KeyT& key, std::pair<T, bool> (*slow_get_page)(const KeyT& key)) = 0;
};

template <typename T, typename KeyT>
void cache<T, KeyT>::stat() const
{
    const std::size_t hits   = this->hits();
    const std::size_t misses = this->misses();
    const std::size_t total  = hits + misses;

    const double hit_rate = total != 0
        ? 100.0 * hits / total
        : 0.0;

    std::cout
        << "Cache statistics\n"
        << "----------------\n"
        << "Requests : " << total  << '\n'
        << "Hits     : " << hits   << '\n'
        << "Misses   : " << misses << '\n'
        << "Hit rate : " << std::fixed << std::setprecision(2)
        << hit_rate << "%\n";
}

}
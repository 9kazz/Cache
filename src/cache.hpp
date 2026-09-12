#include <cstddef>
#include <list>
#include <unordered_map>
#include <utility>

namespace caches {

template <typename T, typename KeyT>
class Cache_t {
private:
    size_t sz_;
    size_t n_hits_ = 0;
    size_t n_misses_ = 0;

    std::list<std::pair<T, KeyT>> lru_cache_;
    std::unordered_map<KeyT, typename std::list<std::pair<T, KeyT>>::iterator> hash_;
    bool is_full() const;
public:
    explicit Cache_t(size_t sz) : sz_(sz){}
    Cache_t(const Cache_t&) = delete;
    Cache_t& operator=(const Cache_t&) = delete;
    
    size_t capacity() const {return sz_;}
    size_t size() const {return lru_cache_.size();}
    size_t hits() const {return n_hits_;}
    size_t misses() const {return n_misses_;}

    template <typename F> std::pair<T, bool> lookup_update(KeyT key, F slow_get_page);
};

template <typename T, typename KeyT>
bool Cache_t<T, KeyT>::is_full() const{
    return !(lru_cache_.size() < sz_);
}

template <typename T, typename KeyT>
template <typename F>
std::pair<T, bool> Cache_t<T, KeyT>::lookup_update(KeyT key, F slow_get_page) {
    if (sz_ == 0) {
        ++n_misses_;
        return {slow_get_page(key), false};
    }

    auto hit = hash_.find(key);
    if (hit != hash_.end()) {
        //hit 
        auto eltit = hit->second;
        lru_cache_.splice(lru_cache_.begin(), lru_cache_, eltit);
        ++n_hits_;
        return {eltit->first, true};
    } else {
        //miss
        auto page = slow_get_page(key);
        if (is_full()) {
            KeyT hkey =  lru_cache_.back().second;
            hash_.erase(hkey);
            lru_cache_.pop_back();
        }

        lru_cache_.emplace_front(page, key);
        hash_[key] = lru_cache_.begin();
        ++n_misses_;
        return {page, false};
    }
}

}
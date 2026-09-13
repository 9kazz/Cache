#include <cstddef>
#include <list>
#include <unordered_map>
#include <utility>
#include <cassert>

namespace caches {

template <typename T, typename KeyT>
class lru_cache {
private:
    // types
    struct page_t {
        T    content;
        KeyT key;
    };
    using list_t      = typename std::list<page_t>;
    using list_iter_t = typename list_t::iterator;
    using hmap_t      = typename std::unordered_map<KeyT, list_iter_t>;
    // members
    size_t cap_;
    size_t n_hits_   = 0;
    size_t n_misses_ = 0;

    list_t cache_;
    hmap_t hash_;
    // methods
    list_iter_t store(const page_t& page);
    void refresh(list_iter_t eltit);
    void rm_elem();                                                                                
public:
    explicit lru_cache(size_t capacity);
    lru_cache(const lru_cache&) = delete;
    lru_cache& operator=(const lru_cache&) = delete;
    
    size_t capacity() const {return cap_;}
    size_t size()     const {return cache_.size();}
    size_t hits()     const {return n_hits_;}
    size_t misses()   const {return n_misses_;}
    bool   is_full()  const;

    template <typename F> std::pair<T, bool> lookup_update(const KeyT& key, F slow_get_page);
};

template <typename T, typename KeyT>
lru_cache<T, KeyT>::lru_cache(size_t capacity) 
    : cap_ {capacity}
{
    assert(capacity > 0); //TODO: временное решение через ассерт -- нужно сделать нормальную обработку
}

template <typename T, typename KeyT>
bool lru_cache<T, KeyT>::is_full() const {
    assert(cache_.size() <= cap_);
    return cache_.size() == cap_;
}

template <typename T, typename KeyT>
template <typename F>
std::pair<T, bool> lru_cache<T, KeyT>::lookup_update(const KeyT& key, F slow_get_page) {
    auto hit = hash_.find(key);
    if (hit != hash_.end()) {
        // cache hit 
        auto eltit = hit->second;
        refresh(eltit);
        ++n_hits_;
        return {eltit->content, true}; 
    }
    // cache miss
    page_t page {
        .content = slow_get_page(key), 
        .key     = key
    };
    store(page);
    ++n_misses_;
    return {page.content, false};
}

template <typename T, typename KeyT>
typename lru_cache<T, KeyT>::list_iter_t lru_cache<T, KeyT>::store(const page_t& page) {
    if (is_full()) {
        rm_elem();
    }
    cache_.push_front(page);
    auto new_page = cache_.begin();
    hash_[page.key] = new_page;
    return new_page;
}

template <typename T, typename KeyT>
void lru_cache<T, KeyT>::refresh(list_iter_t eltit) {
    cache_.splice(cache_.begin(), cache_, eltit);
}

template <typename T, typename KeyT>
void lru_cache<T, KeyT>::rm_elem() {
    assert(size() == cap_);
    const auto& hkey = cache_.back().key;
    hash_.erase(hkey);
    cache_.pop_back();
}
}
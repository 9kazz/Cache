#pragma once

// TODO (for Matvey) --------------------------------------------------------
// [small]  `typename std::list<page_t>` and `typename std::unordered_map<...>`:
//          typename is not needed there, only before list_t::iterator.
//          Figure out the rule (dependent qualified names).
// [small]  Extra `;` after the constructor body `{};`.
// [small]  size_t without std:: compiles only because <cstddef> usually also
//          puts it into the global namespace. std::size_t is the portable one.
// [small]  hash_[page.key] = ... in store(): operator[] default-constructs the
//          value and then assigns. Compare with emplace/insert_or_assign.
// [medium] rm_elem() keeps `const auto& hkey` - a reference into the list node.
//          It is fine only because hash_.erase() runs before pop_back().
//          What exactly breaks if you swap these two lines?
// [medium] The page is copied: slow_get_page -> page -> list node -> return.
//          Try T = std::string and think where std::move / emplace_front help.
// [medium] Exception safety: slow_get_page() is called before store(), so if
//          it throws the cache is untouched. Keep this order when refactoring.
// [big]    lookup_update returns std::pair<T, bool> by value, i.e. a copy of
//          the page on every call. What would change if it returned a
//          reference or a pointer (hint: iterator/reference invalidation)?
// [big]    Google tests: capacity 0 and 1, repeated key, eviction order,
//          hits()/misses() counters, size() never exceeds capacity().
// --------------------------------------------------------------------------

#include <cstddef>
#include <list>
#include <unordered_map>
#include <utility>
#include <cassert>

namespace caches {

// LRU (Least Recently Used) cache.
//
// cache_ is a list ordered by recency: [MRU ... LRU].
// hash_ maps a key to its node in cache_, so lookup is O(1).
// Hit  -> the node is spliced to the front (MRU).
// Miss -> if full, the back (LRU) node is dropped; the new page goes to front.
//
// Invariants:
//   cache_.size() == hash_.size() <= cap_
//   hash_[key] points to the node with this key
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
    void store(const page_t& page);
    void refresh(list_iter_t eltit);
    void rm_elem();                                                                                
public:
    explicit lru_cache(size_t capacity) : cap_(capacity) {};
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
bool lru_cache<T, KeyT>::is_full() const {
    assert(cache_.size() <= cap_);
    return cache_.size() == cap_;
}

template <typename T, typename KeyT>
template <typename F>
std::pair<T, bool> lru_cache<T, KeyT>::lookup_update(const KeyT& key, F slow_get_page) {
    if (cap_ == 0) { // temporary special case, keep it: with cap_ == 0
                     // rm_elem() would pop_back() an empty list
        ++n_misses_;
        return {slow_get_page(key), false};
    }

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

// miss: drop the LRU page if needed, put the new one to the MRU end
template <typename T, typename KeyT>
void lru_cache<T, KeyT>::store(const page_t& page) {
    if (is_full()) {
        rm_elem();
    }
    cache_.push_front(page);
    hash_[page.key] = cache_.begin();
}

// hit: move the node to the MRU end (splice keeps the iterator valid)
template <typename T, typename KeyT>
void lru_cache<T, KeyT>::refresh(list_iter_t eltit) {
    cache_.splice(cache_.begin(), cache_, eltit);
}

// drop the LRU page (the back of the list)
template <typename T, typename KeyT>
void lru_cache<T, KeyT>::rm_elem() {
    assert(size() == cap_);
    const auto& hkey = cache_.back().key;
    hash_.erase(hkey);
    cache_.pop_back();
}
}
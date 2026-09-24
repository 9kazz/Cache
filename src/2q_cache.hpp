#pragma once

#include <cstddef>
#include <list>
#include <iterator>
#include <unordered_map>
#include <utility>
#include <cassert>
#include <cstdint>
#include <cmath>
#include <vector>
#include <stdexcept>
#include <string>

#include "cache.hpp"

namespace caches {

template <typename T, typename KeyT>
class qq_cache : public cache<T, KeyT> {
private:
    static constexpr size_t default_a1in_divisor  = 4;
    static constexpr size_t default_a1out_divisor = 2;

    static size_t validate_default_capacity(size_t cap) {
        constexpr size_t min_capacity = default_a1in_divisor > default_a1out_divisor
            ? default_a1in_divisor : default_a1out_divisor;
            
        if (cap < min_capacity) {
            throw std::invalid_argument(
                "qq_cache(cap): minimum cache size: " + std::to_string(min_capacity) +
                " pages; use the full constructor for more detailed configuration");
        }
        return cap;
    }

    // types
    struct page_t {
        T    content;
        KeyT key;
    };
    using a1in_t  = typename std::list<KeyT>::iterator;
    using a1out_t = typename std::list<KeyT>::iterator;
    using am_t    = typename std::list<KeyT>::iterator;
    using cache_iter_t = typename std::vector<page_t>::iterator;

    enum state_t {
        A1IN  = 1, // first access to the page
        A1OUT = 2, // access history (metadata)
        AM    = 3  // second access to the page
    };
    struct meta_data_t {
        KeyT    key;
        state_t state;
        size_t  idx;  // idx in cache: valid only when state != A1OUT
        a1in_t  iter; // iter in corresponding queque
    };
    // members
    size_t a1in_cap_; // Kin: eviction threshold, not a hard queue limit
    size_t a1out_cap_;
    size_t am_cap_;

    std::unordered_map<KeyT, meta_data_t> hash_;
    std::list<KeyT> a1in_;
    std::list<KeyT> a1out_;
    std::list<KeyT> am_;
    std::vector<page_t> cache_;
    // methods
    void   store_to_a1in  (const page_t& page, size_t free_idx);
    void   store_to_am    (const page_t& page, size_t free_idx);
    void   store_to_cache (const page_t& page, size_t free_idx);
    void   store_to_a1out (const KeyT& key);
    size_t evict_from_a1in();
    size_t evict_from_cache_if_need();
    void   evict_from_a1out_if_need();
    size_t evict_from_am();

public:
    // cap must be >= both default divisors so neither queue size rounds to zero.
    explicit qq_cache(size_t cap);
    explicit qq_cache(size_t a1in_cap, size_t am_cap, size_t a1out_cap);
    ~qq_cache() override = default;
    qq_cache(const qq_cache&) = delete;
    qq_cache& operator=(const qq_cache&) = delete;

    size_t capacity() const override {return a1in_cap_ + am_cap_;}
    size_t size()     const override {return cache_.size();}

    std::pair<T, bool> lookup_update(const KeyT& key, std::pair<T, bool> (*slow_get_page)(const KeyT& key)) override;
};

template <typename T, typename KeyT>
qq_cache<T, KeyT>::qq_cache(size_t cap)
    : qq_cache(validate_default_capacity(cap) / default_a1in_divisor,
               cap - cap / default_a1in_divisor,
               cap / default_a1out_divisor)
{}

template <typename T, typename KeyT>
qq_cache<T, KeyT>::qq_cache(size_t a1in_cap, size_t am_cap, size_t a1out_cap)
    : a1in_cap_  {a1in_cap},
      a1out_cap_ {a1out_cap},
      am_cap_    {am_cap}
{
    if (a1in_cap_ == 0 || am_cap_ == 0) {
        throw std::invalid_argument("qq_cache(a1in_cap, am_cap, a1out_cap): a1in_cap and am_cap must both be > 0; physical capacity = a1in_cap + am_cap (at least 2 pages)");
    }
    if (a1out_cap_ == 0) {
        throw std::invalid_argument("qq_cache(a1in_cap, am_cap, a1out_cap): a1out_cap must be > 0; it limits history entries, not resident pages");
    }
    cache_.reserve(capacity());
}

template <typename T, typename KeyT>
std::pair<T, bool> qq_cache<T, KeyT>::lookup_update(const KeyT& key, std::pair<T, bool> (*slow_get_page)(const KeyT& key)) {
    auto page_it = hash_.find(key);

    if (page_it == hash_.end()) {
        page_t new_page{slow_get_page(key).first, key};

        const auto free_idx = evict_from_cache_if_need();
        meta_data_t new_meta {
            .key   = key,
            .state = A1IN,
            .idx   = free_idx,
            .iter  = a1in_.end()
        };
        hash_.emplace(key, new_meta);
        store_to_a1in(new_page, free_idx);
        cache<T, KeyT>::n_misses_++;
        return {new_page.content, false};
    }

    auto& meta = page_it->second;
    switch (meta.state) {    
    case AM:
        am_.splice(am_.begin(), am_, meta.iter);    
        [[fallthrough]];
    case A1IN:
        cache<T, KeyT>::n_hits_++;
        return {cache_[meta.idx].content, true};

    case A1OUT: {
        page_t new_page{slow_get_page(key).first, key};
        // Reclaim may add a ghost and trim A1out. Remove this key first
        // so its metadata survives and no other ghost is needlessly lost.
        a1out_.erase(meta.iter);
        const auto free_idx = evict_from_cache_if_need();
        store_to_am(new_page, free_idx);
        cache<T, KeyT>::n_misses_++;
        return {new_page.content, false};
    }
    
    default:
        throw std::logic_error("invalid 2Q page state");
    }
}

template <typename T, typename KeyT>
void qq_cache<T, KeyT>::store_to_a1in(const page_t& page, size_t free_idx) {
    assert(a1in_.size() <= capacity());
    auto& meta = hash_.find(page.key)->second;

    a1in_.push_front(page.key);
    meta.state = A1IN;
    meta.iter  = a1in_.begin();

    store_to_cache(page, free_idx);
}

template <typename T, typename KeyT>
void qq_cache<T, KeyT>::store_to_a1out(const KeyT& key) {
    assert(a1out_.size() < a1out_cap_);
    auto& meta = hash_.find(key)->second;

    a1out_.push_front(key);
    meta.state = A1OUT;
    meta.iter  = a1out_.begin();
}

template <typename T, typename KeyT>
void qq_cache<T, KeyT>::store_to_am(const page_t& page, size_t free_idx) {
    assert(am_.size() < am_cap_);
    auto& meta = hash_.find(page.key)->second;

    am_.push_front(page.key);
    meta.state = AM;
    meta.iter  = am_.begin();

    store_to_cache(page, free_idx);
}

template <typename T, typename KeyT>
void qq_cache<T, KeyT>::store_to_cache(const page_t& page, size_t free_idx) {
    assert(free_idx <= size());
    assert(free_idx < capacity());
    auto& meta = hash_.find(page.key)->second;

    if (free_idx == size()) {
        cache_.push_back(page); // cold cache
    } else {
        cache_[free_idx] = page; // hot cache
    }
    meta.idx = free_idx;
}

// return first free idx in cache
template <typename T, typename KeyT>
size_t qq_cache<T, KeyT>::evict_from_cache_if_need() {
    assert(size() <= capacity());

    if (!this->is_full()) {
        return cache_.size();
    }
    if (a1in_.size() > a1in_cap_) {
        return evict_from_a1in();
    }
    return evict_from_am();
}

// return first free idx in cache
template <typename T, typename KeyT>
size_t qq_cache<T, KeyT>::evict_from_a1in() {
    assert(!a1in_.empty());
    assert(a1in_.size() <= capacity());

    auto  it   = hash_.find(a1in_.back());
    assert(it != hash_.end());
    auto& meta = it->second;
    const auto free_idx = meta.idx;

    a1in_.pop_back();
    evict_from_a1out_if_need();
    store_to_a1out(it->first);
    return free_idx;
}

template <typename T, typename KeyT>
void qq_cache<T, KeyT>::evict_from_a1out_if_need() {
    assert(a1out_.size() <= a1out_cap_);
    if (a1out_.size() == a1out_cap_) {
        hash_.erase(a1out_.back());
        a1out_.pop_back();
    }
}

// return first free idx in cache
template <typename T, typename KeyT>
size_t qq_cache<T, KeyT>::evict_from_am() {
    assert(!am_.empty());
    assert(am_.size() <= am_cap_);    
    auto it    = hash_.find(am_.back());
    assert(it != hash_.end());
    auto& meta = it->second;
    const auto free_idx = meta.idx;

    hash_.erase(it);
    am_.pop_back();
    return free_idx;
}

}

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

namespace caches {

template <typename T, typename KeyT>
class lirs_cache {
private:
    // types
    struct page_t {
        T    content;
        KeyT key;
    };
    using s_iter_t     = typename std::list<KeyT>::iterator;
    using q_iter_t     = typename std::list<KeyT>::iterator;
    using cache_iter_t = typename std::vector<page_t>::iterator;

    enum state_t {
        LIR   = 1, // low inter-reference recency
        HIR_R = 2, // hight, resident
        HIR_N = 3  // hight, non-resident
    };
    struct meta_data_t {
        KeyT    key;
        state_t state;
        size_t  idx; // valid only when state != HIR_N
        bool in_s = false;
        bool in_q = false;
        // valid only when corresponding bool == true
        s_iter_t s_iter {};
        q_iter_t q_iter {};
    };
    // members
    size_t cap_;
    size_t s_cap_; // maximum number of LIR and HIR entries in S
    size_t q_cap_;

    std::unordered_map<KeyT, meta_data_t> hash_;
    std::list<KeyT> s_;
    std::list<KeyT> q_;
    std::vector<page_t> cache_;
    // for statistic
    size_t n_hits_   = 0;
    size_t n_misses_ = 0;
    // methods
    void   s_pruning();
    void   store_to_cache(const page_t& page, size_t idx);
    void   store_to_s    (const KeyT& key);
    void   store_to_q    (const KeyT& key);
    size_t evict_from_q_if_need();
    void   evict_from_s_if_need();

public:
    explicit lirs_cache(size_t cap, size_t q_cap, size_t s_cap);
    ~lirs_cache() = default;
    lirs_cache(const lirs_cache&) = delete;
    lirs_cache& operator=(const lirs_cache&) = delete;

    size_t capacity() const {return cap_;}
    size_t hits()     const {return n_hits_;}
    size_t misses()   const {return n_misses_;}
    size_t size()     const {return cache_.size();}
    bool   is_full()  const {return size() == capacity();}

    template <typename F> std::pair<T, bool> lookup_update(const KeyT& key, F slow_get_page);
};

template <typename T, typename KeyT>
lirs_cache<T, KeyT>::lirs_cache(size_t cap, size_t q_cap, size_t s_cap)
    : cap_   {cap},
      s_cap_ {s_cap},
      q_cap_ {q_cap}
{
    if (cap_ == 0 || q_cap_ == 0 || q_cap_ >= cap) {
        throw std::invalid_argument("Incorrect capacity of cache or Q stack");
    }
    if (s_cap_ == 0 || s_cap_ <= cap_) {
        throw std::invalid_argument("S stack capacity must be more than cache capacity");
    }
    cache_.reserve(cap);
}

template <typename T, typename KeyT>
template <typename F>
std::pair<T, bool> lirs_cache<T, KeyT>::lookup_update(const KeyT& key, F slow_get_page) {
    auto page_iter = hash_.find(key);

    if (page_iter == hash_.end()) {
        page_t new_page{slow_get_page(key), key};

        const bool make_lir = cache_.size() - q_.size() < cap_ - q_cap_;
        const auto free_idx = evict_from_q_if_need();

        meta_data_t new_meta {
            .key   = key,
            .state = make_lir ? LIR : HIR_R,
            .idx   = free_idx
        };
        hash_.emplace(key, new_meta);
        if (!make_lir) {
            store_to_q(key);
        }
        store_to_s(key);
        evict_from_s_if_need();
        store_to_cache(new_page, free_idx);
        s_pruning();
        ++n_misses_;
        return {cache_[free_idx].content, false};
    }

    auto& meta_data = page_iter->second;
    switch (meta_data.state)
    {
    case LIR:
        s_.splice(s_.begin(), s_, meta_data.s_iter);
        s_pruning();
        ++n_hits_;
        return {cache_[meta_data.idx].content, true};

    case HIR_R:
        if (meta_data.in_s) {
            meta_data.state = LIR;
            meta_data.in_q  = false;
            q_.erase(meta_data.q_iter);
            s_.splice(s_.begin(), s_, meta_data.s_iter);

            auto& old_lir = hash_.find(s_.back())->second;
            old_lir.state = HIR_R;
            store_to_q(old_lir.key);
        } else {
            store_to_s(key);
            evict_from_s_if_need();
            q_.splice(q_.begin(), q_, meta_data.q_iter);
        }
        s_pruning();
        ++n_hits_;
        return {cache_[meta_data.idx].content, true};

    case HIR_N: {
        page_t new_page{slow_get_page(key), key};

        s_.splice(s_.begin(), s_, meta_data.s_iter);
        const auto free_idx = evict_from_q_if_need();

        meta_data.state = LIR;
        auto& old_lir   = hash_.find(s_.back())->second;
        old_lir.state   = HIR_R;
        store_to_q(old_lir.key);
        store_to_cache(new_page, free_idx);

        s_pruning();
        ++n_misses_;
        return {cache_[free_idx].content, false};
    }
    default:
        throw std::logic_error("invalid LIRS page state");
    }
}

template <typename T, typename KeyT>
void lirs_cache<T, KeyT>::s_pruning() {
    while (!s_.empty()) {
        auto  it   = hash_.find(s_.back());
        auto& meta = it->second;
        if (meta.state == LIR) {
            break;
        }
        s_.pop_back();
        meta.in_s = false;
        if (meta.state == HIR_N) {
            hash_.erase(it);
        }
    }
}

template <typename T, typename KeyT>
void lirs_cache<T, KeyT>::store_to_cache(const page_t& page, size_t idx) {
    assert(idx <= cache_.size());

    if (idx == cache_.size()) {
        cache_.push_back(page); // cold cache
    } else {
        cache_[idx] = page; // hot cache
    }
    hash_.find(page.key)->second.idx = idx;
}

template <typename T, typename KeyT>
void lirs_cache<T, KeyT>::store_to_s(const KeyT& key) {
    auto& meta = hash_.find(key)->second;

    s_.push_front(key);
    meta.in_s   = true;
    meta.s_iter = s_.begin();
}

template <typename T, typename KeyT>
void lirs_cache<T, KeyT>::store_to_q(const KeyT& key) {
    auto& meta = hash_.find(key)->second;

    q_.push_front(key);
    meta.in_q   = true;
    meta.q_iter = q_.begin();
    if (meta.state == HIR_N) {
        meta.state = HIR_R;
    }
}

template <typename T, typename KeyT>
void lirs_cache<T, KeyT>::evict_from_s_if_need() {
    if (s_.size() <= s_cap_) {
        return;
    }
    auto s_it = std::prev(s_.end());

    while (true) {
        auto& meta = hash_.at(*s_it);
        if (meta.state != LIR) {
            break;
        }
        assert(s_it != s_.begin());
        --s_it;
    }
    auto hash_it = hash_.find(*s_it);
    auto& meta = hash_it->second;
    s_.erase(s_it);
    meta.in_s = false;

    if (meta.state == HIR_N) {
        hash_.erase(hash_it);
    }
}

// return first free idx in cache
template <typename T, typename KeyT>
size_t lirs_cache<T, KeyT>::evict_from_q_if_need() {
    if (!is_full()) {
        return cache_.size();
    }
    auto  it   = hash_.find(q_.back());
    auto& meta = it->second;
    const auto free_idx = meta.idx;

    q_.pop_back();
    if (!meta.in_s) {
        hash_.erase(it);
    } else {
        meta.in_q  = false;
        meta.state = HIR_N;
    }
    return free_idx;
}

}

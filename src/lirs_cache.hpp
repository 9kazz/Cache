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

namespace caches {

template <typename T, typename KeyT>
class lirs_cache {
private:
// types
struct page_t {
    T    content;
    KeyT key;
};
using s_iter_t     = std::list<KeyT>::iterator;
using q_iter_t     = std::list<KeyT>::iterator;
using cache_iter_t = std::vector<page_t>::iterator;

static constexpr double s_cap_multr_default = 2;   // s has s_cap_multr_default * cap_ capacity
static constexpr double q_cap_multr_default = 0.1; // q has s_cap_multr_default * s_cap_ capacity

enum state_t {
    LIR   = 1, // low inter-reference recency
    HIR_R = 2, // hight, resident
    HIR_N = 3  // hight, non-resident
};
struct meta_data_t {
    KeyT    key;
    state_t state;
    size_t  idx; // valid only when state != HIR_N
    bool in_s;
    bool in_q;
    // valid only when corresponding bool == true 
    s_iter_t s_iter;
    q_iter_t q_iter;
};
    // members
    size_t cap_;
    size_t s_cap_; // s can be bigger than the real cache 
    size_t q_cap_;

    std::unordered_map<KeyT, meta_data_t> hash_;
    std::list<KeyT> s_;
    std::list<KeyT> q_;
    std::vector<page_t> cache_;
    // for statistic
    size_t n_hits_   = 0;
    size_t n_misses_ = 0;
    // methods
    void s_pruning();
    void store_to_cache(const page_t& page);
    void store_to_cache(const page_t& page, size_t idx);
    void store_to_s    (const KeyT& key);
    void store_to_q    (const KeyT& key);
public:
    explicit lirs_cache(size_t cap);
    explicit lirs_cache(size_t cap, size_t s_cap);
    explicit lirs_cache(size_t cap, size_t s_cap, size_t q_cap);
    ~lirs_cache() = default;
    lirs_cache(const lirs_cache&) = delete;
    lirs_cache& operator=(const lirs_cache&) = delete;
    
    size_t capacity() const {return cap_;}
    size_t hits()     const {return n_hits_;}
    size_t misses()   const {return n_misses_;}
    size_t size()     const {return cache_.size();}
    bool   is_full()  const;

    template <typename F> std::pair<T, bool> lookup_update(const KeyT& key, F slow_get_page);
};    

template <typename T, typename KeyT>
template <typename F>
std::pair<T, bool> lirs_cache<T, KeyT>::lookup_update(const KeyT& key, F slow_get_page) {
    auto  page_iter = hash_.find(key);
    if (page_iter == hash_.end()) { 
        // cache miss: new page
        meta_data_t new_page {  // new resident HIR
            .key   = key,
            .state = HIR_R
        };
        hash_[key] = meta_data_t;
        store_to_s(key);
        store_to_q(key);
        store_to_cache(page_t{slow_get_page(key), key});
        n_misses_++;
        return {cache_[new_page.idx].content, false};
    }

    auto& meta_data = page_iter->second;
    auto& page      = cache_[meta_data.idx];
    switch (meta_data.state)
    {
    case LIR:
        // cache hit
        s_.splice(s_.begin(), s_, meta_data.s_iter);
        s_pruning();
        n_hits_++;
        return {page.content, true};

    case HIR_R:
        // cache hit
        if (meta_data.in_s) { 
            // resident HIR in s
            meta_data.state = LIR; // becomes new LIR
            meta_data.in_s  = true;
            meta_data.in_q  = false;
            q_.erase(meta_data.q_iter);

            hash_.find(s_.back())->second.state = HIR_R; // old LIR becomes new HIR_R
            store_to_q(hash_.find(s_.back())->second.key);
            s_pruning();

        } else {
            // resident HIR out of LIR
            store_to_s(key);
            q_.splice(q_.begin(), q_, meta_data.q_iter);
        }
        n_hits_++;
        return {page.content, true};

    case HIR_N: {
        // cache miss
        // remove LRU page from q and from cache
        auto free_idx = hash_.find(q_.back())->second.idx; 
        q_.erase(std::prev(q_.end()));

        // LRU page from s becomes new HIR_R
        hash_.find(s_.back())->second.state = HIR_R;
        store_to_q(hash_.find(s_.back())->second.key);

        meta_data_t new_page { // new LIR
            .key   = key,
            .state = LIR,
            .in_q  = false
        };
        hash_[key] = meta_data_t;
        store_to_s(key);
        store_to_cache(page_t{slow_get_page(key), key}, free_idx);

        s_pruning();
        n_misses_++;
        return {cache_[new_page.idx].content, false};

    }
    default: 
        assert(0); //TODO: временное решение через ассерт -- нужно сделать нормальную обработку
        return {0, false};
    }
}

template <typename T, typename KeyT>
void lirs_cache<T, KeyT>::s_pruning() {
    while (hash_.find(s_.back())->second.state != LIR) { // check if last elem in s_ has not state LIR
        hash_.find(s_.back())->second.in_s = false;
        s_.erase(std::prev(s_.end()));
    }
}

template <typename T, typename KeyT>
void lirs_cache<T, KeyT>::store_to_cache(const page_t& page) {
    if (is_full()) {
        cache_.pop_back();        
    }
    cache_.push_back(page);
    hash_.find(page.key)->second.idx = std::prev(cache_.end()) - cache_.begin();
}

template <typename T, typename KeyT>
void lirs_cache<T, KeyT>::store_to_cache(const page_t& page, size_t idx) {
    cache_[idx] = page;
    hash_.find(page.key)->second.idx = idx;
}

template <typename T, typename KeyT>
void lirs_cache<T, KeyT>::store_to_s(const KeyT& key) {
    if (s_.size() == s_cap_) {
        hash_.find(s_.back())->second.in_s = false;
        s_.pop_back();
    }
    s_.push_front(key);
    hash_.find(key)->second.in_s   = true;
    hash_.find(key)->second.s_iter = s_.begin();
}

template <typename T, typename KeyT>
void lirs_cache<T, KeyT>::store_to_q(const KeyT& key) {
    if (q_.size() == q_cap_) {
        hash_.find(q_.back())->second.in_q = false;
        q_.pop_back();
    }
    q_.push_front(key);
    hash_.find(key)->second.in_q = true;
    hash_.find(key)->second.q_iter = q_.begin();
}

template <typename T, typename KeyT>
lirs_cache<T, KeyT>::lirs_cache(size_t cap) 
    : cap_   {cap},
      s_cap_ {0},
      q_cap_ {0},
      cache_.reserve(cap)
{   
    assert(cap > 0); //TODO: временное решение через ассерт -- нужно сделать нормальную обработку
    s_cap_ = std::ceil(s_cap_multr_default * cap);
    q_cap_ = std::ceil(q_cap_multr_default * s_cap_);
}

template <typename T, typename KeyT>
lirs_cache<T, KeyT>::lirs_cache(size_t cap, size_t s_cap) 
    : cap_   {cap},
      s_cap_ {s_cap},
      q_cap_ {0},
      cache_.reserve(cap)
{   
    assert(cap > 0); //TODO: временное решение через ассерт -- нужно сделать нормальную обработку
    assert(s_cap > 0);
    q_cap_ = (q_cap_multr_default * s_cap_);
}

template <typename T, typename KeyT>
lirs_cache<T, KeyT>::lirs_cache(size_t cap, size_t s_cap, size_t q_cap) 
    : cap_   {cap},
      s_cap_ {s_cap},
      q_cap_ {q_cap},
      cache_.reserve(cap)
{   
    assert(cap > 0); //TODO: временное решение через ассерт -- нужно сделать нормальную обработку
    assert(q_cap > 0);
}

template <typename T, typename KeyT>
bool lirs_cache<T, KeyT>::is_full() const {
    assert(size() <= cap_);
    return size() == cap_;
}

}

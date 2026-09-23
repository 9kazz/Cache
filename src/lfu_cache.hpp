#pragma once

// TODO-----------------------------------------------------------------
// [small]  promote()/evict() use freq_buckets_[f] (operator[]). If the
//          invariant is ever broken it silently creates an empty bucket
//          instead of failing. Compare with find() + assert.
// [small]  hash_[key] = ... in store(): operator[] default-constructs a
//          cache_elem_t and then assigns. Look at emplace/insert_or_assign.
// [medium] promote() holds references old_bucket/new_bucket into an
//          unordered_map, and the second operator[] may trigger a rehash.
//          Find in the standard why references survive a rehash while
//          iterators do not. Would the code still be correct with iterators?
// [medium] Prove to yourself why min_freq_ stays correct: why is ++min_freq_
//          enough in promote(), and why min_freq_ = 1 is always right in store().
// [medium] Order in lookup_update(): slow_get_page() is called BEFORE evict().
//          If slow_get_page() throws, the cache is left untouched. What happens
//          if you swap them? (exception safety, will be in the lectures)
// [medium] T is copied several times per miss (page -> list -> return value).
//          Try it with T = std::string and think where std::move would help.
// [big]    Frequencies never decay: a page that was hot long ago stays forever,
//          a new hot page (freq 1) is evicted first. This is exactly what ARC
//          fixes - compare both on a workload that changes its hot set.
// [big]    Google tests: capacity 0 and 1; all keys with equal freq (must
//          behave like LRU); tie-break inside one frequency; min_freq_ after
//          the last page of the min bucket is promoted.
// [medium] n_hits_/n_misses_ now live in the base class cache<T, KeyT>, so
//          they are written as cache<T, KeyT>::n_hits_, and is_full() as
//          this->is_full(). Why does a plain n_hits_ not compile inside a
//          class template? (dependent base class, two-phase name lookup)
// [small]  slow_get_page returns std::pair<T, bool> (base class interface),
//          only .first is used. Discuss with the team what .second means.
// --------------------------------------------------------------------------

#include <cassert>
#include <cstddef>
#include <list>
#include <unordered_map>
#include <utility>

#include "cache.hpp"

namespace caches {

// LFU (Least Frequently Used) cache.
//
// Every cached page has an access counter freq. Pages with the same freq
// live in one LRU list ("bucket"), buckets are indexed by freq:
//
//   freq_buckets_[1] : [MRU ... LRU]
//   freq_buckets_[2] : [MRU ... LRU]
//   ...
//
// Victim = LRU page of the bucket with the minimal freq (min_freq_).
//
// Invariants:
//   hash_.size() <= cap_
//   every key in hash_ lives in freq_buckets_[hash_[key].freq]
//   there are no empty buckets in freq_buckets_
//   if the cache is not empty, freq_buckets_[min_freq_] exists
template <typename T, typename KeyT>
class lfu_cache : public cache<T, KeyT> {
private:
    // types
    struct page_t {
        T    content;
        KeyT key;
    };

    using list_t      = std::list<page_t>;
    using list_iter_t = typename list_t::iterator;

    // where the page is: iterator into its bucket + the bucket's freq
    struct cache_elem_t {
        list_iter_t it;
        size_t      freq;
    };

    using bucket_map_t = std::unordered_map<size_t, list_t>;
    using hmap_t       = std::unordered_map<KeyT, cache_elem_t>;

    // members
    size_t cap_;
    size_t min_freq_ = 0;

    bucket_map_t freq_buckets_;  // freq -> pages with this freq, [MRU ... LRU]
    hmap_t       hash_;          // key  -> where the page is

    // methods
    void store(const page_t& page);
    void promote(cache_elem_t& elem);
    void evict();

public:
    explicit lfu_cache(size_t capacity) : cap_(capacity) {}
    ~lfu_cache() override = default;
    lfu_cache(const lfu_cache&) = delete;
    lfu_cache& operator=(const lfu_cache&) = delete;

    size_t capacity() const override {return cap_;}
    size_t size()     const override {return hash_.size();}

    std::pair<T, bool> lookup_update(const KeyT& key, std::pair<T, bool> (*slow_get_page)(const KeyT& key)) override;
};

template <typename T, typename KeyT>
std::pair<T, bool> lfu_cache<T, KeyT>::lookup_update(const KeyT& key, std::pair<T, bool> (*slow_get_page)(const KeyT& key)) {
    // nothing can be stored: every request is a miss
    if (cap_ == 0) {
        cache<T, KeyT>::n_misses_++;
        return {slow_get_page(key).first, false};
    }

    auto hit = hash_.find(key);
    if (hit != hash_.end()) {
        // cache hit: freq -> freq + 1
        cache_elem_t& elem = hit->second;
        promote(elem);

        cache<T, KeyT>::n_hits_++;
        return {elem.it->content, true};
    }

    // cache miss: load the page first, then make room and store it
    page_t page {
        .content = slow_get_page(key).first,
        .key     = key
    };
    if (this->is_full()) {
        evict();
    }
    store(page);

    cache<T, KeyT>::n_misses_++;
    return {page.content, false};
}

// new page: freq = 1, goes to the MRU end of bucket 1
template <typename T, typename KeyT>
void lfu_cache<T, KeyT>::store(const page_t& page) {
    list_t& bucket = freq_buckets_[1];
    bucket.push_front(page);
    hash_[page.key] = cache_elem_t{bucket.begin(), 1};

    min_freq_ = 1;  // a page with freq 1 exists now, nothing can be lower
}

// hit: move the page from bucket freq to the MRU end of bucket freq + 1
// (splice relinks the node, elem.it stays valid)
template <typename T, typename KeyT>
void lfu_cache<T, KeyT>::promote(cache_elem_t& elem) {
    const size_t old_freq = elem.freq;

    list_t& old_bucket = freq_buckets_[old_freq];
    list_t& new_bucket = freq_buckets_[old_freq + 1];

    new_bucket.splice(new_bucket.begin(), old_bucket, elem.it);
    elem.freq = old_freq + 1;

    if (old_bucket.empty()) {
        // the page just moved to old_freq + 1, so that bucket is the new minimum
        if (min_freq_ == old_freq)
            ++min_freq_;
        freq_buckets_.erase(old_freq);
    }
}

// full cache: drop the LRU page of the least frequent bucket
template <typename T, typename KeyT>
void lfu_cache<T, KeyT>::evict() {
    assert(!hash_.empty());

    list_t& victims = freq_buckets_[min_freq_];
    assert(!victims.empty());

    // erase from hash_ first: the key lives inside the list node
    hash_.erase(victims.back().key);
    victims.pop_back();

    if (victims.empty())
        freq_buckets_.erase(min_freq_);
    // min_freq_ may be stale now, but store() sets it to 1 right after
}

}

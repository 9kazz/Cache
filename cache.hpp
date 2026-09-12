#pragma once

#include <cstddef>
#include <cstdint>
#include <list>
#include <unordered_map>
#include <cassert>

class CacheLRU {
private:
    using listIter = std::list <objInfo>::iterator;
    using hashIter = std::unordered_map <std::uint64_t, listIter>::iterator;

    std::size_t cap_;
    std::list <objInfo> cache_;
    std::unordered_map <std::uint64_t, listIter> hash_;
    
    // statistic
    std::uint64_t nMiss_;
    std::uint64_t nHit_;

    // methods
    void refresh(listIter objIt);
    void rmElem ();

public:    
    explicit CacheLRU(std::size_t cap);
    ~CacheLRU() = default;
    CacheLRU(const CacheLRU& other) = delete;
    CacheLRU& operator=(const CacheLRU& other) = delete;

    std::uint64_t hit() const;
    std::uint64_t miss() const;
    bool isFull() const;

    listIter store (const objInfo& obj);
    objInfo* lookup(std::uint64_t key);
};

#include "cache.hpp"

CacheLRU::CacheLRU(std::size_t cap) 
    : cap_  {cap},
      nMiss_{0},
      nHit_ {0}
{    
}

// try to find obj by a key. return nullptr if obj is not found
objInfo* CacheLRU::lookup(std::uint64_t key) {
    if (cap_ == 0) {
        retunr nullptr;
    }
    hashIter wantedPair = hash_.find(key);
    if (wantedPair != hash_.end()) {
        // obj is cached
        nHit_++;
        refresh(wantedPair->second); 
        return &(*wantedPair->second);
    }
    // obj is not cached
    nMiss_++;
    return nullptr;
    // slowGetPage(key);
}

// store obj into the start of list and {key, objIter} pair in hashmap 
std::list<objInfo>::iterator CacheLRU::store(const objInfo& obj) {    
    if (isFull()) {
        rmElem();
    }
    cache_.push_front(obj);
    listIter newObj = cache_.begin();
    hash_[obj.key] = newObj;

    return newObj;
}

// replace obj into the start of list
void CacheLRU::refresh(listIter objIt) {
    cache_.splice(cache_.begin(), cache_, objIt);
}

// remove cached elem chosen by cache's algorithm
void CacheLRU::rmElem() {
    assert(isFull());

    auto lastObj = std::prev(cache_.end());
    hash_.erase(lastObj->key);
    cache_.erase(lastObj);
}

std::uint64_t CacheLRU::hit() const {
    return nHit_;
}

std::uint64_t CacheLRU::miss() const {
    return nMiss_;
}

bool CacheLRU::isFull() const {
    assert(cache_.size() <= cap_);
    return cache_.size() == cap_;
}
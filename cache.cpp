#include <cstddef>
#include <cassert>
#include <list>
#include <unordered_map>
#include <string>

#include <iostream>

class CacheLRU {
    using str      = std::string;
    using listIter = std::list<std::string>::iterator;
    using hashIter = std::unordered_map<int, listIter>::iterator;

    std::size_t cap_;
    std::list<std::string> cache_;
    std::unordered_map<int, listIter> hash_;
    bool isFull;
    
    // statistic
    unsigned long nMiss_;
    unsigned long nHit_;

    // methods
    listIter store  (int key, str obj);
    void     refresh(listIter obj);
    void     rmElem ();

public:    
    str lookupUpdate(int key, str obj);
};

std::string CacheLRU::lookupUpdate(int key, str obj) {
    hashIter wantedPair = hash_.find(key);

    if (wantedPair != hash_.end()) {
        // obj is cached
        nHit_++;
        refresh(wantedPair->second); 
        return *(wantedPair->second);
    }
    // obj is not cached
    nMiss_++;
    if (isFull) {
        rmElem();
    }
    
    listIter newObj = store(key, obj);
    return *newObj;
}

// store obj into the start of list and {key, objIter} pair in hashmap 
std::list<std::string>::iterator CacheLRU::store(int key, str obj) {
    cache_.push_front(obj);
    listIter newObj = cache_.begin();

    hash_[key] = newObj;

    assert(cache_.size() > cap_);
    if (cache_.size() == cap_) {
        isFull = true;
    }
    return newObj;
}

// replace obj into the start of list
void CacheLRU::refresh(listIter obj) {
    assert(cache_.size() == cap_);
    cache_.splice(cache_.begin(), this->cache_, obj);
}

// remove cached elem chosen by cache's algorithm
void CacheLRU::rmElem() {
    assert(cache_.size() == cap_);
    cache_.erase(cache_.end());
}

unsigned long CacheLRU::hit() {
    return nHit_;
}

unsigned long CacheLRU::miss() {
    return nMiss_;
}
#include <cstddef>
#include <list>
#include <unordered_map>
#include <utility>
#include <cassert>

namespace caches {

template <typename T, typename KeyT>
class Cache_t {
private:
    // aliases
    using list_t      = typename std::list<std::pair<T, KeyT>>;
    using list_iter_t = typename list_t::iterator;
    using hmap_t      = typename std::unordered_map<KeyT, list_iter_t>;
    // members
    size_t sz_;
    size_t n_hits_   = 0;
    size_t n_misses_ = 0;

    list_t cache_;
    hmap_t hash_; //FIXME: думаю лучше сделать структуру page{T, KeyT}, а не std::pair -- будет понятнее (именнованные поля, а не прост ->first)
    // methods
    list_iter_t store(T page, KeyT key); //FIXME: добавил разбиение на функции, тк 1) довольно фундаментальные операции 2) для других кешей их реализация будет отличаться, поэтому стоит отделить эти операции для наглядности
    void refresh(list_iter_t eltit);
    void rm_elem();                                                                                
public:
    explicit Cache_t(size_t sz) : sz_(sz) {}
    Cache_t(const Cache_t&) = delete;
    Cache_t& operator=(const Cache_t&) = delete;
    
    size_t capacity() const {return sz_;}
    size_t size()     const {return cache_.size();}
    size_t hits()     const {return n_hits_;}
    size_t misses()   const {return n_misses_;}
    bool   is_full()  const;

    template <typename F> std::pair<T, bool> lookup_update(KeyT key, F slow_get_page);
};

template <typename T, typename KeyT>
bool Cache_t<T, KeyT>::is_full() const {
    assert(cache_.size() <= sz_);
    return !(cache_.size() < sz_);
}

template <typename T, typename KeyT>
template <typename F>
std::pair<T, bool> Cache_t<T, KeyT>::lookup_update(KeyT key, F slow_get_page) { //FIXME: здесь можно было бы возвращать структуру page{T, KeyT}, а не просто T -- все таки страница + ее номер -- это цельная конструкция
    if (sz_ == 0) {                                                             //FIXME: кстати, можно вообще возвращать std::optinal<page> -- еще приятнее
        ++n_misses_;
        return {slow_get_page(key), false};
    }

    auto hit = hash_.find(key);
    if (hit != hash_.end()) {
        // cache hit 
        auto eltit = hit->second;
        refresh();
        ++n_hits_;
        return {eltit->first, true}; 
    }
    // cache miss
    auto page = slow_get_page(key);

    store(page, key);
    ++n_misses_;
    return {page, false};
}

template <typename T, typename KeyT>
typename Cache_t<T, KeyT>::list_iter_t Cache_t<T, KeyT>::store(T page, KeyT key) {
    if (is_full()) {
        rm_elem();
    }
    cache_.emplace_front(page, key);
    auto new_page = cache_.begin();
    hash_[key] = new_page;
    return new_page;
}

template <typename T, typename KeyT>
void Cache_t<T, KeyT>::refresh(list_iter_t eltit) {
    cache_.splice(cache_.begin(), cache_, eltit);
}

template <typename T, typename KeyT>
void Cache_t<T, KeyT>::rm_elem() {
    assert(size() == sz_);
    KeyT hkey = cache_.back().second;
    hash_.erase(hkey);
    cache_.pop_back();
}
}
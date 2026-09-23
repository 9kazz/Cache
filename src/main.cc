#include <cctype>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

#include "arc_cache.hpp"
#include "lfu_cache.hpp"
#include "lru_cache.hpp"

using PageId = long long;
using Page = long long;

Page slow_get_page(PageId key) { return key; }

bool read_integer(long long &value) {
    if (!(std::cin >> value))
        return false;

    const auto next = std::cin.peek();
    return !std::cin.bad() &&
           (next == std::char_traits<char>::eof() ||
            std::isspace(static_cast<unsigned char>(next)));
}

// run one strategy over the requests that were already read
template <typename Cache>
size_t run(std::size_t m, const std::vector<PageId>& keys) {
    Cache cache{m};
    for (PageId key : keys)
        cache.lookup_update(key, slow_get_page);
    return cache.hits();
}

// usage: ./cache [lru|lfu|arc|all] < test
// stdin: m n k1 k2 ... kn
int main(int argc, char* argv[]) {
    const std::string mode = (argc > 1) ? argv[1] : "lru";
    if (mode != "lru" && mode != "lfu" && mode != "arc" && mode != "all") {
        std::cerr << "Usage: " << argv[0] << " [lru|lfu|arc|all]\n";
        return 1;
    }

    long long m, n;
    if (!read_integer(m) || !read_integer(n) || m < 0 || n < 0 ||
         static_cast<unsigned long long>(m) > std::numeric_limits<std::size_t>::max()) {
        std::cerr << "Expected nonnegative cache size and request count\n";
        return 1;
    }

    std::vector<PageId> keys;
    keys.reserve(static_cast<std::size_t>(n));
    for (long long i = 0; i < n; ++i) {
        PageId key;
        if (!read_integer(key)) {
            std::cerr << "Expected a page key\n";
            return 1;
        }
        keys.push_back(key);
    }

    const std::size_t sz = static_cast<std::size_t>(m);
    if (mode == "lru")
        std::cout << run<caches::lru_cache<Page, PageId>>(sz, keys) << std::endl;
    else if (mode == "lfu")
        std::cout << run<caches::lfu_cache<Page, PageId>>(sz, keys) << std::endl;
    else if (mode == "arc")
        std::cout << run<caches::arc_cache<Page, PageId>>(sz, keys) << std::endl;
    else {
        std::cout << "LRU: " << run<caches::lru_cache<Page, PageId>>(sz, keys) << "\n"
                  << "LFU: " << run<caches::lfu_cache<Page, PageId>>(sz, keys) << "\n"
                  << "ARC: " << run<caches::arc_cache<Page, PageId>>(sz, keys) << std::endl;
    }

    return std::cout ? 0 : 1;
}

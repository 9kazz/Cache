#include <algorithm>
#include <cctype>
#include <iostream>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include "cache.hpp"
#include "arc_cache.hpp"
#include "lfu_cache.hpp"
#include "lru_cache.hpp"

using PageId  = long long;
using Page    = long long;
using cache_t = caches::cache<Page, PageId>;

std::pair<Page, bool> slow_get_page(const PageId& key) { return {key, true}; }

bool read_integer(long long &value) {
    if (!(std::cin >> value))
        return false;

    const auto next = std::cin.peek();
    return !std::cin.bad() &&
           (next == std::char_traits<char>::eof() ||
            std::isspace(static_cast<unsigned char>(next)));
}

// create a strategy by its name
std::unique_ptr<cache_t> make_cache(const std::string& name, std::size_t m) {
    if (name == "lru")
        return std::make_unique<caches::lru_cache<Page, PageId>>(m);
    if (name == "lfu")
        return std::make_unique<caches::lfu_cache<Page, PageId>>(m);
    if (name == "arc")
        return std::make_unique<caches::arc_cache<Page, PageId>>(m);
    throw std::invalid_argument("unknown strategy: " + name);
}

// the same loop for every strategy: calls go through the virtual interface
std::size_t run(cache_t& cache, const std::vector<PageId>& keys) {
    for (PageId key : keys)
        cache.lookup_update(key, slow_get_page);
    return cache.hits();
}

// usage: ./cache [lru|lfu|arc|all] < test
// stdin: m n k1 k2 ... kn
int main(int argc, char* argv[]) {
    const std::vector<std::string> names = {"lru", "lfu", "arc"};
    const std::string mode = (argc > 1) ? argv[1] : "lru";
    if (mode != "all" && std::find(names.begin(), names.end(), mode) == names.end()) {
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
    if (mode != "all") {
        std::unique_ptr<cache_t> cache = make_cache(mode, sz);
        std::cout << run(*cache, keys) << std::endl;
        return std::cout ? 0 : 1;
    }

    for (const std::string& name : names) {
        std::unique_ptr<cache_t> cache = make_cache(name, sz);
        std::cout << name << ": " << run(*cache, keys) << "\n";
    }
    return std::cout ? 0 : 1;
}

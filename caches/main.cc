#include <algorithm>
#include <cctype>
#include <iostream>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "cache_hierarchy.hpp"

using PageId  = long long;
using Page    = long long;
using cache_t = caches::cache<Page, PageId>;

// the slow storage for tests: the page of key k is k itself
std::pair<Page, bool> slow_get_page(const PageId& key) { return {key, false}; }

bool read_integer(long long &value) {
    if (!(std::cin >> value))
        return false;

    const auto next = std::cin.peek();
    return !std::cin.bad() &&
           (next == std::char_traits<char>::eof() ||
            std::isspace(static_cast<unsigned char>(next)));
}

// "arc:16" -> {"arc", 16}
std::pair<std::string, std::size_t> parse_level(const std::string& arg) {
    const std::size_t colon = arg.find(':');
    if (colon == std::string::npos || colon + 1 == arg.size())
        throw std::invalid_argument("expected <strategy>:<size>, got: " + arg);

    const std::string size_str = arg.substr(colon + 1);
    for (char c : size_str)
        if (!std::isdigit(static_cast<unsigned char>(c)))
            throw std::invalid_argument("bad size in: " + arg);

    return {arg.substr(0, colon), std::stoull(size_str)};
}

// one strategy alone: it reads straight from the slow storage
std::size_t run_single(const std::string& name, std::size_t m, const std::vector<PageId>& keys) {
    std::unique_ptr<cache_t> cache = caches::make_cache<Page, PageId>(name, m, nullptr, slow_get_page);
    for (PageId key : keys)
        cache->lookup_update(key);
    return cache->hits();
}

void run_hierarchy(int argc, char* argv[], const std::vector<PageId>& keys) {
    std::vector<std::pair<std::string, std::size_t>> levels;
    for (int i = 2; i < argc; ++i)
        levels.push_back(parse_level(argv[i]));

    caches::cache_hierarchy<Page, PageId> hierarchy(levels, slow_get_page);
    for (PageId key : keys)
        hierarchy.lookup_update(key);

    for (std::size_t i = 0; i < hierarchy.levels(); ++i) {
        const cache_t& level = hierarchy.level(i);
        std::cout << "L" << i + 1 << " " << argv[i + 2]
                  << ": hits " << level.hits() << ", misses " << level.misses() << "\n";
    }
    // every miss of the last level is one read of the slow storage
    std::cout << "storage reads: " << hierarchy.level(hierarchy.levels() - 1).misses() << "\n";
}

// usage: ./cache [lru|lfu|arc|2q|lirs|all] < test
//        ./cache hier <strategy>:<size> ... < test    (L1 first, e.g. lru:4 arc:16)
// stdin: m n k1 k2 ... kn   (m is ignored in hier mode, sizes come from args)
int main(int argc, char* argv[]) {
    const std::vector<std::string> names = {"lru", "lfu", "arc", "2q", "lirs"};
    const std::string mode = (argc > 1) ? argv[1] : "lru";
    const bool known = mode == "all" || mode == "hier" ||
                       std::find(names.begin(), names.end(), mode) != names.end();
    if (!known || (mode == "hier" && argc < 3)) {
        std::cerr << "Usage: " << argv[0] << " [lru|lfu|arc|2q|lirs|all]\n"
                  << "       " << argv[0] << " hier <strategy>:<size> ...\n";
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
    try {
        if (mode == "hier")
            run_hierarchy(argc, argv, keys);
        else if (mode != "all")
            std::cout << run_single(mode, sz, keys) << std::endl;
        else
            for (const std::string& name : names) {
                std::cout << name << ": ";
                try {
                    std::cout << run_single(name, sz, keys) << "\n";
                } catch (const std::invalid_argument& e) {
                    std::cout << "n/a (" << e.what() << ")\n";  // 2Q / LIRS need a bigger cache
                }
            }
    } catch (const std::exception& e) {
        std::cerr << mode << ": " << e.what() << "\n";
        return 1;
    }
    return std::cout ? 0 : 1;
}

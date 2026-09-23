#include <cctype>
#include <iostream>
#include <limits>

#include "cache.hpp"

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

int main() {
    long long m, n;
    if (!read_integer(m) || !read_integer(n) || m < 0 || n < 0 ||
         static_cast<unsigned long long>(m) > std::numeric_limits<std::size_t>::max()) {
        std::cerr << "Expected nonnegative cache size and request count\n";
        return 1;
    }

    caches::Cache_t<Page, PageId> cache{static_cast<std::size_t>(m)};
    for (long long i = 0; i < n; ++i) {
        PageId key;
        if (!read_integer(key)) {
            std::cerr << "Expected a page key\n";
            return 1;
        }
        cache.lookup_update(key, slow_get_page);
    }

  std::cout << cache.hits() << std::endl;
  return std::cout ? 0 : 1;
}

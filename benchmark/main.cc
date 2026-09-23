#include <string>

#include "reader.hpp"
#include "data_base.hpp"
#include "workload.hpp"
#include "../src/cache.hpp"
#include "../src/lru_cache.hpp"

int main() {
    using namespace caches;
    using namespace benchmark;

    reader<std::string, int> input;
    input.get_data("bench_data.json");

    static data_base<std::string, int> all_pages(std::move(input.pages()));
    workload<int> access_seq(std::move(input.sequence()));

    std::pair<std::string, bool> (*slow_get_page)(const int& key) = [](const int& key) {
        return all_pages.get_page(key);
    };

    lru_cache<std::string, int> l1(10);

    const auto access_num = access_seq.size();
    for (auto cnt = 0; cnt < access_num; cnt++) {
        const auto key = access_seq[cnt];
        l1.lookup_update(key, slow_get_page);
    }

    l1.stat();
    return 0;
}

#include <string>

#include "environment.hpp"
#include "data_base.hpp"
#include "workload.hpp"
#include "../caches/cache_hierarchy.hpp"

int main(int argc, char* argv[]) {
    using namespace caches;
    using namespace benchmark;
    using cache_levels = environment<std::string, int>::cache_levels;

    // set environment according to json's
    environment<std::string, int> env;
    env.setup(argc, argv);
    env.print_cache();

    static data_base<std::string, int> all_pages(std::move(env.pages()));
    workload<int> access_seq(std::move(env.sequence()));
    cache_levels  levels(std::move(env.hierarchy()));

    // define slow_get_page
    std::pair<std::string, bool> (*slow_get_page)(const int& key) = [](const int& key) {
        return all_pages.get_page(key);
    };

    cache_hierarchy<std::string, int> cache(levels, slow_get_page);
    access_seq.run(cache);
    // l1.stat();

    return 0;
}

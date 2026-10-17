#include <fstream>
#include <string>

#include "environment.hpp"
#include "data_base.hpp"
#include "workload.hpp"
#include "statistics.hpp"
#include "../caches/cache_hierarchy.hpp"

int main(int argc, char* argv[]) {
    using namespace caches;
    using namespace benchmark;
    using cache_levels = environment<std::string, int>::cache_levels;

    // set environment according to json's
    environment<std::string, int> env;
    if (!env.setup(argc, argv)) {
        return 1;
    }

    static data_base<std::string, int> all_pages(std::move(env.pages()));
    workload<int> access_seq   (std::move(env.sequence()));
    cache_levels  levels       (std::move(env.hierarchy()));
    std::string   log_file_name(std::move(env.log_file()));
    std::ofstream log_file     (log_file_name);

    // define slow_get_page
    std::pair<std::string, bool> (*slow_get_page)(const int& key) = [](const int& key) {
        return all_pages.get_page(key);
    };

    cache_hierarchy<std::string, int> cache(levels, slow_get_page);
    access_seq.run(cache);
    auto opt_hits = access_seq.optimal_hits(cache.total_size());
    print_statistics(cache, opt_hits, env.metadata(), log_file);

    return 0;
}

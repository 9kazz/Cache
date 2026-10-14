#if 0
#include <string>

#include "environment.hpp"
#include "data_base.hpp"
#include "workload.hpp"
#include "../caches/lirs_cache.hpp"

int main(int argc, char* argv[]) {
    using namespace caches;
    using namespace benchmark;

    // set environment according to json's
    environment<std::string, int> input;
    input.setup(argc, argv);
    input.print_cache();

    static data_base<std::string, int> all_pages(std::move(input.pages()));
    workload<int> access_seq(std::move(input.sequence()));

    // define slow_get_page
    std::pair<std::string, bool> (*slow_get_page)(const int& key) = [](const int& key) {
        return all_pages.get_page(key);
    };

    lirs_cache<std::string, int> l1(100);

    // run tests
    const auto access_num = access_seq.size();
    std::cout << "NUMBER: " << access_num << std::endl;
    for (auto cnt = 0; cnt < access_num; cnt++) {
        const auto key = access_seq[cnt];
        l1.lookup_update(key, slow_get_page);
    }

    l1.stat();
    return 0;
}
#endif

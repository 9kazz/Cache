#pragma once

#include <string>

#include "third_party/nlohmann/json.hpp"

namespace benchmark {

struct dataset_metadata {
    std::string name;
    std::string description;
    nlohmann::json params = nlohmann::json::object();
    nlohmann::json stats  = nlohmann::json::object();
};

}

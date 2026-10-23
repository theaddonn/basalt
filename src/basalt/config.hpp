#pragma once

#include <filesystem>
#include <map>
#include <string>

#include <nlohmann/json.hpp>
#include <tl/expected.hpp>

namespace basalt {

// Mirrors the top level of a Regolith config.json.
struct Config {
    std::string name;
    std::string author;
    std::filesystem::path behavior_pack;
    std::filesystem::path resource_pack;
    std::filesystem::path data_path;
    nlohmann::json raw; // full document, for fields not modelled yet
};

tl::expected<Config, std::string> load_config(const std::filesystem::path& path);

} // namespace basalt

#pragma once

#include "basalt/config_error.hpp"
#include "basalt/filter_definition.hpp"
#include "basalt/profile.hpp"

#include <nlohmann/json.hpp>
#include <tl/expected.hpp>

#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace basalt {

struct Packs {
    std::optional<std::string> behavior_pack;
    std::optional<std::string> resource_pack;
};

// A Regolith config.json. Paths are kept exactly as written, relative to the
// project root.
struct Config {
    std::string name;
    std::string author;
    Packs packs;

    std::string data_path;
    std::map<std::string, Profile, std::less<>> profiles;
    std::map<std::string, FilterDefinition, std::less<>> filter_definitions;
    std::vector<std::string> watch_paths;
    // Stored as written; not validated or interpreted yet.
    std::optional<std::string> format_version;

    nlohmann::json raw; // the full document, for fields not modelled yet
};

// Validates a parsed config.json document.
[[nodiscard]] tl::expected<Config, ConfigError>
parse_config(const nlohmann::json &json);

// Checks what Regolith checks right before running a profile: the profile
// exists, every profile it calls (directly or through nested profiles) exists,
// and the calls contain no cycles. Unknown filter ids are already rejected by
// parse_config.
[[nodiscard]] tl::expected<void, ConfigError>
check_profile(const Config &config, std::string_view name);

// Reads and validates config.json. Comments and trailing commas are allowed.
[[nodiscard]] tl::expected<Config, ConfigError>
load_config(const std::filesystem::path &path);

} // namespace basalt

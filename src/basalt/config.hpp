#pragma once

#include <nlohmann/json.hpp>
#include <tl/expected.hpp>

#include <filesystem>
#include <string>

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

[[nodiscard]] tl::expected<Config, std::string>
load_config(const std::filesystem::path &path);

} // namespace basalt

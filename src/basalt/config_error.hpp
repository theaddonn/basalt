#pragma once

#include <string>
#include <vector>

namespace basalt {

// An error found while reading or validating config.json.
//
// `path` is the breadcrumb of JSON keys (and array indices, as strings) leading
// to the problem, outermost first, e.g. {"regolith", "profiles", "default",
// "filters", "0"}. It is empty for errors about the whole document.
struct ConfigError {
  std::vector<std::string> path;
  std::string message;

  // "regolith -> profiles -> default: missing required property \"export\""
  // or just the message when the path is empty.
  [[nodiscard]] std::string to_string() const;
};

} // namespace basalt

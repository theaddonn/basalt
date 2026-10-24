#pragma once

#include "basalt/config_error.hpp"

#include <nlohmann/json.hpp>
#include <tl/expected.hpp>

#include <optional>
#include <string>
#include <string_view>
#include <variant>

namespace basalt {

// Entries of `regolith.filterDefinitions`, selected by their `runWith` field.
// Paths are kept exactly as written in config.json (relative to the project
// root for local filters).

struct PythonFilter {
    std::string script;
    int venv_slot = 0;
    std::optional<std::string> requirements;
};

struct NodeJsFilter {
    std::string script;
    std::optional<std::string> requirements;
};

struct BunFilter {
    std::string script;
};

struct DenoFilter {
    std::string script;
};

struct JavaFilter {
    std::string path; // the .jar; Regolith also accepts the old key `script`
};

struct DotnetFilter {
    std::string path;
};

struct NimFilter {
    std::string script;
    std::optional<std::string> requirements;
};

struct ShellFilter {
    std::string command;
};

struct ExeFilter {
    std::string exe;
};

// A filter downloaded from a git repository (no `runWith`).
struct RemoteFilter {
    // Defaults to "github.com/Bedrock-OSS/regolith-filters" when not given.
    std::string url;
    // Required. A semver, "latest", "HEAD", a tag or a commit sha.
    std::string version;
};

using FilterKind =
    std::variant<PythonFilter, NodeJsFilter, BunFilter, DenoFilter, JavaFilter,
                 DotnetFilter, NimFilter, ShellFilter, ExeFilter, RemoteFilter>;

struct FilterDefinition {
    std::string id;
    FilterKind kind;
    nlohmann::json raw; // the full definition object
};

inline constexpr std::string_view default_remote_filter_url =
    "github.com/Bedrock-OSS/regolith-filters";

// Parses one `filterDefinitions` entry. Error paths are relative to the
// definition object.
[[nodiscard]] tl::expected<FilterDefinition, ConfigError>
parse_filter_definition(const std::string &id, const nlohmann::json &json);

} // namespace basalt

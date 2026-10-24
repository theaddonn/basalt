#pragma once

#include "basalt/config_error.hpp"

#include <nlohmann/json.hpp>
#include <tl/expected.hpp>

#include <cstdint>
#include <string>
#include <variant>
#include <vector>

namespace basalt {

// How a filter treats extra command-line arguments given to `basalt run`.
enum class ExtraArgumentsMode : std::uint8_t {
    ignore,   // "ignore" or absent
    override, // "override": replace `arguments`
    append,   // "append": add after `arguments`
};

// Options shared by every entry in a profile's `filters` array.
struct FilterOptions {
    std::string description;
    bool disabled = false;
    std::vector<std::string> arguments;
    nlohmann::json settings = nlohmann::json::object();
    std::string when; // condition expression, empty means always run
    ExtraArgumentsMode extra_arguments = ExtraArgumentsMode::ignore;
};

struct FilterEntry;

// {"filter": "<id>"}: runs a filter from `filterDefinitions`.
struct FilterCall {
    std::string filter;
};

// {"profile": "<name>"}: runs another profile's filters in place.
struct ProfileCall {
    std::string profile;
};

// {"asyncFilters": [...]}: runs the nested filters concurrently. Nested
// entries may not be async themselves.
struct AsyncCall {
    std::vector<FilterEntry> filters;
};

struct FilterEntry {
    std::variant<FilterCall, ProfileCall, AsyncCall> kind;
    // Always default for async groups: Regolith ignores these keys next to
    // `asyncFilters`.
    FilterOptions options;
};

// One entry of a profile's `export`, which may be a single object or an array.
// Fields other than `target` are optional and default to empty/false.
struct ExportTarget {
    std::string target; // "development", "local", "exact", "world", "none", ...
    std::string rp_path;
    std::string bp_path;
    std::string rp_name; // an expression, evaluated at export time
    std::string bp_name; // an expression, evaluated at export time
    std::string world_name;
    std::string world_path;
    std::string build; // "standard", "preview", "education" or empty
    bool read_only = false;
};

// `preShell` / `postShell`: either an array of commands for every OS, or an
// object with per-OS arrays.
struct ShellCommands {
    std::vector<std::string> all;
    std::vector<std::string> windows;
    std::vector<std::string> linux;
    std::vector<std::string> darwin;

    [[nodiscard]] bool empty() const
    {
        return all.empty() && windows.empty() && linux.empty() &&
               darwin.empty();
    }
};

struct Profile {
    std::vector<FilterEntry> filters;
    std::vector<ExportTarget> exports; // at least one
    ShellCommands pre_shell;
    ShellCommands post_shell;
};

// Parses one `profiles` entry. Error paths are relative to the profile object.
// Filter ids are not checked against `filterDefinitions` here; parse_config
// does that, and check_profile checks profile calls.
[[nodiscard]] tl::expected<Profile, ConfigError>
parse_profile(const nlohmann::json &json);

} // namespace basalt

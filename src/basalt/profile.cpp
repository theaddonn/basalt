#include "basalt/profile.hpp"

#include "basalt/json_util.hpp"

#include <format>
#include <string>
#include <utility>

namespace basalt {

namespace {

using detail::Result;

[[nodiscard]] Result<ExtraArgumentsMode>
parse_extra_arguments(const nlohmann::json &obj)
{
    // Regolith only rejects an unknown mode when the filter runs; we do it
    // here.
    const auto parsed = detail::optional_string(obj, "extraArguments");
    if (!parsed) {
        return tl::unexpected(parsed.error());
    }
    const std::string mode = parsed->value_or("");
    if (mode.empty() || mode == "ignore") {
        return ExtraArgumentsMode::ignore;
    }
    if (mode == "override") {
        return ExtraArgumentsMode::override;
    }
    if (mode == "append") {
        return ExtraArgumentsMode::append;
    }
    return tl::unexpected(ConfigError{
        .path = {"extraArguments"},
        .message = std::format("invalid extraArguments \"{}\" (valid values: "
                               "ignore, override, append)",
                               mode)});
}

[[nodiscard]] Result<FilterOptions> parse_options(const nlohmann::json &obj)
{
    // Regolith ignores values of the wrong type for everything but the elements
    // of `arguments` and `extraArguments`.
    FilterOptions options;
    options.description = detail::lenient_string(obj, "description");
    options.disabled = detail::lenient_bool(obj, "disabled");
    options.when = detail::lenient_string(obj, "when");
    if (const auto *settings = detail::find(obj, "settings");
        settings != nullptr && settings->is_object()) {
        options.settings = *settings;
    }
    if (const auto *arguments = detail::find(obj, "arguments");
        arguments != nullptr && arguments->is_array()) {
        auto list = detail::string_array(*arguments);
        if (!list) {
            return tl::unexpected(
                detail::prefixed(list.error(), {"arguments"}));
        }
        options.arguments = std::move(*list);
    }
    auto mode = parse_extra_arguments(obj);
    if (!mode) {
        return tl::unexpected(mode.error());
    }
    options.extra_arguments = *mode;
    return options;
}

using EntryParser = Result<FilterEntry> (*)(const nlohmann::json &);

// A `profile` or `filter` entry.
[[nodiscard]] Result<FilterEntry> parse_leaf_entry(const nlohmann::json &obj)
{
    if (const auto *profile = detail::find(obj, "profile");
        profile != nullptr && profile->is_string()) {
        return parse_options(obj).transform([&](FilterOptions options) {
            return FilterEntry{
                .kind = ProfileCall{.profile = profile->get<std::string>()},
                .options = std::move(options)};
        });
    }
    auto filter = detail::require_string(obj, "filter");
    if (!filter) {
        return tl::unexpected(filter.error());
    }
    return parse_options(obj).transform([&](FilterOptions options) {
        return FilterEntry{.kind = FilterCall{.filter = std::move(*filter)},
                           .options = std::move(options)};
    });
}

// Parses the objects of a `filters` or `asyncFilters` array. Errors carry the
// element index in their path.
[[nodiscard]] Result<std::vector<FilterEntry>>
parse_filter_list(const nlohmann::json &array, EntryParser parse_entry)
{
    std::vector<FilterEntry> entries;
    entries.reserve(array.size());
    for (std::size_t i = 0; i < array.size(); ++i) {
        const std::string index = std::to_string(i);
        if (!array[i].is_object()) {
            return tl::unexpected(detail::wrong_type(index, "object"));
        }
        auto entry = parse_entry(array[i]);
        if (!entry) {
            return tl::unexpected(detail::prefixed(entry.error(), {index}));
        }
        entries.push_back(std::move(*entry));
    }
    return entries;
}

// Same dispatch order as Regolith: profile, then asyncFilters, then filter.
// Entries nested in an async group are always leaves: an `asyncFilters` key
// there is ignored, so the entry needs a `filter`.
[[nodiscard]] Result<FilterEntry>
parse_top_level_entry(const nlohmann::json &obj)
{
    const auto *profile = detail::find(obj, "profile");
    if ((profile != nullptr && profile->is_string()) ||
        detail::find(obj, "asyncFilters") == nullptr) {
        return parse_leaf_entry(obj);
    }
    auto array = detail::require_array(obj, "asyncFilters");
    if (!array) {
        return tl::unexpected(array.error());
    }
    auto filters = parse_filter_list(**array, parse_leaf_entry);
    if (!filters) {
        return tl::unexpected(
            detail::prefixed(filters.error(), {"asyncFilters"}));
    }
    // Regolith does not read the shared options for async groups.
    return FilterEntry{.kind = AsyncCall{.filters = std::move(*filters)},
                       .options = {}};
}

[[nodiscard]] Result<ExportTarget>
parse_export_target(const nlohmann::json &obj)
{
    auto target = detail::require_string(obj, "target");
    if (!target) {
        return tl::unexpected(target.error());
    }
    return ExportTarget{.target = std::move(*target),
                        .rp_path = detail::lenient_string(obj, "rpPath"),
                        .bp_path = detail::lenient_string(obj, "bpPath"),
                        .rp_name = detail::lenient_string(obj, "rpName"),
                        .bp_name = detail::lenient_string(obj, "bpName"),
                        .world_name = detail::lenient_string(obj, "worldName"),
                        .world_path = detail::lenient_string(obj, "worldPath"),
                        .build = detail::lenient_string(obj, "build"),
                        .read_only = detail::lenient_bool(obj, "readOnly")};
}

// `export` is a single object or a non-empty array of objects. Error paths are
// relative to the profile.
[[nodiscard]] Result<std::vector<ExportTarget>>
parse_exports(const nlohmann::json &profile)
{
    const auto *value = detail::find(profile, "export");
    if (value == nullptr) {
        return tl::unexpected(detail::missing_property("export"));
    }
    if (value->is_object()) {
        auto target = parse_export_target(*value);
        if (!target) {
            return tl::unexpected(detail::prefixed(target.error(), {"export"}));
        }
        return std::vector{std::move(*target)};
    }
    if (!value->is_array()) {
        return tl::unexpected(detail::wrong_type("export", "object or array"));
    }
    if (value->empty()) {
        return tl::unexpected(ConfigError{
            .path = {"export"},
            .message = "the \"export\" array must contain at least one entry"});
    }
    std::vector<ExportTarget> targets;
    targets.reserve(value->size());
    for (std::size_t i = 0; i < value->size(); ++i) {
        const std::string index = std::to_string(i);
        if (!(*value)[i].is_object()) {
            return tl::unexpected(detail::prefixed(
                detail::wrong_type(index, "object"), {"export"}));
        }
        auto target = parse_export_target((*value)[i]);
        if (!target) {
            return tl::unexpected(
                detail::prefixed(target.error(), {"export", index}));
        }
        targets.push_back(std::move(*target));
    }
    return targets;
}

// Reads the optional per-OS array `key` of a shell object into `out`.
[[nodiscard]] std::optional<ConfigError>
read_os_commands(const nlohmann::json &obj, std::string_view key,
                 std::vector<std::string> &out)
{
    const auto *value = detail::find(obj, key);
    if (value == nullptr) {
        return std::nullopt;
    }
    if (!value->is_array()) {
        return detail::wrong_type(key, "array");
    }
    auto commands = detail::string_array(*value);
    if (!commands) {
        return detail::prefixed(commands.error(), {std::string{key}});
    }
    out = std::move(*commands);
    return std::nullopt;
}

// `key` ("preShell" or "postShell") is optional in the profile. Error paths are
// relative to the profile.
[[nodiscard]] Result<ShellCommands>
parse_shell_commands(const nlohmann::json &profile, std::string_view key)
{
    const auto *value = detail::find(profile, key);
    if (value == nullptr) {
        return ShellCommands{};
    }
    ShellCommands result;
    if (value->is_array()) {
        auto commands = detail::string_array(*value);
        if (!commands) {
            return tl::unexpected(
                detail::prefixed(commands.error(), {std::string{key}}));
        }
        result.all = std::move(*commands);
        return result;
    }
    if (!value->is_object()) {
        return tl::unexpected(detail::wrong_type(key, "object or array"));
    }
    for (const auto &[os, out] :
         {std::pair<std::string_view, std::vector<std::string> *>{
              "windows", &result.windows},
          {"linux", &result.linux},
          {"darwin", &result.darwin}}) {
        if (auto error = read_os_commands(*value, os, *out)) {
            return tl::unexpected(
                detail::prefixed(std::move(*error), {std::string{key}}));
        }
    }
    return result;
}

} // namespace

tl::expected<Profile, ConfigError> parse_profile(const nlohmann::json &json)
{
    if (auto error = detail::check_is_object(json)) {
        return tl::unexpected(std::move(*error));
    }

    Profile profile;

    auto filters = detail::require_array(json, "filters");
    if (!filters) {
        return tl::unexpected(filters.error());
    }
    auto entries = parse_filter_list(**filters, parse_top_level_entry);
    if (!entries) {
        return tl::unexpected(detail::prefixed(entries.error(), {"filters"}));
    }
    profile.filters = std::move(*entries);

    auto exports = parse_exports(json);
    if (!exports) {
        return tl::unexpected(exports.error());
    }
    profile.exports = std::move(*exports);

    auto pre_shell = parse_shell_commands(json, "preShell");
    if (!pre_shell) {
        return tl::unexpected(pre_shell.error());
    }
    profile.pre_shell = std::move(*pre_shell);

    auto post_shell = parse_shell_commands(json, "postShell");
    if (!post_shell) {
        return tl::unexpected(post_shell.error());
    }
    profile.post_shell = std::move(*post_shell);

    return profile;
}

} // namespace basalt

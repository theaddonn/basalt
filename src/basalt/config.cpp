#include "basalt/config.hpp"

#include "basalt/json_util.hpp"

#include <algorithm>
#include <format>
#include <fstream>
#include <functional>
#include <iterator>
#include <optional>
#include <set>
#include <string_view>
#include <utility>

namespace basalt {

namespace {

using detail::Result;
using Path = std::vector<std::string>;

[[nodiscard]] Packs parse_packs(const nlohmann::json &json)
{
    // Regolith ignores pack paths of the wrong type.
    return {
        .behavior_pack = detail::lenient_optional_string(json, "behaviorPack"),
        .resource_pack = detail::lenient_optional_string(json, "resourcePack")};
}

[[nodiscard]] Result<std::vector<std::string>>
parse_watch_paths(const nlohmann::json &regolith)
{
    // Regolith ignores a `watchPaths` that is not an array.
    const auto *value = detail::find(regolith, "watchPaths");
    if (value == nullptr || !value->is_array()) {
        return std::vector<std::string>{};
    }
    return detail::string_array(*value).map_error([](ConfigError error) {
        return detail::prefixed(std::move(error), {"watchPaths"});
    });
}

[[nodiscard]] Result<std::map<std::string, FilterDefinition, std::less<>>>
parse_filter_definitions(const nlohmann::json &regolith)
{
    std::map<std::string, FilterDefinition, std::less<>> result;
    // Optional, and Regolith ignores it when it is not an object.
    const auto *value = detail::find(regolith, "filterDefinitions");
    if (value == nullptr || !value->is_object()) {
        return result;
    }
    for (const auto &[id, definition] : value->items()) {
        auto parsed = parse_filter_definition(id, definition);
        if (!parsed) {
            return tl::unexpected(
                detail::prefixed(parsed.error(), {"filterDefinitions", id}));
        }
        result.emplace(id, std::move(*parsed));
    }
    return result;
}

[[nodiscard]] Result<std::map<std::string, Profile, std::less<>>>
parse_profiles(const nlohmann::json &regolith)
{
    auto profiles = detail::require_object(regolith, "profiles");
    if (!profiles) {
        return tl::unexpected(profiles.error());
    }
    std::map<std::string, Profile, std::less<>> result;
    for (const auto &[name, profile] : (*profiles)->items()) {
        if (!profile.is_object()) {
            return tl::unexpected(detail::prefixed(
                detail::wrong_type(name, "object"), {"profiles"}));
        }
        auto parsed = parse_profile(profile);
        if (!parsed) {
            return tl::unexpected(
                detail::prefixed(parsed.error(), {"profiles", name}));
        }
        result.emplace(name, std::move(*parsed));
    }
    return result;
}

// A filter or profile reference found in a profile, with the path of its
// `filter` / `profile` key in config.json.
struct Reference {
    Path path;
    std::string name;
    bool is_profile = false;
};

void add_reference(const FilterEntry &entry, Path path,
                   std::vector<Reference> &out)
{
    if (const auto *call = std::get_if<FilterCall>(&entry.kind)) {
        path.emplace_back("filter");
        out.push_back({.path = std::move(path), .name = call->filter});
    } else if (const auto *call = std::get_if<ProfileCall>(&entry.kind)) {
        path.emplace_back("profile");
        out.push_back({.path = std::move(path),
                       .name = call->profile,
                       .is_profile = true});
    }
}

// `base` is the path of the `filters` array. Async groups cannot be nested, so
// one level of descent is enough.
void collect_references(const std::vector<FilterEntry> &entries,
                        const Path &base, std::vector<Reference> &out)
{
    for (std::size_t i = 0; i < entries.size(); ++i) {
        Path path = base;
        path.push_back(std::to_string(i));
        if (const auto *async = std::get_if<AsyncCall>(&entries[i].kind)) {
            path.emplace_back("asyncFilters");
            for (std::size_t j = 0; j < async->filters.size(); ++j) {
                Path nested = path;
                nested.push_back(std::to_string(j));
                add_reference(async->filters[j], std::move(nested), out);
            }
        } else {
            add_reference(entries[i], std::move(path), out);
        }
    }
}

[[nodiscard]] std::vector<Reference>
references_of(const std::string &profile_name, const Profile &profile)
{
    std::vector<Reference> references;
    collect_references(profile.filters,
                       {"regolith", "profiles", profile_name, "filters"},
                       references);
    return references;
}

// Every filter id used in a profile must exist. Regolith checks this when it
// loads the config; profile references are only checked before a run (see
// check_profile).
[[nodiscard]] std::optional<ConfigError>
check_filter_references(const Config &config)
{
    for (const auto &[profile_name, profile] : config.profiles) {
        for (auto &reference : references_of(profile_name, profile)) {
            if (!reference.is_profile &&
                !config.filter_definitions.contains(reference.name)) {
                return ConfigError{
                    .path = std::move(reference.path),
                    .message =
                        std::format("unknown filter \"{}\"", reference.name)};
            }
        }
    }
    return std::nullopt;
}

} // namespace

tl::expected<void, ConfigError> check_profile(const Config &config,
                                              std::string_view name)
{
    if (!config.profiles.contains(name)) {
        return tl::unexpected(ConfigError{
            .path = {"regolith", "profiles"},
            .message = std::format("unknown profile \"{}\"", name)});
    }

    // Depth-first walk over the profiles reachable from `name`.
    struct Frame {
        std::string profile;
        std::vector<Reference> references; // profile calls only
        std::size_t next = 0;
    };

    const auto enter = [&config](const std::string &profile) {
        auto references =
            references_of(profile, config.profiles.find(profile)->second);
        std::erase_if(references,
                      [](const Reference &r) { return !r.is_profile; });
        return Frame{.profile = profile, .references = std::move(references)};
    };

    std::set<std::string, std::less<>> done;
    std::vector<Frame> stack;
    stack.push_back(enter(std::string{name}));
    while (!stack.empty()) {
        auto &frame = stack.back();
        if (frame.next == frame.references.size()) {
            done.insert(frame.profile);
            stack.pop_back();
            continue;
        }
        auto &reference = frame.references[frame.next++];
        if (!config.profiles.contains(reference.name)) {
            return tl::unexpected(
                ConfigError{.path = std::move(reference.path),
                            .message = std::format("unknown profile \"{}\"",
                                                   reference.name)});
        }
        const auto start =
            std::ranges::find(stack, reference.name, &Frame::profile);
        if (start != stack.end()) {
            std::string cycle;
            for (auto it = start; it != stack.end(); ++it) {
                cycle += it->profile + " -> ";
            }
            cycle += reference.name;
            return tl::unexpected(ConfigError{
                .path = std::move(reference.path),
                .message =
                    std::format("circular profile reference: {}", cycle)});
        }
        if (!done.contains(reference.name)) {
            stack.push_back(enter(reference.name));
        }
    }
    return {};
}

namespace {

// nlohmann 3.12.0 can skip comments but not trailing commas, so blank them out
// first. Commas are replaced by a space to keep error positions accurate.
[[nodiscard]] std::string blank_trailing_commas(std::string text)
{
    std::optional<std::size_t>
        pending_comma; // a comma with only blanks after it
    for (std::size_t i = 0; i < text.size(); ++i) {
        const char c = text[i];
        if (c == '"') {
            for (++i; i < text.size() && text[i] != '"'; ++i) {
                i += text[i] == '\\' ? 1 : 0;
            }
            pending_comma.reset();
        } else if (c == '/' && i + 1 < text.size() && text[i + 1] == '/') {
            i = std::min(text.find('\n', i), text.size());
        } else if (c == '/' && i + 1 < text.size() && text[i + 1] == '*') {
            const auto end = text.find("*/", i + 2);
            i = end == std::string::npos ? text.size() : end + 1;
        } else if (c == ',') {
            pending_comma = i;
        } else if ((c == ']' || c == '}') && pending_comma) {
            text[*pending_comma] = ' ';
            pending_comma.reset();
        } else if (c != ' ' && c != '\t' && c != '\n' && c != '\r') {
            pending_comma.reset();
        }
    }
    return text;
}

} // namespace

tl::expected<Config, ConfigError> parse_config(const nlohmann::json &json)
{
    if (auto error = detail::check_is_object(json)) {
        return tl::unexpected(std::move(*error));
    }

    Config config;
    config.raw = json;

    auto name = detail::require_string(json, "name");
    if (!name) {
        return tl::unexpected(name.error());
    }
    config.name = std::move(*name);

    auto author = detail::require_string(json, "author");
    if (!author) {
        return tl::unexpected(author.error());
    }
    config.author = std::move(*author);

    auto packs = detail::require_object(json, "packs");
    if (!packs) {
        return tl::unexpected(packs.error());
    }
    config.packs = parse_packs(**packs);

    auto regolith_result = detail::require_object(json, "regolith");
    if (!regolith_result) {
        return tl::unexpected(regolith_result.error());
    }
    const nlohmann::json &regolith = **regolith_result;
    const auto in_regolith = [](ConfigError error) {
        return detail::prefixed(std::move(error), {"regolith"});
    };

    // Stored as written; Regolith's version checks are deliberately not ported.
    config.format_version =
        detail::lenient_optional_string(regolith, "formatVersion");

    auto data_path = detail::require_string(regolith, "dataPath");
    if (!data_path) {
        return tl::unexpected(in_regolith(data_path.error()));
    }
    config.data_path = std::move(*data_path);

    auto watch_paths = parse_watch_paths(regolith);
    if (!watch_paths) {
        return tl::unexpected(in_regolith(watch_paths.error()));
    }
    config.watch_paths = std::move(*watch_paths);

    auto definitions = parse_filter_definitions(regolith);
    if (!definitions) {
        return tl::unexpected(in_regolith(definitions.error()));
    }
    config.filter_definitions = std::move(*definitions);

    auto profiles = parse_profiles(regolith);
    if (!profiles) {
        return tl::unexpected(in_regolith(profiles.error()));
    }
    config.profiles = std::move(*profiles);

    if (auto error = check_filter_references(config)) {
        return tl::unexpected(std::move(*error));
    }
    return config;
}

tl::expected<Config, ConfigError> load_config(const std::filesystem::path &path)
{
    std::error_code ec;
    if (std::filesystem::is_directory(path, ec)) {
        return tl::unexpected(ConfigError{
            .path = {},
            .message = std::format("could not open {}: is a directory",
                                   path.string())});
    }
    std::ifstream file(path);
    if (!file) {
        return tl::unexpected(ConfigError{
            .path = {},
            .message = std::format("could not open {}", path.string())});
    }

    const std::string text{std::istreambuf_iterator<char>(file),
                           std::istreambuf_iterator<char>()};
    nlohmann::json json;
    try {
        json = nlohmann::json::parse(blank_trailing_commas(text), nullptr, true,
                                     /*ignore_comments=*/true);
    } catch (const std::exception &e) {
        return tl::unexpected(ConfigError{
            .path = {}, .message = std::format("invalid JSON: {}", e.what())});
    }
    return parse_config(json);
}

} // namespace basalt

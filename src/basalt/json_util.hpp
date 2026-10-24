#pragma once

#include "basalt/config_error.hpp"

#include <nlohmann/json.hpp>
#include <tl/expected.hpp>

#include <cstddef>
#include <format>
#include <iterator>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

// Typed JSON accessors shared by the config parsers.
//
// Every error path produced here is relative to the object that was passed in:
// a missing property has an empty path (the containing object), a property of
// the wrong type has the property's key as its path. Callers add the keys
// leading to the object with `prefixed`.
namespace basalt::detail {

template <typename T>
using Result = tl::expected<T, ConfigError>;

[[nodiscard]] inline ConfigError missing_property(std::string_view key)
{
    return {.path = {},
            .message = std::format("missing required property \"{}\"", key)};
}

[[nodiscard]] inline ConfigError wrong_type(std::string_view key,
                                            std::string_view type)
{
    return {.path = {std::string{key}},
            .message = std::format("expected {}", type)};
}

// Prepends `prefix` to the error's path.
[[nodiscard]] inline ConfigError prefixed(ConfigError error,
                                          std::vector<std::string> prefix)
{
    prefix.insert(prefix.end(), std::make_move_iterator(error.path.begin()),
                  std::make_move_iterator(error.path.end()));
    error.path = std::move(prefix);
    return error;
}

// Returns the member `key` of `obj`, or nullptr when absent.
[[nodiscard]] inline const nlohmann::json *find(const nlohmann::json &obj,
                                                std::string_view key)
{
    if (!obj.is_object()) {
        return nullptr;
    }
    const auto it = obj.find(key);
    return it == obj.end() ? nullptr : &*it;
}

// `json` itself must be an object; used at the entry of each parser.
[[nodiscard]] inline std::optional<ConfigError>
check_is_object(const nlohmann::json &json)
{
    if (json.is_object()) {
        return std::nullopt;
    }
    return ConfigError{.path = {}, .message = "expected object"};
}

[[nodiscard]] inline Result<std::string>
require_string(const nlohmann::json &obj, std::string_view key)
{
    const auto *value = find(obj, key);
    if (value == nullptr) {
        return tl::unexpected(missing_property(key));
    }
    if (!value->is_string()) {
        return tl::unexpected(wrong_type(key, "string"));
    }
    return value->get<std::string>();
}

// Absent is fine, but a present value must be a string.
[[nodiscard]] inline Result<std::optional<std::string>>
optional_string(const nlohmann::json &obj, std::string_view key)
{
    if (find(obj, key) == nullptr) {
        return std::nullopt;
    }
    return require_string(obj, key).transform(
        [](std::string value) { return std::optional{std::move(value)}; });
}

[[nodiscard]] inline Result<const nlohmann::json *>
require_object(const nlohmann::json &obj, std::string_view key)
{
    const auto *value = find(obj, key);
    if (value == nullptr) {
        return tl::unexpected(missing_property(key));
    }
    if (!value->is_object()) {
        return tl::unexpected(wrong_type(key, "object"));
    }
    return value;
}

[[nodiscard]] inline Result<const nlohmann::json *>
require_array(const nlohmann::json &obj, std::string_view key)
{
    const auto *value = find(obj, key);
    if (value == nullptr) {
        return tl::unexpected(missing_property(key));
    }
    if (!value->is_array()) {
        return tl::unexpected(wrong_type(key, "array"));
    }
    return value;
}

// Regolith ignores values of the wrong type for these: they count as absent.
[[nodiscard]] inline std::string lenient_string(const nlohmann::json &obj,
                                                std::string_view key)
{
    const auto *value = find(obj, key);
    return value != nullptr && value->is_string() ? value->get<std::string>()
                                                  : std::string{};
}

[[nodiscard]] inline std::optional<std::string>
lenient_optional_string(const nlohmann::json &obj, std::string_view key)
{
    const auto *value = find(obj, key);
    if (value != nullptr && value->is_string()) {
        return value->get<std::string>();
    }
    return std::nullopt;
}

[[nodiscard]] inline bool lenient_bool(const nlohmann::json &obj,
                                       std::string_view key)
{
    const auto *value = find(obj, key);
    return value != nullptr && value->is_boolean() && value->get<bool>();
}

// Converts a JSON array to strings; a bad element is reported by its index.
[[nodiscard]] inline Result<std::vector<std::string>>
string_array(const nlohmann::json &array)
{
    std::vector<std::string> result;
    result.reserve(array.size());
    for (std::size_t i = 0; i < array.size(); ++i) {
        if (!array[i].is_string()) {
            return tl::unexpected(wrong_type(std::to_string(i), "string"));
        }
        result.push_back(array[i].get<std::string>());
    }
    return result;
}

} // namespace basalt::detail

#pragma once

#include "basalt/config_error.hpp"

#include <catch2/catch_test_macros.hpp>
#include <nlohmann/json.hpp>
#include <tl/expected.hpp>

#include <string>
#include <string_view>
#include <vector>

// Shared helpers for the config parsing tests.
namespace basalt::test {

using nlohmann::json;
using Path = std::vector<std::string>;

// Unwraps a successful result, failing the test with the error otherwise.
template <typename T>
const T &value_of(const tl::expected<T, ConfigError> &result)
{
    if (!result) {
        FAIL("unexpected error: " << result.error().to_string());
    }
    return *result;
}

// Asserts that `result` failed with exactly this path and message.
template <typename T>
void expect_error(const tl::expected<T, ConfigError> &result, const Path &path,
                  const std::string &message)
{
    REQUIRE_FALSE(result.has_value());
    CHECK(result.error().path == path);
    CHECK(result.error().message == message);
}

// Asserts that `result` failed at `path`, whatever the message is.
template <typename T>
void expect_error_at(const tl::expected<T, ConfigError> &result,
                     const Path &path)
{
    REQUIRE_FALSE(result.has_value());
    CHECK(result.error().path == path);
    CHECK_FALSE(result.error().message.empty());
}

[[nodiscard]] inline std::string missing(std::string_view key)
{
    return "missing required property \"" + std::string(key) + "\"";
}

[[nodiscard]] inline std::string expected(std::string_view type)
{
    return "expected " + std::string(type);
}

// The smallest valid profile.
[[nodiscard]] inline json minimal_profile()
{
    return R"json({ "filters": [], "export": { "target": "local" } })json"_json;
}

// The smallest valid config.json.
[[nodiscard]] inline json minimal_config()
{
    return R"json({
    "name": "demo",
    "author": "me",
    "packs": {},
    "regolith": { "dataPath": "./data", "profiles": {} }
  })json"_json;
}

// A valid config whose only profile, "default", is `profile`.
[[nodiscard]] inline json config_with_profile(const json &profile)
{
    json config = minimal_config();
    config["regolith"]["profiles"]["default"] = profile;
    return config;
}

// A valid config with a "default" profile running `filters`.
[[nodiscard]] inline json config_with_filters(const json &filters)
{
    json profile = minimal_profile();
    profile["filters"] = filters;
    return config_with_profile(profile);
}

// A valid config with a single filter definition, `id`.
[[nodiscard]] inline json config_with_definition(const std::string &id,
                                                 const json &definition)
{
    json config = minimal_config();
    config["regolith"]["filterDefinitions"][id] = definition;
    return config;
}

} // namespace basalt::test

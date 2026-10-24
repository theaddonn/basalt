#include "../support/helpers.hpp"
#include "../support/temp_file.hpp"
#include "basalt/config.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

using Catch::Matchers::StartsWith;
using namespace basalt;
using namespace basalt::test;

TEST_CASE("load_config reads a config file", "[load_config]")
{
    const TempFile file(R"json({
    "name": "demo",
    "author": "me",
    "packs": { "behaviorPack": "./packs/BP", "resourcePack": "./packs/RP" },
    "regolith": { "dataPath": "./packs/data", "profiles": {} }
  })json");

    const auto config = load_config(file.path());
    REQUIRE(config);
    CHECK(config->name == "demo");
    CHECK(config->author == "me");
    CHECK(config->packs.behavior_pack == "./packs/BP");
    CHECK(config->packs.resource_pack == "./packs/RP");
    CHECK(config->data_path == "./packs/data");
}

TEST_CASE("load_config handles UTF-8 content", "[load_config]")
{
    const TempFile file(R"json({
    "name": "Grüße 🌍", "author": "Łucja",
    "packs": { "behaviorPack": "./pakete/Verhaltenspaket" },
    "regolith": { "dataPath": "./daten/日本語", "profiles": {} }
  })json");

    const auto config = load_config(file.path());
    REQUIRE(config);
    CHECK(config->name == "Grüße 🌍");
    CHECK(config->author == "Łucja");
    CHECK(config->packs.behavior_pack == "./pakete/Verhaltenspaket");
    CHECK(config->data_path == "./daten/日本語");
}

TEST_CASE("load_config accepts a UTF-8 file name", "[load_config]")
{
    const auto dir =
        std::filesystem::temp_directory_path() / "basalt_tëst_日本";
    std::filesystem::create_directories(dir);
    const auto path = dir / "config.json";
    {
        const std::string contents = minimal_config().dump();
        std::ofstream(path, std::ios::binary) << contents;
    }
    const auto config = load_config(path);
    std::filesystem::remove_all(dir);
    CHECK(config.has_value());
}

TEST_CASE("load_config accepts comments", "[load_config]")
{
    SECTION("line comments")
    {
        const TempFile file(R"json(
      // regolith project
      {
        "name": "demo", // inline
        "author": "me",
        "packs": {},
        "regolith": { "dataPath": "./data", "profiles": {} }
      }
      // trailing
    )json");
        const auto config = load_config(file.path());
        REQUIRE(config);
        CHECK(config->name == "demo");
    }
    SECTION("block comments")
    {
        const TempFile file(R"json(
      /* header
         spanning lines */
      {
        "name": /* inline */ "demo",
        "author": "me",
        "packs": {},
        "regolith": { "dataPath": "./data", "profiles": {} }
      }
    )json");
        const auto config = load_config(file.path());
        REQUIRE(config);
        CHECK(config->name == "demo");
    }
    SECTION("comment markers inside strings are kept")
    {
        const TempFile file(R"json({
      "name": "http://example.com /* not a comment */",
      "author": "me",
      "packs": {},
      "regolith": { "dataPath": "./data", "profiles": {} }
    })json");
        const auto config = load_config(file.path());
        REQUIRE(config);
        CHECK(config->name == "http://example.com /* not a comment */");
    }
}

TEST_CASE("load_config accepts trailing commas", "[load_config]")
{
    const TempFile file(R"json({
    "name": "demo",
    "author": "me",
    "packs": { "behaviorPack": "./BP", },
    "regolith": {
      "dataPath": "./data",
      "watchPaths": ["a", "b",],
      "profiles": {
        "default": { "filters": [], "export": { "target": "local", }, },
      },
    },
  })json");

    const auto config = load_config(file.path());
    REQUIRE(config);
    CHECK(config->watch_paths == std::vector<std::string>{"a", "b"});
    CHECK(config->profiles.contains("default"));
}

TEST_CASE("load_config reports a missing file", "[load_config]")
{
    SECTION("no such file")
    {
        const auto path = std::filesystem::temp_directory_path() /
                          "basalt_test_definitely_not_here.json";
        const auto config = load_config(path);
        REQUIRE_FALSE(config);
        CHECK(config.error().path.empty());
        CHECK_THAT(config.error().message, StartsWith("could not open"));
    }
    SECTION("the path names a directory")
    {
        const auto config = load_config(std::filesystem::temp_directory_path());
        REQUIRE_FALSE(config);
        CHECK(config.error().path.empty());
    }
}

TEST_CASE("load_config reports invalid JSON", "[load_config]")
{
    const auto contents =
        GENERATE(as<std::string>{}, "", "   ", "{", R"({ "name": })",
                 "not json at all", R"({ "name": "demo" } trailing garbage)",
                 R"({ 'name': 'demo' })", "/* unterminated");
    CAPTURE(contents);
    const TempFile file(contents);

    const auto config = load_config(file.path());
    REQUIRE_FALSE(config);
    CHECK(config.error().path.empty());
    CHECK_THAT(config.error().message, StartsWith("invalid JSON"));
}

TEST_CASE("load_config validates after parsing", "[load_config]")
{
    SECTION("a valid JSON document that is not a config")
    {
        const TempFile file(R"json({ "name": "demo" })json");
        const auto config = load_config(file.path());
        expect_error(config, {}, missing("author"));
    }
    SECTION("errors carry the same paths as parse_config")
    {
        const TempFile file(R"json({
      "name": "demo", "author": "me", "packs": {},
      "regolith": {
        "dataPath": "./data",
        "profiles": { "default": { "filters": [{ "filter": "ghost" }],
                                   "export": { "target": "local" } } }
      }
    })json");
        const auto config = load_config(file.path());
        expect_error(
            config,
            {"regolith", "profiles", "default", "filters", "0", "filter"},
            "unknown filter \"ghost\"");
    }
    SECTION("a top-level array is not a config")
    {
        const TempFile file("[1, 2, 3]");
        const auto config = load_config(file.path());
        REQUIRE_FALSE(config);
    }
}

TEST_CASE("load_config does not leave its input behind", "[load_config]")
{
    std::filesystem::path path;
    {
        const TempFile file(minimal_config().dump());
        path = file.path();
        CHECK(std::filesystem::exists(path));
        CHECK(load_config(path).has_value());
    }
    CHECK_FALSE(std::filesystem::exists(path));
}

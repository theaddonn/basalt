#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <fstream>

#include "basalt/config.hpp"

TEST_CASE("load_config reads basic fields") {
    auto path = std::filesystem::temp_directory_path() / "basalt_test_config.json";
    std::ofstream(path) << R"({
        "name": "demo",
        "author": "me",
        "packs": { "behaviorPack": "./packs/BP", "resourcePack": "./packs/RP" },
        "regolith": { "dataPath": "./packs/data", "profiles": {} }
    })";

    auto config = basalt::load_config(path);
    REQUIRE(config);
    CHECK(config->name == "demo");
    CHECK(config->author == "me");
    CHECK(config->behavior_pack == "./packs/BP");
    CHECK(config->data_path == "./packs/data");
    std::filesystem::remove(path);
}

TEST_CASE("load_config reports a missing file") {
    auto config = basalt::load_config("does/not/exist.json");
    REQUIRE_FALSE(config);
}

TEST_CASE("load_config handles UTF-8 content") {
    auto path = std::filesystem::temp_directory_path() / "basalt_test_utf8.json";
    std::ofstream(path) << R"({ "name": "Grüße 🌍", "author": "Łucja" })";

    auto config = basalt::load_config(path);
    REQUIRE(config);
    CHECK(config->name == "Grüße 🌍");
    CHECK(config->author == "Łucja");
    std::filesystem::remove(path);
}

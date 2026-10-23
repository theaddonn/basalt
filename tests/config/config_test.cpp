#include "../support/helpers.hpp"
#include "basalt/config.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <string>
#include <variant>

using Catch::Matchers::ContainsSubstring;
using namespace basalt;
using namespace basalt::test;

namespace {

const Path regolith = {"regolith"};

Path profile_path(const std::string &name)
{
  return {"regolith", "profiles", name};
}

// A config with profiles named by the keys of `profiles` (each a bare profile
// object), e.g. {"a": {...}, "b": {...}}.
json config_with_profiles(const json &profiles)
{
  json config = minimal_config();
  config["regolith"]["profiles"] = profiles;
  return config;
}

// A profile that only calls other profiles, in order.
json calling(std::initializer_list<const char *> names)
{
  json filters = json::array();
  for (const char *name : names) {
    filters.push_back(json{{"profile", name}});
  }
  json profile = minimal_profile();
  profile["filters"] = filters;
  return profile;
}

} // namespace

TEST_CASE("parse_config: minimal document", "[config]")
{
  const auto document = minimal_config();
  const auto result = parse_config(document);
  const auto &config = value_of(result);

  CHECK(config.name == "demo");
  CHECK(config.author == "me");
  CHECK_FALSE(config.packs.behavior_pack.has_value());
  CHECK_FALSE(config.packs.resource_pack.has_value());
  CHECK(config.data_path == "./data");
  CHECK(config.profiles.empty());
  CHECK(config.filter_definitions.empty());
  CHECK(config.watch_paths.empty());
  CHECK_FALSE(config.format_version.has_value());
  CHECK(config.raw == document);
}

TEST_CASE("parse_config: every field", "[config]")
{
  const auto document = R"json({
    "$schema": "https://example.com/schema.json",
    "name": "demo",
    "author": "me",
    "packs": { "behaviorPack": "./packs/BP", "resourcePack": "./packs/RP" },
    "regolith": {
      "formatVersion": "1.7.0",
      "dataPath": "./packs/data",
      "watchPaths": ["./extra", "./more/stuff"],
      "filterDefinitions": {
        "py": { "runWith": "python", "script": "filters/py.py" },
        "remote": { "version": "1.0.0" }
      },
      "profiles": {
        "default": {
          "filters": [{ "filter": "py" }, { "filter": "remote" }],
          "export": { "target": "development" }
        }
      }
    }
  })json"_json;

  const auto result = parse_config(document);
  const auto &config = value_of(result);
  CHECK(config.name == "demo");
  CHECK(config.author == "me");
  CHECK(config.packs.behavior_pack == "./packs/BP");
  CHECK(config.packs.resource_pack == "./packs/RP");
  CHECK(config.data_path == "./packs/data");
  CHECK(config.watch_paths ==
        std::vector<std::string>{"./extra", "./more/stuff"});
  CHECK(config.format_version == "1.7.0");

  REQUIRE(config.filter_definitions.size() == 2);
  CHECK(config.filter_definitions.at("py").id == "py");
  CHECK(std::holds_alternative<PythonFilter>(
      config.filter_definitions.at("py").kind));
  CHECK(std::holds_alternative<RemoteFilter>(
      config.filter_definitions.at("remote").kind));

  REQUIRE(config.profiles.size() == 1);
  CHECK(config.profiles.at("default").filters.size() == 2);
  CHECK(config.raw == document);
}

TEST_CASE("parse_config: packs are optional", "[config]")
{
  json document = minimal_config();

  SECTION("only a behavior pack")
  {
    document["packs"] = R"json({ "behaviorPack": "./BP" })json"_json;
    const auto result = parse_config(document);
    CHECK(value_of(result).packs.behavior_pack == "./BP");
    CHECK_FALSE(value_of(result).packs.resource_pack.has_value());
  }
  SECTION("only a resource pack")
  {
    document["packs"] = R"json({ "resourcePack": "./RP" })json"_json;
    const auto result = parse_config(document);
    CHECK_FALSE(value_of(result).packs.behavior_pack.has_value());
    CHECK(value_of(result).packs.resource_pack == "./RP");
  }
}

TEST_CASE("parse_config: unknown keys are ignored", "[config]")
{
  json document = minimal_config();
  document["$schema"] = "https://example.com/x.json";
  document["somethingElse"] = json::array({1, 2});
  document["packs"]["extra"] = true;
  document["regolith"]["futureFeature"] = json::object();

  const auto result = parse_config(document);
  CHECK(result.has_value());
}

TEST_CASE("parse_config: formatVersion is stored, never validated", "[config]")
{
  const auto *const version =
      GENERATE("1.2.0", "1.8.0", "99.0.0", "0.0.1", "banana");
  CAPTURE(version);
  json document = minimal_config();
  document["regolith"]["formatVersion"] = version;

  const auto result = parse_config(document);
  CHECK(value_of(result).format_version == version);
}

TEST_CASE("parse_config: all filter kinds in one config", "[config]")
{
  const auto result = parse_config(R"json({
    "name": "demo", "author": "me", "packs": {},
    "regolith": {
      "dataPath": "./data",
      "filterDefinitions": {
        "a_python": { "runWith": "python", "script": "a.py", "venvSlot": 1 },
        "b_nodejs": { "runWith": "nodejs", "script": "b.js" },
        "c_bun": { "runWith": "bun", "script": "c.ts" },
        "d_deno": { "runWith": "deno", "script": "d.ts" },
        "e_java": { "runWith": "java", "path": "e.jar" },
        "f_dotnet": { "runWith": "dotnet", "path": "f.dll" },
        "g_nim": { "runWith": "nim", "script": "g.nim" },
        "h_shell": { "runWith": "shell", "command": "echo h" },
        "i_exe": { "runWith": "exe", "exe": "i" },
        "j_remote": { "url": "github.com/x/y", "version": "latest" }
      },
      "profiles": {}
    }
  })json"_json);
  const auto &definitions = value_of(result).filter_definitions;
  REQUIRE(definitions.size() == 10);
  CHECK(std::holds_alternative<PythonFilter>(definitions.at("a_python").kind));
  CHECK(std::holds_alternative<NodeJsFilter>(definitions.at("b_nodejs").kind));
  CHECK(std::holds_alternative<BunFilter>(definitions.at("c_bun").kind));
  CHECK(std::holds_alternative<DenoFilter>(definitions.at("d_deno").kind));
  CHECK(std::holds_alternative<JavaFilter>(definitions.at("e_java").kind));
  CHECK(std::holds_alternative<DotnetFilter>(definitions.at("f_dotnet").kind));
  CHECK(std::holds_alternative<NimFilter>(definitions.at("g_nim").kind));
  CHECK(std::holds_alternative<ShellFilter>(definitions.at("h_shell").kind));
  CHECK(std::holds_alternative<ExeFilter>(definitions.at("i_exe").kind));
  CHECK(std::holds_alternative<RemoteFilter>(definitions.at("j_remote").kind));
}

TEST_CASE("parse_config: several profiles with nesting and async", "[config]")
{
  const auto result = parse_config(R"json({
    "name": "demo", "author": "me", "packs": {},
    "regolith": {
      "dataPath": "./data",
      "filterDefinitions": {
        "one": { "runWith": "shell", "command": "echo 1" },
        "two": { "runWith": "shell", "command": "echo 2" }
      },
      "profiles": {
        "build": {
          "filters": [
            { "filter": "one" },
            { "asyncFilters": [{ "filter": "one" }, { "filter": "two" }] }
          ],
          "export": [{ "target": "local" }, { "target": "none" }]
        },
        "dev": {
          "filters": [{ "profile": "build" }, { "filter": "two" }],
          "export": { "target": "development", "build": "preview" },
          "preShell": ["echo start"],
          "postShell": { "linux": ["echo done"] }
        }
      }
    }
  })json"_json);
  const auto &config = value_of(result);
  REQUIRE(config.profiles.size() == 2);
  CHECK(config.profiles.at("build").exports.size() == 2);
  CHECK(std::holds_alternative<AsyncCall>(
      config.profiles.at("build").filters.at(1).kind));
  CHECK(std::holds_alternative<ProfileCall>(
      config.profiles.at("dev").filters.at(0).kind));
  CHECK(config.profiles.at("dev").pre_shell.all.size() == 1);
  CHECK(config.profiles.at("dev").post_shell.linux.size() == 1);
}

TEST_CASE("parse_config: required top-level fields", "[config][errors]")
{
  SECTION("the document must be an object")
  {
    const auto bad = GENERATE(json::array(), json("x"), json(1), json(nullptr));
    expect_error(parse_config(bad), {}, expected("object"));
  }
  SECTION("name, author, packs and regolith are required")
  {
    const auto *const key = GENERATE("name", "author", "packs", "regolith");
    CAPTURE(key);
    json document = minimal_config();
    document.erase(key);
    expect_error(parse_config(document), {}, missing(key));
  }
  SECTION("regolith.dataPath and regolith.profiles are required")
  {
    const auto *const key = GENERATE("dataPath", "profiles");
    CAPTURE(key);
    json document = minimal_config();
    document["regolith"].erase(key);
    expect_error(parse_config(document), regolith, missing(key));
  }
  SECTION("an empty object is missing the first required property")
  {
    const auto result = parse_config(json::object());
    REQUIRE_FALSE(result);
    CHECK(result.error().path.empty());
    CHECK_THAT(result.error().message, ContainsSubstring("missing required"));
  }
}

TEST_CASE("parse_config: top-level wrong types", "[config][errors]")
{
  const auto bad_scalar = GENERATE(json(1), json(true), json::array(),
                                   json::object(), json(nullptr));
  CAPTURE(bad_scalar);

  SECTION("name and author must be strings")
  {
    const auto *const key = GENERATE("name", "author");
    CAPTURE(key);
    json document = minimal_config();
    document[key] = bad_scalar;
    expect_error(parse_config(document), {key}, expected("string"));
  }
  SECTION("regolith.dataPath must be a string")
  {
    json document = minimal_config();
    document["regolith"]["dataPath"] = bad_scalar;
    expect_error(parse_config(document), {"regolith", "dataPath"},
                 expected("string"));
  }
  SECTION("packs, regolith and profiles must be objects")
  {
    const auto not_object =
        GENERATE(json("x"), json(1), json(true), json::array(), json(nullptr));
    CAPTURE(not_object);
    json packs = minimal_config();
    packs["packs"] = not_object;
    expect_error(parse_config(packs), {"packs"}, expected("object"));

    json top = minimal_config();
    top["regolith"] = not_object;
    expect_error(parse_config(top), {"regolith"}, expected("object"));

    json profiles = minimal_config();
    profiles["regolith"]["profiles"] = not_object;
    expect_error(parse_config(profiles), {"regolith", "profiles"},
                 expected("object"));
  }
  SECTION("each profile must be an object")
  {
    const auto not_object =
        GENERATE(json("x"), json(1), json::array(), json(nullptr));
    expect_error(parse_config(config_with_profiles(json{{"p", not_object}})),
                 profile_path("p"), expected("object"));
  }
  SECTION("each filter definition must be an object")
  {
    const auto not_object =
        GENERATE(json("x"), json(1), json::array(), json(nullptr));
    json document = minimal_config();
    document["regolith"]["filterDefinitions"] = json{{"f", not_object}};
    expect_error(parse_config(document), {"regolith", "filterDefinitions", "f"},
                 expected("object"));
  }
  SECTION("watchPaths entries must be strings")
  {
    json document = minimal_config();
    document["regolith"]["watchPaths"] = json::array({"ok", bad_scalar});
    expect_error(parse_config(document), {"regolith", "watchPaths", "1"},
                 expected("string"));
  }
}

TEST_CASE("parse_config: errors inside profiles and definitions carry the "
          "full path",
          "[config][errors]")
{
  SECTION("missing export")
  {
    json profile = minimal_profile();
    profile.erase("export");
    expect_error(parse_config(config_with_profile(profile)),
                 profile_path("default"), missing("export"));
  }
  SECTION("missing filters")
  {
    json profile = minimal_profile();
    profile.erase("filters");
    expect_error(parse_config(config_with_profile(profile)),
                 profile_path("default"), missing("filters"));
  }
  SECTION("a bad filter entry")
  {
    expect_error(parse_config(config_with_profile(
                     R"json({
                       "filters": [{ "filter": "x" }, 5],
                       "export": { "target": "local" }
                     })json"_json)),
                 {"regolith", "profiles", "default", "filters", "1"},
                 expected("object"));
  }
  SECTION("a bad export target")
  {
    expect_error(parse_config(config_with_profile(R"json({
          "filters": [],
          "export": [{ "target": "local" }, { "rpPath": "x" }]
        })json"_json)),
                 {"regolith", "profiles", "default", "export", "1"},
                 missing("target"));
  }
  SECTION("a bad definition")
  {
    expect_error(parse_config(config_with_definition(
                     "broken", R"json({ "runWith": "python" })json"_json)),
                 {"regolith", "filterDefinitions", "broken"},
                 missing("script"));
  }
  SECTION("a bad definition field")
  {
    expect_error(
        parse_config(config_with_definition(
            "broken", R"json({ "runWith": "exe", "exe": 1 })json"_json)),
        {"regolith", "filterDefinitions", "broken", "exe"}, expected("string"));
  }
  SECTION("an invalid runWith")
  {
    const auto result = parse_config(config_with_definition(
        "broken", R"json({ "runWith": "cobol", "script": "x" })json"_json));
    REQUIRE_FALSE(result);
    CHECK(result.error().path ==
          Path{"regolith", "filterDefinitions", "broken", "runWith"});
    CHECK_THAT(result.error().message, ContainsSubstring("cobol"));
  }
  SECTION("a remote filter without a version")
  {
    expect_error(parse_config(config_with_definition(
                     "remote", R"json({ "url": "github.com/a/b" })json"_json)),
                 {"regolith", "filterDefinitions", "remote"},
                 missing("version"));
  }
  SECTION("a bad async child")
  {
    expect_error(parse_config(config_with_filters(R"json([
                   { "asyncFilters": [{ "filter": 1 }] }
                 ])json"_json)),
                 {"regolith", "profiles", "default", "filters", "0",
                  "asyncFilters", "0", "filter"},
                 expected("string"));
  }
}

TEST_CASE("parse_config: wrong-typed optional values (Go leniency)",
          "[config][go-leniency]")
{
  // These mirror `x, _ := obj["x"].(T)` and `if x, ok := ...; ok` patterns in
  // Go's ConfigFromObject / RegolithProjectFromObject, which skip a value of
  // the wrong type instead of failing.
  const auto bad = GENERATE(json(1), json(true), json::array(), json::object(),
                            json(nullptr));
  CAPTURE(bad);

  SECTION("packs.behaviorPack and packs.resourcePack")
  {
    json document = minimal_config();
    document["packs"] = json{{"behaviorPack", bad}, {"resourcePack", bad}};
    const auto result = parse_config(document);
    CHECK(value_of(result).packs.behavior_pack.value_or("").empty());
    CHECK(value_of(result).packs.resource_pack.value_or("").empty());
  }
  SECTION("watchPaths that is not an array is ignored")
  {
    if (!bad.is_array()) {
      json document = minimal_config();
      document["regolith"]["watchPaths"] = bad;
      const auto result = parse_config(document);
      CHECK(value_of(result).watch_paths.empty());
    }
  }
  SECTION("filterDefinitions that is not an object is ignored")
  {
    if (!bad.is_object()) {
      json document = minimal_config();
      document["regolith"]["filterDefinitions"] = bad;
      const auto result = parse_config(document);
      CHECK(value_of(result).filter_definitions.empty());
    }
  }
}

TEST_CASE("parse_config: filters are checked against filterDefinitions",
          "[config][references]")
{
  SECTION("a known filter")
  {
    json document =
        config_with_filters(R"json([{ "filter": "tool" }])json"_json);
    document["regolith"]["filterDefinitions"] =
        R"json({ "tool": { "runWith": "exe", "exe": "t" } })json"_json;
    CHECK(parse_config(document).has_value());
  }
  SECTION("an unknown filter")
  {
    json document = config_with_filters(R"json([
      { "filter": "tool" }, { "filter": "ghost" }
    ])json"_json);
    document["regolith"]["filterDefinitions"] =
        R"json({ "tool": { "runWith": "exe", "exe": "t" } })json"_json;
    expect_error(parse_config(document),
                 {"regolith", "profiles", "default", "filters", "1", "filter"},
                 "unknown filter \"ghost\"");
  }
  SECTION("ids are case sensitive")
  {
    json document =
        config_with_filters(R"json([{ "filter": "Tool" }])json"_json);
    document["regolith"]["filterDefinitions"] =
        R"json({ "tool": { "runWith": "exe", "exe": "t" } })json"_json;
    expect_error(parse_config(document),
                 {"regolith", "profiles", "default", "filters", "0", "filter"},
                 "unknown filter \"Tool\"");
  }
  SECTION("an unknown filter inside an async group")
  {
    json document = config_with_filters(R"json([{ "asyncFilters": [
      { "filter": "a" }, { "filter": "b" }, { "filter": "missing" }
    ] }])json"_json);
    document["regolith"]["filterDefinitions"] = R"json({
      "a": { "runWith": "exe", "exe": "a" },
      "b": { "runWith": "exe", "exe": "b" }
    })json"_json;
    expect_error(parse_config(document),
                 {"regolith", "profiles", "default", "filters", "0",
                  "asyncFilters", "2", "filter"},
                 "unknown filter \"missing\"");
  }
  SECTION("a filter named like a profile is still unknown")
  {
    json const document =
        config_with_filters(R"json([{ "filter": "default" }])json"_json);
    expect_error(parse_config(document),
                 {"regolith", "profiles", "default", "filters", "0", "filter"},
                 "unknown filter \"default\"");
  }
  SECTION("unused definitions are fine")
  {
    json document = minimal_config();
    document["regolith"]["filterDefinitions"] =
        R"json({ "unused": { "runWith": "exe", "exe": "t" } })json"_json;
    CHECK(parse_config(document).has_value());
  }
}

namespace {

// Parses a config with the given profiles (which must succeed) and runs
// check_profile on `root`.
tl::expected<void, ConfigError> check_root(const json &profiles,
                                           std::string_view root)
{
  const auto config = parse_config(config_with_profiles(profiles));
  REQUIRE(config);
  return check_profile(*config, root);
}

void expect_circular(const json &profiles, std::string_view root)
{
  const auto result = check_root(profiles, root);
  REQUIRE_FALSE(result);
  CHECK_THAT(result.error().message,
             ContainsSubstring("circular profile reference"));
}

} // namespace

TEST_CASE("check_profile: profile calls must name existing profiles",
          "[config][references]")
{
  SECTION("a known profile")
  {
    CHECK(check_root(json{{"a", calling({"b"})}, {"b", minimal_profile()}}, "a")
              .has_value());
  }
  SECTION("the profile itself must exist")
  {
    expect_error(check_root(json{{"a", minimal_profile()}}, "ghost"),
                 {"regolith", "profiles"}, "unknown profile \"ghost\"");
  }
  SECTION("an unknown profile")
  {
    expect_error(check_root(json{{"a", calling({"ghost"})}}, "a"),
                 {"regolith", "profiles", "a", "filters", "0", "profile"},
                 "unknown profile \"ghost\"");
  }
  SECTION("an unknown profile at a later index")
  {
    expect_error(check_root(json{{"a", calling({"b", "ghost"})},
                                 {"b", minimal_profile()}},
                            "a"),
                 {"regolith", "profiles", "a", "filters", "1", "profile"},
                 "unknown profile \"ghost\"");
  }
  SECTION("an unknown profile inside an async group")
  {
    json profile = minimal_profile();
    profile["filters"] = R"json([
      { "asyncFilters": [{ "profile": "ghost" }] }
    ])json"_json;
    expect_error(check_root(json{{"a", profile}}, "a"),
                 {"regolith", "profiles", "a", "filters", "0", "asyncFilters",
                  "0", "profile"},
                 "unknown profile \"ghost\"");
  }
  SECTION("an unknown profile in a nested profile")
  {
    expect_error(
        check_root(json{{"a", calling({"b"})}, {"b", calling({"ghost"})}}, "a"),
        {"regolith", "profiles", "b", "filters", "0", "profile"},
        "unknown profile \"ghost\"");
  }
  SECTION("parse_config itself accepts unknown profile calls, like Regolith")
  {
    CHECK(parse_config(config_with_profiles(json{{"a", calling({"ghost"})}}))
              .has_value());
  }
  SECTION("the same profile can be called many times")
  {
    CHECK(check_root(
              json{{"a", calling({"b", "b", "b"})}, {"b", minimal_profile()}},
              "a")
              .has_value());
  }
  SECTION("a diamond is not a cycle")
  {
    CHECK(check_root(json{{"top", calling({"left", "right"})},
                          {"left", calling({"base"})},
                          {"right", calling({"base"})},
                          {"base", minimal_profile()}},
                     "top")
              .has_value());
  }
  SECTION("a deep chain is not a cycle")
  {
    CHECK(check_root(json{{"a", calling({"b"})},
                          {"b", calling({"c"})},
                          {"c", calling({"d"})},
                          {"d", minimal_profile()}},
                     "a")
              .has_value());
  }
}

TEST_CASE("check_profile: circular profile references are rejected",
          "[config][references]")
{
  SECTION("a profile calling itself")
  {
    expect_error(check_root(json{{"a", calling({"a"})}}, "a"),
                 {"regolith", "profiles", "a", "filters", "0", "profile"},
                 "circular profile reference: a -> a");
  }
  SECTION("two profiles calling each other")
  {
    expect_error(
        check_root(json{{"a", calling({"b"})}, {"b", calling({"a"})}}, "a"),
        {"regolith", "profiles", "b", "filters", "0", "profile"},
        "circular profile reference: a -> b -> a");
  }
  SECTION("a three-profile loop")
  {
    expect_circular(json{{"one", calling({"two"})},
                         {"two", calling({"three"})},
                         {"three", calling({"one"})}},
                    "two");
  }
  SECTION("a loop reachable from the profile being run")
  {
    expect_circular(json{{"entry", calling({"a"})},
                         {"a", calling({"b"})},
                         {"b", calling({"a"})}},
                    "entry");
  }
  SECTION("a loop elsewhere does not affect other profiles, like Regolith")
  {
    const json profiles = json{{"ok", calling({"leaf"})},
                               {"leaf", minimal_profile()},
                               {"x", calling({"leaf", "y"})},
                               {"y", calling({"x"})}};
    CHECK(check_root(profiles, "ok").has_value());
    expect_circular(profiles, "x");
  }
  SECTION("a cycle through an async group")
  {
    json a = minimal_profile();
    a["filters"] = R"json([{ "asyncFilters": [{ "profile": "b" }] }])json"_json;
    expect_circular(json{{"a", a}, {"b", calling({"a"})}}, "b");
  }
}

TEST_CASE("parse_config: error to_string uses the full path",
          "[config][errors]")
{
  const auto result = parse_config(
      config_with_filters(R"json([{ "filter": "ghost" }])json"_json));
  REQUIRE_FALSE(result);
  CHECK(result.error().to_string() ==
        "regolith -> profiles -> default -> filters -> 0 -> filter: "
        "unknown filter \"ghost\"");
}

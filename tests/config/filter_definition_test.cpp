#include "../support/helpers.hpp"
#include "basalt/filter_definition.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <string>
#include <variant>

using Catch::Matchers::ContainsSubstring;
using namespace basalt;
using namespace basalt::test;

namespace {

// Parses `json` as the definition "demo" and returns the held filter kind.
template <typename Kind>
Kind parse_as(const json &definition)
{
  const auto result = parse_filter_definition("demo", definition);
  const auto &parsed = value_of(result);
  REQUIRE(std::holds_alternative<Kind>(parsed.kind));
  return std::get<Kind>(parsed.kind);
}

} // namespace

TEST_CASE("filter definitions: every runWith selects its filter kind",
          "[filter_definition]")
{
  SECTION("python")
  {
    const auto filter = parse_as<PythonFilter>(
        R"json({ "runWith": "python", "script": "f.py" })json"_json);
    CHECK(filter.script == "f.py");
    CHECK(filter.venv_slot == 0);
    CHECK_FALSE(filter.requirements.has_value());
  }
  SECTION("python with every option")
  {
    const auto filter = parse_as<PythonFilter>(R"json({
      "runWith": "python", "script": "f.py", "venvSlot": 3,
      "requirements": "reqs.txt"
    })json"_json);
    CHECK(filter.venv_slot == 3);
    CHECK(filter.requirements == "reqs.txt");
  }
  SECTION("nodejs")
  {
    const auto filter = parse_as<NodeJsFilter>(R"json({
      "runWith": "nodejs", "script": "f.js", "requirements": "package.json"
    })json"_json);
    CHECK(filter.script == "f.js");
    CHECK(filter.requirements == "package.json");
  }
  SECTION("nodejs without requirements")
  {
    const auto filter = parse_as<NodeJsFilter>(
        R"json({ "runWith": "nodejs", "script": "f.js" })json"_json);
    CHECK_FALSE(filter.requirements.has_value());
  }
  SECTION("bun")
  {
    CHECK(parse_as<BunFilter>(
              R"json({ "runWith": "bun", "script": "f.ts" })json"_json)
              .script == "f.ts");
  }
  SECTION("deno")
  {
    CHECK(parse_as<DenoFilter>(
              R"json({ "runWith": "deno", "script": "f.ts" })json"_json)
              .script == "f.ts");
  }
  SECTION("java")
  {
    CHECK(parse_as<JavaFilter>(
              R"json({ "runWith": "java", "script": "f.jar" })json"_json)
              .path == "f.jar");
  }
  SECTION("dotnet")
  {
    CHECK(parse_as<DotnetFilter>(
              R"json({ "runWith": "dotnet", "path": "f.dll" })json"_json)
              .path == "f.dll");
  }
  SECTION("nim")
  {
    const auto filter = parse_as<NimFilter>(R"json({
      "runWith": "nim", "script": "f.nim", "requirements": "f.nimble"
    })json"_json);
    CHECK(filter.script == "f.nim");
    CHECK(filter.requirements == "f.nimble");
  }
  SECTION("shell")
  {
    CHECK(parse_as<ShellFilter>(
              R"json({ "runWith": "shell", "command": "echo hi" })json"_json)
              .command == "echo hi");
  }
  SECTION("exe")
  {
    CHECK(parse_as<ExeFilter>(
              R"json({ "runWith": "exe", "exe": "./bin/tool" })json"_json)
              .exe == "./bin/tool");
  }
}

TEST_CASE("filter definitions: java takes `path`, `script` is the old name",
          "[filter_definition][go-quirk]")
{
  // Go's JavaFilterDefinitionFromObject reads "path" and falls back to the
  // deprecated "script" key.
  SECTION("path")
  {
    CHECK(parse_as<JavaFilter>(
              R"json({ "runWith": "java", "path": "f.jar" })json"_json)
              .path == "f.jar");
  }
  SECTION("path wins over script")
  {
    CHECK(parse_as<JavaFilter>(R"json({
      "runWith": "java", "path": "new.jar", "script": "old.jar"
    })json"_json)
              .path == "new.jar");
  }
  SECTION("neither is an error naming `path`")
  {
    expect_error(parse_filter_definition(
                     "demo", R"json({ "runWith": "java" })json"_json),
                 {}, missing("path"));
  }
  SECTION("wrong-typed path")
  {
    expect_error(
        parse_filter_definition(
            "demo", R"json({ "runWith": "java", "path": 1 })json"_json),
        {"path"}, expected("string"));
  }
  SECTION("wrong-typed fallback script")
  {
    expect_error(
        parse_filter_definition(
            "demo", R"json({ "runWith": "java", "script": 1 })json"_json),
        {"script"}, expected("string"));
  }
}

TEST_CASE("filter definitions: remote filters", "[filter_definition]")
{
  SECTION("no runWith means a remote filter")
  {
    const auto filter = parse_as<RemoteFilter>(R"json({
      "url": "github.com/me/filters", "version": "1.2.3"
    })json"_json);
    CHECK(filter.url == "github.com/me/filters");
    CHECK(filter.version == "1.2.3");
  }
  SECTION("an empty runWith is the same as none")
  {
    const auto filter = parse_as<RemoteFilter>(
        R"json({ "runWith": "", "version": "latest" })json"_json);
    CHECK(filter.version == "latest");
  }
  SECTION("url defaults to the standard library")
  {
    const auto filter =
        parse_as<RemoteFilter>(R"json({ "version": "HEAD" })json"_json);
    CHECK(filter.url == "github.com/Bedrock-OSS/regolith-filters");
    CHECK(filter.url == default_remote_filter_url);
  }
  SECTION("version is kept verbatim")
  {
    const auto *const version =
        GENERATE("1.0.0", "latest", "HEAD", "TEST_TAG_1",
                 "0c129227eb90e2f10a038755e4756fdd47e765e6");
    const auto filter = parse_as<RemoteFilter>(json{{"version", version}});
    CHECK(filter.version == version);
  }
  SECTION("version is required")
  {
    expect_error(
        parse_filter_definition("demo", R"json({ "url": "x" })json"_json), {},
        missing("version"));
  }
  SECTION("version must be a string")
  {
    expect_error(
        parse_filter_definition("demo", R"json({ "version": 1 })json"_json),
        {"version"}, expected("string"));
  }
  SECTION("a wrong-typed url silently falls back to the default (Go)")
  {
    const auto filter = parse_as<RemoteFilter>(
        R"json({ "url": 5, "version": "1.0.0" })json"_json);
    CHECK(filter.url == default_remote_filter_url);
  }
}

TEST_CASE("filter definitions: id and raw are kept", "[filter_definition]")
{
  const auto definition = R"json({
    "runWith": "shell", "command": "ls", "unknownKey": [1, 2]
  })json"_json;
  const auto result = parse_filter_definition("my_filter", definition);
  const auto &parsed = value_of(result);
  CHECK(parsed.id == "my_filter");
  CHECK(parsed.raw == definition);
}

TEST_CASE("filter definitions: invalid runWith", "[filter_definition]")
{
  SECTION("unknown values are errors")
  {
    const auto *const bad = GENERATE("ruby", "Python", "NODEJS", "node",
                                     "python3", " python", "remote");
    const auto result = parse_filter_definition(
        "demo", json{{"runWith", bad}, {"script", "x"}});
    REQUIRE_FALSE(result);
    CHECK(result.error().path == Path{"runWith"});
    CHECK_THAT(result.error().message, ContainsSubstring(bad));
  }
  SECTION("a non-string runWith is treated as absent (Go)")
  {
    // Go reads `obj["runWith"].(string)` and discards a failed assertion, so a
    // number or null selects the remote filter definition.
    const auto run_with = GENERATE(json(5), json(nullptr), json(true),
                                   json::array({"python"}), json::object());
    const auto filter = parse_as<RemoteFilter>(
        json{{"runWith", run_with}, {"version", "1.0.0"}});
    CHECK(filter.version == "1.0.0");
  }
}

TEST_CASE("filter definitions: required fields per kind", "[filter_definition]")
{
  struct Case {
    const char *run_with;
    const char *key;
  };

  const auto c = GENERATE(Case{"python", "script"}, Case{"nodejs", "script"},
                          Case{"bun", "script"}, Case{"deno", "script"},
                          Case{"nim", "script"}, Case{"dotnet", "path"},
                          Case{"shell", "command"}, Case{"exe", "exe"});
  CAPTURE(c.run_with, c.key);

  SECTION("missing")
  {
    expect_error(parse_filter_definition("demo", json{{"runWith", c.run_with}}),
                 {}, missing(c.key));
  }
  SECTION("wrong type")
  {
    const auto bad = GENERATE(json(1), json(true), json::array(),
                              json::object(), json(nullptr));
    expect_error(parse_filter_definition(
                     "demo", json{{"runWith", c.run_with}, {c.key, bad}}),
                 {c.key}, expected("string"));
  }
}

TEST_CASE("filter definitions: optional fields", "[filter_definition]")
{
  SECTION("requirements must be a string where it is read")
  {
    const auto *const run_with = GENERATE("python", "nodejs", "nim");
    CAPTURE(run_with);
    expect_error(parse_filter_definition("demo", json{{"runWith", run_with},
                                                      {"script", "f"},
                                                      {"requirements", 7}}),
                 {"requirements"}, expected("string"));
  }
  SECTION("bun and deno do not read requirements")
  {
    const auto *const run_with = GENERATE("bun", "deno");
    const auto result = parse_filter_definition(
        "demo",
        json{{"runWith", run_with}, {"script", "f"}, {"requirements", 7}});
    CHECK(result.has_value());
  }
}

TEST_CASE("filter definitions: venvSlot", "[filter_definition]")
{
  const auto parse_slot = [](const json &slot) {
    return parse_as<PythonFilter>(
               json{{"runWith", "python"}, {"script", "f"}, {"venvSlot", slot}})
        .venv_slot;
  };

  CHECK(parse_slot(0) == 0);
  CHECK(parse_slot(5) == 5);
  CHECK(parse_slot(-1) == -1);
  // Go decodes JSON numbers as float64 and truncates with int().
  CHECK(parse_slot(2.0) == 2);
  CHECK(parse_slot(2.9) == 2);
  // A non-number is silently 0 in Go.
  CHECK(parse_slot("3") == 0);
  CHECK(parse_slot(true) == 0);
  CHECK(parse_slot(nullptr) == 0);
}

TEST_CASE("filter definitions: parse errors are relative to the definition",
          "[filter_definition]")
{
  SECTION("a definition must be an object")
  {
    const auto bad = GENERATE(json("x"), json(1), json::array(), json(nullptr));
    expect_error(parse_filter_definition("demo", bad), {}, expected("object"));
  }
  SECTION("an empty object is a remote filter missing its version")
  {
    expect_error(parse_filter_definition("demo", json::object()), {},
                 missing("version"));
  }
}

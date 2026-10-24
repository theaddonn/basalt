#include "../support/helpers.hpp"
#include "basalt/profile.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include <string>
#include <variant>

using namespace basalt;
using namespace basalt::test;

namespace {

// A profile with the given filters and a valid export.
json profile_with_filters(const json &filters)
{
    json profile = minimal_profile();
    profile["filters"] = filters;
    return profile;
}

// A profile with the given `export` value and no filters.
json profile_with_export(const json &export_value)
{
    json profile = minimal_profile();
    profile["export"] = export_value;
    return profile;
}

// Parses a profile whose only filter is `entry` and returns that entry.
FilterEntry parse_entry(const json &entry)
{
    const auto result =
        parse_profile(profile_with_filters(json::array({entry})));
    const auto &profile = value_of(result);
    REQUIRE(profile.filters.size() == 1);
    return profile.filters.front();
}

template <typename Kind>
const Kind &kind_of(const FilterEntry &entry)
{
    REQUIRE(std::holds_alternative<Kind>(entry.kind));
    return std::get<Kind>(entry.kind);
}

const Path filter_0 = {"filters", "0"};

} // namespace

TEST_CASE("profile: minimal profile", "[profile]")
{
    const auto result = parse_profile(minimal_profile());
    const auto &profile = value_of(result);
    CHECK(profile.filters.empty());
    REQUIRE(profile.exports.size() == 1);
    CHECK(profile.exports[0].target == "local");
    CHECK(profile.pre_shell.empty());
    CHECK(profile.post_shell.empty());
}

TEST_CASE("profile: unknown keys are ignored", "[profile]")
{
    json profile = minimal_profile();
    profile["comment"] = "hello";
    profile["somethingNew"] = json::object();
    CHECK(parse_profile(profile).has_value());
}

TEST_CASE("profile: a profile must be an object", "[profile]")
{
    const auto bad = GENERATE(json("x"), json(1), json::array(), json(nullptr));
    expect_error(parse_profile(bad), {}, expected("object"));
}

TEST_CASE("profile: filters", "[profile]")
{
    SECTION("required")
    {
        expect_error(parse_profile(
                         R"json({ "export": { "target": "local" } })json"_json),
                     {}, missing("filters"));
    }
    SECTION("must be an array")
    {
        const auto bad =
            GENERATE(json("x"), json(1), json::object(), json(nullptr));
        json profile = minimal_profile();
        profile["filters"] = bad;
        expect_error(parse_profile(profile), {"filters"}, expected("array"));
    }
    SECTION("entries must be objects")
    {
        const auto bad =
            GENERATE(json("x"), json(1), json::array(), json(nullptr));
        expect_error(parse_profile(profile_with_filters(json::array(
                         {R"json({"filter": "a"})json"_json, bad}))),
                     {"filters", "1"}, expected("object"));
    }
    SECTION("order is preserved")
    {
        const auto result = parse_profile(profile_with_filters(R"json([
      { "filter": "a" }, { "profile": "b" }, { "filter": "c" }
    ])json"_json));
        const auto &profile = value_of(result);
        REQUIRE(profile.filters.size() == 3);
        CHECK(kind_of<FilterCall>(profile.filters[0]).filter == "a");
        CHECK(kind_of<ProfileCall>(profile.filters[1]).profile == "b");
        CHECK(kind_of<FilterCall>(profile.filters[2]).filter == "c");
    }
}

TEST_CASE("profile: filter entry dispatch", "[profile]")
{
    SECTION("filter")
    {
        const auto entry =
            parse_entry(R"json({ "filter": "my_filter" })json"_json);
        CHECK(kind_of<FilterCall>(entry).filter == "my_filter");
    }
    SECTION("profile")
    {
        const auto entry =
            parse_entry(R"json({ "profile": "other" })json"_json);
        CHECK(kind_of<ProfileCall>(entry).profile == "other");
    }
    SECTION("asyncFilters")
    {
        const auto entry = parse_entry(R"json({ "asyncFilters": [
      { "filter": "a" }, { "profile": "b" }, { "filter": "c" }
    ] })json"_json);
        const auto &async = kind_of<AsyncCall>(entry);
        REQUIRE(async.filters.size() == 3);
        CHECK(kind_of<FilterCall>(async.filters[0]).filter == "a");
        CHECK(kind_of<ProfileCall>(async.filters[1]).profile == "b");
        CHECK(kind_of<FilterCall>(async.filters[2]).filter == "c");
    }
    SECTION("an empty asyncFilters array is allowed")
    {
        const auto entry =
            parse_entry(R"json({ "asyncFilters": [] })json"_json);
        CHECK(kind_of<AsyncCall>(entry).filters.empty());
    }
    SECTION("profile wins over asyncFilters and filter")
    {
        const auto entry = parse_entry(R"json({
      "profile": "p", "asyncFilters": [], "filter": "f"
    })json"_json);
        CHECK(kind_of<ProfileCall>(entry).profile == "p");
    }
    SECTION("asyncFilters wins over filter")
    {
        const auto entry = parse_entry(R"json({
      "asyncFilters": [{ "filter": "a" }], "filter": "f"
    })json"_json);
        CHECK(kind_of<AsyncCall>(entry).filters.size() == 1);
    }
    SECTION("a non-string `profile` is skipped, like Go does")
    {
        const auto entry =
            parse_entry(R"json({ "profile": 5, "filter": "f" })json"_json);
        CHECK(kind_of<FilterCall>(entry).filter == "f");
    }
    SECTION("an entry with none of them needs a `filter`")
    {
        expect_error(parse_profile(profile_with_filters(
                         R"json([{ "description": "oops" }])json"_json)),
                     filter_0, missing("filter"));
    }
    SECTION("`filter` must be a string")
    {
        const auto bad = GENERATE(json(1), json(true), json::array(),
                                  json::object(), json(nullptr));
        expect_error(parse_profile(profile_with_filters(
                         json::array({json{{"filter", bad}}}))),
                     {"filters", "0", "filter"}, expected("string"));
    }
}

TEST_CASE("profile: async filters", "[profile]")
{
    const Path async_path = {"filters", "0", "asyncFilters"};

    SECTION("asyncFilters must be an array")
    {
        const auto bad =
            GENERATE(json("x"), json(1), json::object(), json(nullptr));
        expect_error(parse_profile(profile_with_filters(
                         json::array({json{{"asyncFilters", bad}}}))),
                     async_path, expected("array"));
    }
    SECTION("children must be objects")
    {
        expect_error(
            parse_profile(profile_with_filters(
                R"json([{ "asyncFilters": [{ "filter": "a" }, 7] }])json"_json)),
            {"filters", "0", "asyncFilters", "1"}, expected("object"));
    }
    SECTION("children are checked like top-level entries")
    {
        expect_error(
            parse_profile(profile_with_filters(
                R"json([{ "asyncFilters": [{ "filter": "a" }, {}] }])json"_json)),
            {"filters", "0", "asyncFilters", "1"}, missing("filter"));
    }
    SECTION("a child's `filter` must be a string")
    {
        expect_error(
            parse_profile(profile_with_filters(
                R"json([{ "asyncFilters": [{ "filter": 3 }] }])json"_json)),
            {"filters", "0", "asyncFilters", "0", "filter"},
            expected("string"));
    }
    SECTION("async filters cannot be nested")
    {
        const auto result = parse_profile(profile_with_filters(R"json([{
      "asyncFilters": [
        { "filter": "a" },
        { "filter": "b" },
        { "asyncFilters": [{ "filter": "c" }] }
      ]
    }])json"_json));
        REQUIRE_FALSE(result);
        const Path &path = result.error().path;
        REQUIRE(path.size() >= 4);
        CHECK(path[0] == "filters");
        CHECK(path[1] == "0");
        CHECK(path[2] == "asyncFilters");
        CHECK(path[3] == "2");
    }
    SECTION("a profile call is fine inside an async group")
    {
        const auto entry = parse_entry(
            R"json({ "asyncFilters": [{ "profile": "p" }] })json"_json);
        CHECK(kind_of<ProfileCall>(kind_of<AsyncCall>(entry).filters[0])
                  .profile == "p");
    }
}

TEST_CASE("profile: shared filter options default", "[profile]")
{
    const auto entry_json = GENERATE(R"json({ "filter": "f" })json"_json,
                                     R"json({ "profile": "p" })json"_json,
                                     R"json({ "asyncFilters": [] })json"_json);
    const auto options = parse_entry(entry_json).options;
    CHECK(options.description.empty());
    CHECK_FALSE(options.disabled);
    CHECK(options.arguments.empty());
    CHECK(options.settings == json::object());
    CHECK(options.settings.is_object());
    CHECK(options.when.empty());
    CHECK(options.extra_arguments == ExtraArgumentsMode::ignore);
}

TEST_CASE("profile: shared filter options", "[profile]")
{
    const auto base = GENERATE(R"json({ "filter": "f" })json"_json,
                               R"json({ "profile": "p" })json"_json);
    json entry = base;
    entry["description"] = "does a thing";
    entry["disabled"] = true;
    entry["arguments"] = json::array({"--flag", "value"});
    entry["settings"] =
        R"json({ "a": 1, "nested": { "b": [true, null] } })json"_json;
    entry["when"] = "os == 'linux'";
    entry["extraArguments"] = "append";

    const auto options = parse_entry(entry).options;
    CHECK(options.description == "does a thing");
    CHECK(options.disabled);
    CHECK(options.arguments == std::vector<std::string>{"--flag", "value"});
    CHECK(options.settings ==
          R"json({ "a": 1, "nested": { "b": [true, null] } })json"_json);
    CHECK(options.when == "os == 'linux'");
    CHECK(options.extra_arguments == ExtraArgumentsMode::append);
}

TEST_CASE("profile: an async group ignores the shared options",
          "[profile][go-quirk]")
{
    // Go's AsyncFilterFromObject never calls filterFromObject, so options
    // written next to `asyncFilters` are dropped (even invalid ones).
    const auto entry = parse_entry(R"json({
    "asyncFilters": [], "description": "x", "disabled": true,
    "arguments": ["a"], "settings": { "k": 1 }, "when": "false",
    "extraArguments": "bogus"
  })json"_json);
    CHECK(entry.options.description.empty());
    CHECK_FALSE(entry.options.disabled);
    CHECK(entry.options.arguments.empty());
    CHECK(entry.options.settings == json::object());
    CHECK(entry.options.when.empty());
    CHECK(entry.options.extra_arguments == ExtraArgumentsMode::ignore);
}

TEST_CASE("profile: extraArguments", "[profile]")
{
    SECTION("accepted values")
    {
        struct Case {
            const char *text;
            ExtraArgumentsMode mode;
        };

        const auto c = GENERATE(Case{"ignore", ExtraArgumentsMode::ignore},
                                Case{"override", ExtraArgumentsMode::override},
                                Case{"append", ExtraArgumentsMode::append});
        CAPTURE(c.text);
        const auto entry =
            parse_entry(json{{"filter", "f"}, {"extraArguments", c.text}});
        CHECK(entry.options.extra_arguments == c.mode);
    }
    SECTION("anything else is rejected at parse time")
    {
        // Go also accepts "" as "ignore"; that is not pinned down here.
        const auto *const bad =
            GENERATE("replace", "Append", "IGNORE", " append");
        CAPTURE(bad);
        expect_error_at(parse_profile(profile_with_filters(json::array(
                            {json{{"filter", "f"}, {"extraArguments", bad}}}))),
                        {"filters", "0", "extraArguments"});
    }
    SECTION("must be a string")
    {
        const auto bad = GENERATE(json(1), json(true), json::array(),
                                  json::object(), json(nullptr));
        expect_error(parse_profile(profile_with_filters(json::array(
                         {json{{"filter", "f"}, {"extraArguments", bad}}}))),
                     {"filters", "0", "extraArguments"}, expected("string"));
    }
    SECTION("is checked on profile calls and async groups too")
    {
        expect_error_at(parse_profile(profile_with_filters(R"json([
      { "profile": "p", "extraArguments": "nope" }
    ])json"_json)),
                        {"filters", "0", "extraArguments"});
        expect_error_at(
            parse_profile(profile_with_filters(R"json([
      { "asyncFilters": [{ "filter": "f", "extraArguments": "nope" }] }
    ])json"_json)),
            {"filters", "0", "asyncFilters", "0", "extraArguments"});
    }
}

TEST_CASE("profile: arguments", "[profile]")
{
    SECTION("elements must be strings")
    {
        const auto bad = GENERATE(json(1), json(true), json(nullptr),
                                  json::array(), json::object());
        expect_error(
            parse_profile(profile_with_filters(json::array({json{
                {"filter", "f"}, {"arguments", json::array({"ok", bad})}}}))),
            {"filters", "0", "arguments", "1"}, expected("string"));
    }
    SECTION("an empty array is fine")
    {
        CHECK(parse_entry(R"json({ "filter": "f", "arguments": [] })json"_json)
                  .options.arguments.empty());
    }
}

TEST_CASE("profile: wrong-typed optional fields fall back to defaults (Go)",
          "[profile][go-leniency]")
{
    // Go does `x, _ := obj["x"].(T)`, so a value of the wrong type is silently
    // replaced by the zero value instead of being reported.
    const auto bad = GENERATE(json(1), json(true), json::array({"a"}),
                              json::object(), json(nullptr));
    CAPTURE(bad);

    SECTION("description")
    {
        if (!bad.is_string()) {
            CHECK(parse_entry(json{{"filter", "f"}, {"description", bad}})
                      .options.description.empty());
        }
    }
    SECTION("disabled")
    {
        if (!bad.is_boolean()) {
            CHECK_FALSE(parse_entry(json{{"filter", "f"}, {"disabled", bad}})
                            .options.disabled);
        }
    }
    SECTION("when")
    {
        CHECK(parse_entry(json{{"filter", "f"}, {"when", bad}})
                  .options.when.empty());
    }
    SECTION("settings")
    {
        if (!bad.is_object()) {
            CHECK(parse_entry(json{{"filter", "f"}, {"settings", bad}})
                      .options.settings == json::object());
        }
    }
    SECTION("arguments that are not an array")
    {
        if (!bad.is_array()) {
            CHECK(parse_entry(json{{"filter", "f"}, {"arguments", bad}})
                      .options.arguments.empty());
        }
    }
}

TEST_CASE("profile: export", "[profile]")
{
    SECTION("required")
    {
        expect_error(parse_profile(R"json({ "filters": [] })json"_json), {},
                     missing("export"));
    }
    SECTION("must be an object or an array")
    {
        const auto bad =
            GENERATE(json("local"), json(1), json(true), json(nullptr));
        expect_error(parse_profile(profile_with_export(bad)), {"export"},
                     expected("object or array"));
    }
    SECTION("an empty array is rejected")
    {
        expect_error_at(parse_profile(profile_with_export(json::array())),
                        {"export"});
    }
    SECTION("array elements must be objects")
    {
        expect_error(parse_profile(profile_with_export(
                         json::array({json{{"target", "local"}}, "oops"}))),
                     {"export", "1"}, expected("object"));
    }
    SECTION("target is required (single object)")
    {
        expect_error(parse_profile(profile_with_export(json::object())),
                     {"export"}, missing("target"));
    }
    SECTION("target is required (array element)")
    {
        expect_error(parse_profile(profile_with_export(json::array(
                         {json{{"target", "local"}}, json::object()}))),
                     {"export", "1"}, missing("target"));
    }
    SECTION("target must be a string (single object)")
    {
        expect_error(parse_profile(profile_with_export(json{{"target", 4}})),
                     {"export", "target"}, expected("string"));
    }
    SECTION("target must be a string (array element)")
    {
        expect_error(parse_profile(profile_with_export(
                         json::array({json{{"target", false}}}))),
                     {"export", "0", "target"}, expected("string"));
    }
    SECTION("target values are not validated")
    {
        const auto *const target = GENERATE("development", "local", "exact",
                                            "world", "none", "something-new");
        const auto result =
            parse_profile(profile_with_export(json{{"target", target}}));
        CHECK(value_of(result).exports.at(0).target == target);
    }
}

TEST_CASE("profile: export target fields", "[profile]")
{
    SECTION("every field")
    {
        const auto result = parse_profile(profile_with_export(R"json({
      "target": "exact",
      "rpPath": "out/RP", "bpPath": "out/BP",
      "rpName": "project.name + '_RP'", "bpName": "project.name + '_BP'",
      "worldName": "My World", "worldPath": "worlds/mine",
      "build": "preview", "readOnly": true
    })json"_json));
        const auto &profile = value_of(result);
        REQUIRE(profile.exports.size() == 1);
        const auto &target = profile.exports[0];
        CHECK(target.target == "exact");
        CHECK(target.rp_path == "out/RP");
        CHECK(target.bp_path == "out/BP");
        CHECK(target.rp_name == "project.name + '_RP'");
        CHECK(target.bp_name == "project.name + '_BP'");
        CHECK(target.world_name == "My World");
        CHECK(target.world_path == "worlds/mine");
        CHECK(target.build == "preview");
        CHECK(target.read_only);
    }
    SECTION("only target is required; the rest default")
    {
        const auto result =
            parse_profile(profile_with_export(json{{"target", "development"}}));
        const auto &target = value_of(result).exports.at(0);
        CHECK(target.rp_path.empty());
        CHECK(target.bp_path.empty());
        CHECK(target.rp_name.empty());
        CHECK(target.bp_name.empty());
        CHECK(target.world_name.empty());
        CHECK(target.world_path.empty());
        CHECK(target.build.empty());
        CHECK_FALSE(target.read_only);
    }
    SECTION("multiple targets keep their order")
    {
        const auto result = parse_profile(profile_with_export(R"json([
      { "target": "exact", "rpPath": "a/RP", "bpPath": "a/BP" },
      { "target": "development", "build": "education" },
      { "target": "none" }
    ])json"_json));
        const auto &exports = value_of(result).exports;
        REQUIRE(exports.size() == 3);
        CHECK(exports[0].target == "exact");
        CHECK(exports[0].rp_path == "a/RP");
        CHECK(exports[1].target == "development");
        CHECK(exports[1].build == "education");
        CHECK(exports[2].target == "none");
    }
    SECTION("a one-element array is the same as an object")
    {
        const auto from_array = parse_profile(profile_with_export(
            R"json([{ "target": "local", "readOnly": true }])json"_json));
        const auto from_object = parse_profile(profile_with_export(
            R"json({ "target": "local", "readOnly": true })json"_json));
        REQUIRE(value_of(from_array).exports.size() == 1);
        CHECK(value_of(from_array).exports[0].target ==
              value_of(from_object).exports[0].target);
        CHECK(value_of(from_array).exports[0].read_only ==
              value_of(from_object).exports[0].read_only);
    }
}

TEST_CASE("profile: wrong-typed export fields fall back to defaults (Go)",
          "[profile][go-leniency]")
{
    const auto bad = GENERATE(json(1), json(false), json::array({"x"}),
                              json::object(), json(nullptr));
    CAPTURE(bad);

    const auto parse_target = [&](const char *key) {
        json target = {{"target", "exact"}};
        target[key] = bad;
        const auto result = parse_profile(profile_with_export(target));
        return value_of(result).exports.at(0);
    };

    SECTION("string fields become empty")
    {
        const auto *const key = GENERATE("rpPath", "bpPath", "rpName", "bpName",
                                         "worldName", "worldPath", "build");
        CAPTURE(key);
        const auto target = parse_target(key);
        CHECK(target.rp_path.empty());
        CHECK(target.bp_path.empty());
        CHECK(target.rp_name.empty());
        CHECK(target.bp_name.empty());
        CHECK(target.world_name.empty());
        CHECK(target.world_path.empty());
        CHECK(target.build.empty());
    }
    SECTION("readOnly becomes false")
    {
        if (!bad.is_boolean()) {
            CHECK_FALSE(parse_target("readOnly").read_only);
        }
    }
    SECTION("readOnly given as a string is not truthy")
    {
        json const target = {{"target", "local"}, {"readOnly", "true"}};
        const auto result = parse_profile(profile_with_export(target));
        CHECK_FALSE(value_of(result).exports.at(0).read_only);
    }
}

TEST_CASE("profile: preShell and postShell", "[profile]")
{
    const auto *const key = GENERATE("preShell", "postShell");
    CAPTURE(key);

    const auto parse_shell = [&](const json &value) {
        json profile = minimal_profile();
        profile[key] = value;
        return parse_profile(profile);
    };
    const auto commands_of =
        [&](const Profile &profile) -> const ShellCommands & {
        return std::string_view(key) == "preShell" ? profile.pre_shell
                                                   : profile.post_shell;
    };

    SECTION("absent means no commands")
    {
        const auto result = parse_profile(minimal_profile());
        CHECK(value_of(result).pre_shell.empty());
        CHECK(value_of(result).post_shell.empty());
    }
    SECTION("an array applies to every OS")
    {
        const auto result = parse_shell(R"json(["echo a", "echo b"])json"_json);
        const auto &shell = commands_of(value_of(result));
        CHECK(shell.all == std::vector<std::string>{"echo a", "echo b"});
        CHECK(shell.windows.empty());
        CHECK(shell.linux.empty());
        CHECK(shell.darwin.empty());
        CHECK_FALSE(shell.empty());
    }
    SECTION("an empty array is empty")
    {
        const auto result = parse_shell(json::array());
        CHECK(commands_of(value_of(result)).empty());
    }
    SECTION("an object has one list per OS")
    {
        const auto result = parse_shell(R"json({
      "windows": ["dir", "cls"], "linux": ["ls"], "darwin": ["ls -G", "clear"]
    })json"_json);
        const auto &shell = commands_of(value_of(result));
        CHECK(shell.all.empty());
        CHECK(shell.windows == std::vector<std::string>{"dir", "cls"});
        CHECK(shell.linux == std::vector<std::string>{"ls"});
        CHECK(shell.darwin == std::vector<std::string>{"ls -G", "clear"});
    }
    SECTION("each OS is optional")
    {
        const auto result = parse_shell(R"json({ "linux": ["ls"] })json"_json);
        const auto &shell = commands_of(value_of(result));
        CHECK(shell.windows.empty());
        CHECK(shell.linux == std::vector<std::string>{"ls"});
        CHECK(shell.darwin.empty());
    }
    SECTION("an empty object is empty")
    {
        const auto result = parse_shell(json::object());
        CHECK(commands_of(value_of(result)).empty());
    }
    SECTION("unknown OS keys are ignored")
    {
        const auto result = parse_shell(
            R"json({ "freebsd": ["ls"], "linux": ["ls"] })json"_json);
        CHECK(value_of(result).pre_shell.windows.empty());
    }
    SECTION("commands keep their order and content")
    {
        const auto result = parse_shell(json::array(
            {"echo \"quoted\"", "", "$env:X = 'y'", "echo 日本語"}));
        CHECK(commands_of(value_of(result)).all.size() == 4);
        CHECK(commands_of(value_of(result)).all[0] == "echo \"quoted\"");
        CHECK(commands_of(value_of(result)).all[3] == "echo 日本語");
    }
    SECTION("array elements must be strings")
    {
        expect_error(parse_shell(json::array({"ok", 5})), {key, "1"},
                     expected("string"));
    }
    SECTION("OS lists must be arrays")
    {
        const auto *const os = GENERATE("windows", "linux", "darwin");
        CAPTURE(os);
        const auto result = parse_shell(json{{os, "echo hi"}});
        expect_error_at(result, {key, os});
        CHECK(result.error().message.starts_with("expected"));
    }
    SECTION("OS list elements must be strings")
    {
        const auto *const os = GENERATE("windows", "linux", "darwin");
        CAPTURE(os);
        expect_error(parse_shell(json{{os, json::array({"ok", 1})}}),
                     {key, os, "1"}, expected("string"));
    }
    SECTION("must be an array or an object")
    {
        const auto bad =
            GENERATE(json("echo hi"), json(1), json(true), json(nullptr));
        const auto result = parse_shell(bad);
        expect_error_at(result, {key});
        CHECK(result.error().message.starts_with("expected"));
    }
}

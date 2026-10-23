// Parses the config.json files of Regolith's own test projects. They are
// copied from https://github.com/Bedrock-OSS/regolith (MIT), see
// tests/fixtures/regolith/README.md.

#include "../support/helpers.hpp"
#include "basalt/config.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_range.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

using Catch::Matchers::ContainsSubstring;
using namespace basalt;
using namespace basalt::test;

namespace fs = std::filesystem;

namespace {

// tests/fixtures/regolith, located relative to this source file (or, if the
// build used a relative __FILE__, by walking up from the working directory).
fs::path fixtures_root()
{
  const fs::path suffix = fs::path("tests") / "fixtures" / "regolith";

  fs::path beside_source =
      fs::absolute(fs::path(__FILE__)).parent_path().parent_path() /
      "fixtures" / "regolith";
  if (fs::is_directory(beside_source)) {
    return beside_source;
  }
  for (fs::path dir = fs::current_path(); !dir.empty();
       dir = dir.parent_path()) {
    if (fs::is_directory(dir / suffix)) {
      return dir / suffix;
    }
    if (dir == dir.parent_path()) {
      break;
    }
  }
  return beside_source;
}

// Every fixture config.json, relative to the fixtures root, sorted.
std::vector<fs::path> fixture_configs()
{
  std::vector<fs::path> result;
  const fs::path root = fixtures_root();
  if (!fs::is_directory(root)) {
    return result;
  }
  for (const auto &entry : fs::recursive_directory_iterator(root)) {
    if (entry.is_regular_file() && entry.path().filename() == "config.json") {
      result.push_back(fs::relative(entry.path(), root));
    }
  }
  std::ranges::sort(result);
  return result;
}

// Parses fixtures/<name>/config.json and returns the config.
Config load_fixture(const fs::path &name)
{
  const auto result = load_config(fixtures_root() / name / "config.json");
  return value_of(result);
}

template <typename Kind>
const Kind &definition_of(const Config &config, std::string_view id)
{
  const auto it = config.filter_definitions.find(id);
  REQUIRE(it != config.filter_definitions.end());
  REQUIRE(std::holds_alternative<Kind>(it->second.kind));
  return std::get<Kind>(it->second.kind);
}

// Contains deliberately circular profiles, which Regolith only rejects when one
// of them is run.
const fs::path circular_fixture =
    fs::path("profile_filter") / "project" / "config.json";

// Regolith's own parser rejects this one too: a remote filter definition
// without a version (the file is the expected output of an install test).
const fs::path versionless_fixture =
    fs::path("double_remote_project_installed") / "config.json";

} // namespace

TEST_CASE("fixtures: the Regolith fixtures are present", "[fixtures]")
{
  REQUIRE(fs::is_directory(fixtures_root()));
  CHECK(fixture_configs().size() >= 30);
}

TEST_CASE("fixtures: every Regolith test project config parses", "[fixtures]")
{
  const auto configs = fixture_configs();
  REQUIRE_FALSE(configs.empty());
  const auto &relative = GENERATE_COPY(from_range(configs));
  CAPTURE(relative.generic_string());

  if (relative == versionless_fixture) {
    expect_error(load_config(fixtures_root() / relative),
                 {"regolith", "filterDefinitions", "nested-remote-filter"},
                 missing("version"));
  } else {
    const auto result = load_config(fixtures_root() / relative);
    const std::string detail = result ? "" : result.error().to_string();
    INFO(detail);
    CHECK(result.has_value());
  }
}

TEST_CASE("fixtures: profile_filter only rejects its circular profiles",
          "[fixtures]")
{
  const auto result = load_config(fixtures_root() / circular_fixture);
  const auto &config = value_of(result);

  CHECK(check_profile(config, "dev").has_value());
  CHECK(check_profile(config, "correct_nested_profile").has_value());
  const auto &filters = config.profiles.at("correct_nested_profile").filters;
  REQUIRE(filters.size() == 1);
  REQUIRE(std::holds_alternative<ProfileCall>(filters[0].kind));
  CHECK(std::get<ProfileCall>(filters[0].kind).profile == "dev");

  for (const char *name :
       {"invalid_circular_profile_1", "invalid_circular_profile_2",
        "invalid_circular_profile_3"}) {
    CAPTURE(name);
    const auto checked = check_profile(config, name);
    REQUIRE_FALSE(checked);
    CHECK_THAT(checked.error().message,
               ContainsSubstring("circular profile reference"));
  }
}

TEST_CASE("fixtures: exe_filter", "[fixtures]")
{
  const auto config = load_fixture("exe_filter/project");
  CHECK(config.name == "exe_filter_test_project");
  CHECK(config.author == "Bedrock-OSS");
  CHECK(config.packs.behavior_pack == "./packs/BP");
  CHECK(config.packs.resource_pack == "./packs/RP");
  CHECK(config.data_path == "./packs/data");
  CHECK(definition_of<ExeFilter>(config, "test_exe_filter").exe ==
        "./executables/test_exe_filter");

  const auto &dev = config.profiles.at("dev");
  REQUIRE(dev.filters.size() == 1);
  REQUIRE(std::holds_alternative<FilterCall>(dev.filters[0].kind));
  CHECK(std::get<FilterCall>(dev.filters[0].kind).filter == "test_exe_filter");
  REQUIRE(dev.exports.size() == 1);
  CHECK(dev.exports[0].target == "local");
  CHECK_FALSE(dev.exports[0].read_only);
}

TEST_CASE("fixtures: multitarget_project", "[fixtures]")
{
  const auto config = load_fixture("multitarget_project");
  REQUIRE(config.profiles.size() == 2);

  const auto &a = config.profiles.at("exact_export_A");
  REQUIRE(a.exports.size() == 1);
  CHECK(a.exports[0].target == "exact");
  CHECK(a.exports[0].rp_path == "../target-a/RP");
  CHECK(a.exports[0].bp_path == "../target-a/BP");

  const auto &b = config.profiles.at("exact_export_B");
  REQUIRE(b.exports.size() == 1);
  CHECK(b.exports[0].rp_path == "../target-b/RP");
  CHECK(b.exports[0].bp_path == "../target-b/BP");
}

TEST_CASE("fixtures: async_filter", "[fixtures]")
{
  const auto config = load_fixture("async_filter/project");
  CHECK(config.format_version == "1.7.0");
  CHECK(definition_of<PythonFilter>(config, "wait_and_print").script ==
        "local_filters/wait_and_print.py");

  const auto &filters = config.profiles.at("default").filters;
  REQUIRE(filters.size() == 1);
  REQUIRE(std::holds_alternative<AsyncCall>(filters[0].kind));
  const auto &async = std::get<AsyncCall>(filters[0].kind);
  REQUIRE(async.filters.size() == 3);
  for (const auto &child : async.filters) {
    REQUIRE(std::holds_alternative<FilterCall>(child.kind));
    CHECK(std::get<FilterCall>(child.kind).filter == "wait_and_print");
    CHECK(child.options.settings.at("wait_time") == 3);
  }
  CHECK(async.filters[2].options.settings.at("message") == "Hello 3");
}

TEST_CASE("fixtures: pre/post shell commands", "[fixtures]")
{
  SECTION("one list for every OS")
  {
    const auto config = load_fixture("pre_post_shell/project");
    const auto &profile = config.profiles.at("default");
    CHECK(profile.pre_shell.all.size() == 4);
    CHECK(profile.post_shell.all.size() == 2);
    CHECK(profile.pre_shell.windows.empty());
    CHECK(profile.pre_shell.all[0] == "$env:TEST_VAR = 'hello_from_preshell'");
  }
  SECTION("per-OS lists")
  {
    const auto config = load_fixture("pre_post_shell_os_specific/project");
    const auto &profile = config.profiles.at("default");
    CHECK(profile.pre_shell.all.empty());
    CHECK(profile.pre_shell.windows.size() == 2);
    CHECK(profile.pre_shell.linux.size() == 2);
    CHECK(profile.pre_shell.darwin.size() == 2);
    CHECK(profile.post_shell.windows.size() == 1);
    CHECK(profile.post_shell.linux.size() == 1);
    CHECK(profile.post_shell.darwin.size() == 1);
    CHECK(profile.pre_shell.linux[0] == "export OS_NAME='Linux'");
    CHECK(profile.exports.at(0).rp_path == "./build/RP");
  }
}

TEST_CASE("fixtures: development export targets", "[fixtures]")
{
  const auto config = load_fixture("development_export_targets");
  CHECK(config.format_version == "1.4.0");
  REQUIRE(config.profiles.size() == 3);
  for (const char *build : {"standard", "education", "preview"}) {
    const auto &exports = config.profiles.at(build).exports;
    REQUIRE(exports.size() == 1);
    CHECK(exports[0].target == "development");
    CHECK(exports[0].build == build);
  }
}

TEST_CASE("fixtures: export names and conditions", "[fixtures]")
{
  SECTION("custom pack names")
  {
    const auto config = load_fixture("custom_pack_name/project");
    const auto &target = config.profiles.at("default").exports.at(0);
    CHECK(target.rp_name == "project.name + '_by_' + project.author + '_RP'");
    CHECK(target.bp_name == "project.name + '_by_' + project.author + '_BP'");
    CHECK(config.profiles.at("default").filters.empty());
  }
  SECTION("conditional filters")
  {
    const auto config = load_fixture("conditional_filter/project");
    const auto &filters = config.profiles.at("default").filters;
    REQUIRE(filters.size() == 2);
    CHECK(filters[0].options.when == "2 + 2 == 4");
    CHECK(filters[1].options.when == "2 + 2 == 5");
    CHECK(filters[0].options.settings.at("output_text") == "2 + 2 == 4");
  }
}

TEST_CASE("fixtures: remote filters", "[fixtures]")
{
  SECTION("pinned to a commit")
  {
    const auto config = load_fixture("double_remote_project");
    const auto &filter =
        definition_of<RemoteFilter>(config, "nested-remote-filter");
    CHECK(filter.url == "github.com/Bedrock-OSS/regolith-test-filters");
    CHECK(filter.version == "6230216184232e5bc7439caaf20cdd4c0dc17ca2");
  }
  SECTION("version kinds")
  {
    CHECK(definition_of<RemoteFilter>(load_fixture("regolith_install/latest"),
                                      "hello-version-python-filter")
              .version == "latest");
    CHECK(definition_of<RemoteFilter>(load_fixture("regolith_install/HEAD"),
                                      "hello-version-python-filter")
              .version == "HEAD");
    CHECK(definition_of<RemoteFilter>(load_fixture("regolith_install/tag"),
                                      "hello-version-python-filter")
              .version == "TEST_TAG_1");
    CHECK(definition_of<RemoteFilter>(load_fixture("regolith_install/1.0.1"),
                                      "hello-version-python-filter")
              .version == "1.0.1");
  }
}

TEST_CASE("fixtures: minimal projects", "[fixtures]")
{
  const auto config = load_fixture("minimal_project");
  CHECK(config.filter_definitions.empty());
  CHECK(config.profiles.contains("dev"));

  const auto fresh = load_fixture("fresh_project");
  CHECK(fresh.format_version == "1.8.0");
}

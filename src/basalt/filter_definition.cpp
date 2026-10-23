#include "basalt/filter_definition.hpp"

#include "basalt/json_util.hpp"

#include <algorithm>
#include <array>
#include <format>
#include <utility>

namespace basalt {

namespace {

using detail::Result;

constexpr std::string_view valid_run_with =
    "java, dotnet, nim, deno, nodejs, bun, python, shell, exe";

[[nodiscard]] Result<FilterKind> parse_python(const nlohmann::json &json)
{
  auto script = detail::require_string(json, "script");
  if (!script) {
    return tl::unexpected(script.error());
  }
  // Regolith reads this as a float64 and ignores values of any other type.
  int venv_slot = 0;
  if (const auto *slot = detail::find(json, "venvSlot");
      slot != nullptr && slot->is_number()) {
    venv_slot = static_cast<int>(slot->get<double>());
  }
  auto requirements = detail::optional_string(json, "requirements");
  if (!requirements) {
    return tl::unexpected(requirements.error());
  }
  return PythonFilter{.script = std::move(*script),
                      .venv_slot = venv_slot,
                      .requirements = std::move(*requirements)};
}

[[nodiscard]] Result<FilterKind> parse_nodejs(const nlohmann::json &json)
{
  auto script = detail::require_string(json, "script");
  if (!script) {
    return tl::unexpected(script.error());
  }
  auto requirements = detail::optional_string(json, "requirements");
  if (!requirements) {
    return tl::unexpected(requirements.error());
  }
  return NodeJsFilter{.script = std::move(*script),
                      .requirements = std::move(*requirements)};
}

[[nodiscard]] Result<FilterKind> parse_nim(const nlohmann::json &json)
{
  auto script = detail::require_string(json, "script");
  if (!script) {
    return tl::unexpected(script.error());
  }
  auto requirements = detail::optional_string(json, "requirements");
  if (!requirements) {
    return tl::unexpected(requirements.error());
  }
  return NimFilter{.script = std::move(*script),
                   .requirements = std::move(*requirements)};
}

[[nodiscard]] Result<FilterKind> parse_bun(const nlohmann::json &json)
{
  return detail::require_string(json, "script").transform([](std::string s) {
    return FilterKind{BunFilter{.script = std::move(s)}};
  });
}

[[nodiscard]] Result<FilterKind> parse_deno(const nlohmann::json &json)
{
  return detail::require_string(json, "script").transform([](std::string s) {
    return FilterKind{DenoFilter{.script = std::move(s)}};
  });
}

[[nodiscard]] Result<FilterKind> parse_java(const nlohmann::json &json)
{
  // "script" is a deprecated spelling of "path", only used when "path" is
  // absent.
  const auto *const key = detail::find(json, "path") == nullptr &&
                                  detail::find(json, "script") != nullptr
                              ? "script"
                              : "path";
  return detail::require_string(json, key).transform([](std::string s) {
    return FilterKind{JavaFilter{.path = std::move(s)}};
  });
}

[[nodiscard]] Result<FilterKind> parse_dotnet(const nlohmann::json &json)
{
  return detail::require_string(json, "path").transform([](std::string s) {
    return FilterKind{DotnetFilter{.path = std::move(s)}};
  });
}

[[nodiscard]] Result<FilterKind> parse_shell(const nlohmann::json &json)
{
  return detail::require_string(json, "command").transform([](std::string s) {
    return FilterKind{ShellFilter{.command = std::move(s)}};
  });
}

[[nodiscard]] Result<FilterKind> parse_exe(const nlohmann::json &json)
{
  return detail::require_string(json, "exe").transform([](std::string s) {
    return FilterKind{ExeFilter{.exe = std::move(s)}};
  });
}

[[nodiscard]] Result<FilterKind> parse_remote(const nlohmann::json &json)
{
  auto version = detail::require_string(json, "version");
  if (!version) {
    return tl::unexpected(version.error());
  }
  // A "url" of the wrong type falls back to the default, like Regolith.
  auto url = detail::lenient_optional_string(json, "url");
  return RemoteFilter{.url = url ? std::move(*url)
                                 : std::string{default_remote_filter_url},
                      .version = std::move(*version)};
}

struct RunWithParser {
  std::string_view name;
  Result<FilterKind> (*parse)(const nlohmann::json &);
};

constexpr std::array run_with_parsers{
    RunWithParser{.name = "java", .parse = parse_java},
    RunWithParser{.name = "dotnet", .parse = parse_dotnet},
    RunWithParser{.name = "nim", .parse = parse_nim},
    RunWithParser{.name = "deno", .parse = parse_deno},
    RunWithParser{.name = "nodejs", .parse = parse_nodejs},
    RunWithParser{.name = "bun", .parse = parse_bun},
    RunWithParser{.name = "python", .parse = parse_python},
    RunWithParser{.name = "shell", .parse = parse_shell},
    RunWithParser{.name = "exe", .parse = parse_exe},
};

[[nodiscard]] Result<FilterKind> parse_kind(const std::string &run_with,
                                            const nlohmann::json &json)
{
  if (run_with.empty()) {
    return parse_remote(json);
  }
  const auto *parser =
      std::ranges::find(run_with_parsers, run_with, &RunWithParser::name);
  if (parser == run_with_parsers.end()) {
    return tl::unexpected(ConfigError{
        .path = {"runWith"},
        .message = std::format("invalid runWith \"{}\" (valid values: {})",
                               run_with, valid_run_with)});
  }
  return parser->parse(json);
}

} // namespace

tl::expected<FilterDefinition, ConfigError>
parse_filter_definition(const std::string &id, const nlohmann::json &json)
{
  if (auto error = detail::check_is_object(json)) {
    return tl::unexpected(std::move(*error));
  }

  // A missing, empty or non-string runWith means a remote filter.
  const std::string run_with = detail::lenient_string(json, "runWith");

  auto kind = parse_kind(run_with, json);
  if (!kind) {
    return tl::unexpected(std::move(kind.error()));
  }
  return FilterDefinition{.id = id, .kind = std::move(*kind), .raw = json};
}

} // namespace basalt

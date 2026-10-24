#include "basalt/config.hpp"
#include "basalt/version.hpp"

#include <CLI/CLI.hpp>
#include <spdlog/spdlog.h>

#include <exception>
#include <string>

#ifdef _WIN32
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

namespace {

[[nodiscard]] int run(int argc, char **argv)
{
    CLI::App app{"basalt - a C++ rewrite of Regolith"};
    app.set_version_flag("--version", std::string{basalt::version});
    app.require_subcommand(1);

    bool debug = false;
    app.add_flag("--debug", debug, "Enable debug logging");

    std::string profile = "default";
    auto *run_cmd = app.add_subcommand("run", "Run a profile");
    run_cmd->add_option("profile", profile, "Profile to run");

    argv = app.ensure_utf8(argv);
    CLI11_PARSE(app, argc, argv);

    if (debug) {
        spdlog::set_level(spdlog::level::debug);
    }

    if (*run_cmd) {
        const auto config = basalt::load_config("config.json");
        if (!config) {
            spdlog::error("{}", config.error().to_string());
            return 1;
        }
        if (const auto checked = basalt::check_profile(*config, profile);
            !checked) {
            spdlog::error("{}", checked.error().to_string());
            return 1;
        }
        spdlog::info("project '{}' by {}", config->name, config->author);
        spdlog::warn("running profile '{}' is not implemented yet", profile);
    }
    return 0;
}

} // namespace

int main(int argc, char **argv)
{
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif

    try {
        return run(argc, argv);
    } catch (const std::exception &e) {
        spdlog::critical("{}", e.what());
    } catch (...) {
        spdlog::critical("unknown error");
    }
    return 1;
}

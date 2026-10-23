#include <CLI/CLI.hpp>
#include <spdlog/spdlog.h>

#ifdef _WIN32
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#include "basalt/config.hpp"
#include "basalt/version.hpp"

int main(int argc, char** argv) {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif

    CLI::App app{"basalt - a C++ rewrite of Regolith"};
    app.set_version_flag("--version", std::string(basalt::version));
    app.require_subcommand(1);

    bool debug = false;
    app.add_flag("--debug", debug, "Enable debug logging");

    std::string profile = "default";
    auto* run = app.add_subcommand("run", "Run a profile");
    run->add_option("profile", profile, "Profile to run");

    argv = app.ensure_utf8(argv);
    CLI11_PARSE(app, argc, argv);

    if (debug) {
        spdlog::set_level(spdlog::level::debug);
    }

    if (*run) {
        auto config = basalt::load_config("config.json");
        if (!config) {
            spdlog::error("{}", config.error());
            return 1;
        }
        spdlog::info("project '{}' by {}", config->name, config->author);
        spdlog::warn("running profile '{}' is not implemented yet", profile);
    }
    return 0;
}

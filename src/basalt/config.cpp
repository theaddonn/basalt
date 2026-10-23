#include "basalt/config.hpp"

#include <fstream>

namespace basalt {

tl::expected<Config, std::string> load_config(const std::filesystem::path& path) {
    std::ifstream file(path);
    if (!file) {
        return tl::unexpected("could not open " + path.string());
    }

    nlohmann::json json;
    try {
        json = nlohmann::json::parse(file);
    } catch (const nlohmann::json::parse_error& e) {
        return tl::unexpected(std::string("invalid json: ") + e.what());
    }

    Config config;
    config.name = json.value("name", "");
    config.author = json.value("author", "");
    if (auto packs = json.find("packs"); packs != json.end()) {
        config.behavior_pack = packs->value("behaviorPack", "");
        config.resource_pack = packs->value("resourcePack", "");
    }
    if (auto regolith = json.find("regolith"); regolith != json.end()) {
        config.data_path = regolith->value("dataPath", "");
    }
    config.raw = std::move(json);
    return config;
}

} // namespace basalt

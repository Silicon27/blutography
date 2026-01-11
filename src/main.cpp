#include <drogon/drogon.h>
#include <filesystem>

int main() {
    std::string configPath;
    if (std::filesystem::exists("config.json")) {
        configPath = "config.json";
    } else if (std::filesystem::exists("config.yaml")) {
        configPath = "config.yaml";
    } else if (std::filesystem::exists("../config.json")) {
        std::filesystem::current_path("..");
        configPath = "config.json";
    } else if (std::filesystem::exists("../config.yaml")) {
        std::filesystem::current_path("..");
        configPath = "config.yaml";
    }

    if (configPath.empty()) {
        LOG_ERROR << "Could not find config.yaml or config.json";
        return 1;
    }

    try {
        drogon::app().loadConfigFile(configPath);
        LOG_INFO << "Loaded config file from: " << std::filesystem::absolute(configPath);
    } catch (const std::exception& e) {
        LOG_ERROR << "Failed to load config file: " << e.what();
        return 1;
    }

    drogon::app().run();
}
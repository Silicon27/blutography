#include <drogon/drogon.h>
#include <filesystem>
#include <yaml-cpp/yaml.h>

int main() {
    
    // load configuration files
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

    // initialize cache/, logs/, and gallery_previews/ directory
    if (!std::filesystem::exists("cache")) {
        std::filesystem::create_directory("cache");
        LOG_INFO << "Created cache directory";
    }
    if (!std::filesystem::exists("logs")) {
        std::filesystem::create_directory("logs");
        LOG_INFO << "Created log directory";
    }
    if (!std::filesystem::exists("gallery_previews")) {
        std::filesystem::create_directory("gallery_previews");
        LOG_INFO << "Created gallery_previews directory";
    }

    try {
        drogon::app().loadConfigFile(configPath);
        LOG_INFO << "Loaded config file from: " << std::filesystem::absolute(configPath);
        LOG_INFO << "Current working directory: " << std::filesystem::current_path();
        LOG_INFO << "Gallery previews path exists: " << std::filesystem::exists("gallery_previews");
    } catch (const std::exception& e) {
        LOG_ERROR << "Failed to load config file: " << e.what();
        return 1;
    }

    drogon::app().run();
    
    // parse shutdown_options.yaml
    if (std::filesystem::exists("shutdown_options.yaml")) {
        try {
            YAML::Node root = YAML::LoadFile("shutdown_options.yaml");
            auto options = root["shutdown_options"];
            if (options) {
                if (options["cache_cleanup"] && options["cache_cleanup"].as<bool>()) {
                    std::cout << "Cleaning up cache directory" << std::endl;
                    std::filesystem::remove_all("cache");
                    std::filesystem::create_directory("cache");
                }
                if (options["log_cleanup"] && options["log_cleanup"].as<bool>()) {
                    std::cout << "Cleaning up log directory" << std::endl;
                    std::filesystem::remove_all("logs");
                    std::filesystem::create_directory("logs");
                }
            }
        } catch (const std::exception& e) {
            std::cerr << "Failed to parse shutdown_options.yaml: " << e.what() << std::endl;
        }
    }
}
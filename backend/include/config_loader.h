#pragma once

#include "common.h"
#include <string>
#include <optional>

namespace stone_mill {

struct AppConfig {
    ProcessConfig process;
    DEMConfig dem;
    BreakageModel breakage;
    OptimizationParams optimization;
    AlertThresholds thresholds;
};

class ConfigLoader {
public:
    static std::optional<AppConfig> load_from_file(const std::string& path);
    static std::optional<AppConfig> load_from_string(const std::string& json_content);
    static AppConfig defaults();

    static bool save_to_file(const AppConfig& cfg, const std::string& path);
    static std::string to_json(const AppConfig& cfg);
};

}

#include "config_loader.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <regex>
#include <unordered_map>

namespace stone_mill {

namespace {

std::string trim(const std::string& s) {
    size_t start = s.find_first_not_of(" \t\n\r");
    if (start == std::string::npos) return "";
    size_t end = s.find_last_not_of(" \t\n\r");
    return s.substr(start, end - start + 1);
}

std::string extract_json_value(const std::string& json, const std::string& key) {
    std::string pattern = "\"" + key + "\"\\s*:\\s*";
    std::regex re(pattern + "(-?\\d+\\.?\\d*|\"[^\"]*\"|true|false)");
    std::smatch match;
    if (std::regex_search(json, match, re)) {
        std::string val = match[1].str();
        if (!val.empty() && val.front() == '"' && val.back() == '"') {
            val = val.substr(1, val.size() - 2);
        }
        return val;
    }
    return "";
}

double get_double(const std::string& json, const std::string& key, double def) {
    std::string v = extract_json_value(json, key);
    if (v.empty()) return def;
    try { return std::stod(v); } catch (...) { return def; }
}

int get_int(const std::string& json, const std::string& key, int def) {
    std::string v = extract_json_value(json, key);
    if (v.empty()) return def;
    try { return std::stoi(v); } catch (...) { return def; }
}

bool get_bool(const std::string& json, const std::string& key, bool def) {
    std::string v = extract_json_value(json, key);
    if (v.empty()) return def;
    if (v == "true" || v == "1") return true;
    if (v == "false" || v == "0") return false;
    return def;
}

std::string get_string(const std::string& json, const std::string& key, const std::string& def) {
    std::string v = extract_json_value(json, key);
    return v.empty() ? def : v;
}

std::string extract_section(const std::string& json, const std::string& section) {
    std::string pattern = "\"" + section + "\"\\s*:\\s*\\{";
    std::regex re(pattern);
    std::smatch match;
    if (!std::regex_search(json, match, re)) return "{}";

    size_t start = match.position() + match.length() - 1;
    size_t depth = 1;
    size_t i = start;
    bool in_str = false;
    for (; i < json.size() && depth > 0; ++i) {
        char c = json[i];
        if (c == '"' && (i == 0 || json[i-1] != '\\')) in_str = !in_str;
        if (in_str) continue;
        if (c == '{') depth++;
        else if (c == '}') depth--;
    }
    return json.substr(start, i - start);
}

}

AppConfig ConfigLoader::defaults() {
    AppConfig cfg{};
    return cfg;
}

std::optional<AppConfig> ConfigLoader::load_from_file(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        std::cerr << "[Config] Cannot open file: " << path << ", using defaults" << std::endl;
        return defaults();
    }
    std::stringstream ss;
    ss << file.rdbuf();
    return load_from_string(ss.str());
}

std::optional<AppConfig> ConfigLoader::load_from_string(const std::string& content) {
    AppConfig cfg = defaults();

    auto process = extract_section(content, "process");
    cfg.process.mqtt_host = get_string(process, "mqtt_host", cfg.process.mqtt_host);
    cfg.process.mqtt_port = get_int(process, "mqtt_port", cfg.process.mqtt_port);
    cfg.process.mqtt_topic = get_string(process, "mqtt_topic", cfg.process.mqtt_topic);
    cfg.process.alert_topic = get_string(process, "alert_topic", cfg.process.alert_topic);
    cfg.process.clickhouse_host = get_string(process, "clickhouse_host", cfg.process.clickhouse_host);
    cfg.process.clickhouse_port = get_int(process, "clickhouse_port", cfg.process.clickhouse_port);
    cfg.process.clickhouse_user = get_string(process, "clickhouse_user", cfg.process.clickhouse_user);
    cfg.process.clickhouse_database = get_string(process, "clickhouse_database", cfg.process.clickhouse_database);
    cfg.process.http_port = get_int(process, "http_port", cfg.process.http_port);

    auto dem = extract_section(content, "dem");
    cfg.dem.dt = get_double(dem, "dt", cfg.dem.dt);
    cfg.dem.gravity = get_double(dem, "gravity", cfg.dem.gravity);
    cfg.dem.damping = get_double(dem, "damping", cfg.dem.damping);
    cfg.dem.roller_radius = get_double(dem, "roller_radius", cfg.dem.roller_radius);
    cfg.dem.roller_width = get_double(dem, "roller_width", cfg.dem.roller_width);
    cfg.dem.mill_radius = get_double(dem, "mill_radius", cfg.dem.mill_radius);
    cfg.dem.static_friction = get_double(dem, "static_friction", cfg.dem.static_friction);
    cfg.dem.dynamic_friction = get_double(dem, "dynamic_friction", cfg.dem.dynamic_friction);
    cfg.dem.use_coarse_graining = get_bool(dem, "use_coarse_graining", cfg.dem.use_coarse_graining);
    cfg.dem.coarse_scale = static_cast<uint32_t>(get_int(dem, "coarse_scale", cfg.dem.coarse_scale));
    cfg.dem.coarse_radius_scale = get_double(dem, "coarse_radius_scale", cfg.dem.coarse_radius_scale);
    cfg.dem.use_spatial_grid = get_bool(dem, "use_spatial_grid", cfg.dem.use_spatial_grid);
    cfg.dem.grid_cell_size = get_double(dem, "grid_cell_size", cfg.dem.grid_cell_size);
    cfg.dem.default_moisture = get_double(dem, "default_moisture", cfg.dem.default_moisture);
    cfg.dem.moisture_strength_factor = get_double(dem, "moisture_strength_factor", cfg.dem.moisture_strength_factor);
    cfg.dem.moisture_cohesion_base = get_double(dem, "moisture_cohesion_base", cfg.dem.moisture_cohesion_base);

    auto brk = extract_section(content, "breakage");
    std::string type_str = get_string(brk, "type", "tavar");
    if (type_str == "rosin_rammler") cfg.breakage.type = BreakageFunctionType::ROSIN_RAMMLER;
    else if (type_str == "bond") cfg.breakage.type = BreakageFunctionType::BOND;
    else cfg.breakage.type = BreakageFunctionType::TAVAR;
    cfg.breakage.selection_function_param = get_double(brk, "selection_function_param", cfg.breakage.selection_function_param);
    cfg.breakage.breakage_distribution_param = get_double(brk, "breakage_distribution_param", cfg.breakage.breakage_distribution_param);
    cfg.breakage.screening_efficiency = get_double(brk, "screening_efficiency", cfg.breakage.screening_efficiency);

    auto opt = extract_section(content, "optimization");
    opt = content;
    cfg.optimization.min_speed = get_double(opt, "min_speed", cfg.optimization.min_speed);
    cfg.optimization.max_speed = get_double(opt, "max_speed", cfg.optimization.max_speed);
    cfg.optimization.min_gap = get_double(opt, "min_gap", cfg.optimization.min_gap);
    cfg.optimization.max_gap = get_double(opt, "max_gap", cfg.optimization.max_gap);
    cfg.optimization.population_size = static_cast<uint32_t>(get_int(opt, "population_size", cfg.optimization.population_size));
    cfg.optimization.max_generations = static_cast<uint32_t>(get_int(opt, "max_generations", cfg.optimization.max_generations));
    cfg.optimization.mutation_rate = get_double(opt, "mutation_rate", cfg.optimization.mutation_rate);
    cfg.optimization.crossover_rate = get_double(opt, "crossover_rate", cfg.optimization.crossover_rate);

    auto thr = extract_section(content, "thresholds");
    thr = content;
    cfg.process.thresholds.wear_warning = get_double(thr, "wear_warning", cfg.process.thresholds.wear_warning);
    cfg.process.thresholds.wear_critical = get_double(thr, "wear_critical", cfg.process.thresholds.wear_critical);
    cfg.process.thresholds.low_yield = get_double(thr, "low_yield", cfg.process.thresholds.low_yield);
    cfg.process.thresholds.min_speed = get_double(thr, "min_speed", cfg.process.thresholds.min_speed);
    cfg.process.thresholds.max_speed = get_double(thr, "max_speed", cfg.process.thresholds.max_speed);
    cfg.process.thresholds.min_pressure = get_double(thr, "min_pressure", cfg.process.thresholds.min_pressure);
    cfg.process.thresholds.max_pressure = get_double(thr, "max_pressure", cfg.process.thresholds.max_pressure);

    std::cout << "[Config] Loaded from JSON" << std::endl;
    return cfg;
}

std::string ConfigLoader::to_json(const AppConfig& cfg) {
    std::stringstream ss;
    ss << "{\n";
    ss << "  \"process\": {\n";
    ss << "    \"mqtt_host\": \"" << cfg.process.mqtt_host << "\",\n";
    ss << "    \"mqtt_port\": " << cfg.process.mqtt_port << ",\n";
    ss << "    \"mqtt_topic\": \"" << cfg.process.mqtt_topic << "\",\n";
    ss << "    \"alert_topic\": \"" << cfg.process.alert_topic << "\",\n";
    ss << "    \"clickhouse_host\": \"" << cfg.process.clickhouse_host << "\",\n";
    ss << "    \"clickhouse_port\": " << cfg.process.clickhouse_port << ",\n";
    ss << "    \"clickhouse_database\": \"" << cfg.process.clickhouse_database << "\",\n";
    ss << "    \"http_port\": " << cfg.process.http_port << "\n";
    ss << "  },\n";
    ss << "  \"dem\": {\n";
    ss << "    \"dt\": " << cfg.dem.dt << ",\n";
    ss << "    \"gravity\": " << cfg.dem.gravity << ",\n";
    ss << "    \"damping\": " << cfg.dem.damping << ",\n";
    ss << "    \"roller_radius\": " << cfg.dem.roller_radius << ",\n";
    ss << "    \"roller_width\": " << cfg.dem.roller_width << ",\n";
    ss << "    \"mill_radius\": " << cfg.dem.mill_radius << ",\n";
    ss << "    \"static_friction\": " << cfg.dem.static_friction << ",\n";
    ss << "    \"dynamic_friction\": " << cfg.dem.dynamic_friction << ",\n";
    ss << "    \"use_coarse_graining\": " << (cfg.dem.use_coarse_graining ? "true" : "false") << ",\n";
    ss << "    \"coarse_scale\": " << cfg.dem.coarse_scale << ",\n";
    ss << "    \"coarse_radius_scale\": " << cfg.dem.coarse_radius_scale << ",\n";
    ss << "    \"use_spatial_grid\": " << (cfg.dem.use_spatial_grid ? "true" : "false") << ",\n";
    ss << "    \"grid_cell_size\": " << cfg.dem.grid_cell_size << ",\n";
    ss << "    \"default_moisture\": " << cfg.dem.default_moisture << ",\n";
    ss << "    \"moisture_strength_factor\": " << cfg.dem.moisture_strength_factor << ",\n";
    ss << "    \"moisture_cohesion_base\": " << cfg.dem.moisture_cohesion_base << "\n";
    ss << "  },\n";
    ss << "  \"breakage\": {\n";
    ss << "    \"type\": \"tavar\",\n";
    ss << "    \"selection_function_param\": " << cfg.breakage.selection_function_param << ",\n";
    ss << "    \"breakage_distribution_param\": " << cfg.breakage.breakage_distribution_param << ",\n";
    ss << "    \"screening_efficiency\": " << cfg.breakage.screening_efficiency << "\n";
    ss << "  },\n";
    ss << "  \"optimization\": {\n";
    ss << "    \"min_speed\": " << cfg.optimization.min_speed << ",\n";
    ss << "    \"max_speed\": " << cfg.optimization.max_speed << ",\n";
    ss << "    \"min_gap\": " << cfg.optimization.min_gap << ",\n";
    ss << "    \"max_gap\": " << cfg.optimization.max_gap << ",\n";
    ss << "    \"population_size\": " << cfg.optimization.population_size << ",\n";
    ss << "    \"max_generations\": " << cfg.optimization.max_generations << ",\n";
    ss << "    \"mutation_rate\": " << cfg.optimization.mutation_rate << ",\n";
    ss << "    \"crossover_rate\": " << cfg.optimization.crossover_rate << "\n";
    ss << "  },\n";
    ss << "  \"thresholds\": {\n";
    ss << "    \"wear_warning\": " << cfg.process.thresholds.wear_warning << ",\n";
    ss << "    \"wear_critical\": " << cfg.process.thresholds.wear_critical << ",\n";
    ss << "    \"low_yield\": " << cfg.process.thresholds.low_yield << ",\n";
    ss << "    \"min_speed\": " << cfg.process.thresholds.min_speed << ",\n";
    ss << "    \"max_speed\": " << cfg.process.thresholds.max_speed << ",\n";
    ss << "    \"min_pressure\": " << cfg.process.thresholds.min_pressure << ",\n";
    ss << "    \"max_pressure\": " << cfg.process.thresholds.max_pressure << "\n";
    ss << "  }\n";
    ss << "}";
    return ss.str();
}

bool ConfigLoader::save_to_file(const AppConfig& cfg, const std::string& path) {
    std::ofstream file(path);
    if (!file.is_open()) return false;
    file << to_json(cfg);
    return true;
}

}

#include "common.h"
#include <sstream>
#include <iomanip>
#include <random>
#include <algorithm>

namespace stone_mill {

std::string to_string(AlertType type) {
    switch (type) {
        case AlertType::WEAR: return "wear";
        case AlertType::LOW_YIELD: return "low_yield";
        case AlertType::ABNORMAL_SPEED: return "abnormal_speed";
        case AlertType::ABNORMAL_PRESSURE: return "abnormal_pressure";
    }
    return "unknown";
}

std::string to_string(AlertSeverity severity) {
    switch (severity) {
        case AlertSeverity::INFO: return "info";
        case AlertSeverity::WARNING: return "warning";
        case AlertSeverity::CRITICAL: return "critical";
    }
    return "unknown";
}

AlertType alert_type_from_string(const std::string& s) {
    if (s == "wear") return AlertType::WEAR;
    if (s == "low_yield") return AlertType::LOW_YIELD;
    if (s == "abnormal_speed") return AlertType::ABNORMAL_SPEED;
    if (s == "abnormal_pressure") return AlertType::ABNORMAL_PRESSURE;
    throw std::invalid_argument("Unknown alert type: " + s);
}

AlertSeverity alert_severity_from_string(const std::string& s) {
    if (s == "info") return AlertSeverity::INFO;
    if (s == "warning") return AlertSeverity::WARNING;
    if (s == "critical") return AlertSeverity::CRITICAL;
    throw std::invalid_argument("Unknown alert severity: " + s);
}

ProcessConfig default_process_config() {
    ProcessConfig c{};
    c.mqtt_host = "localhost";
    c.mqtt_port = 1883;
    c.mqtt_topic = "stone_mill/sensor/+";
    c.alert_topic = "stone_mill/alerts";
    c.clickhouse_host = "localhost";
    c.clickhouse_port = 8123;
    c.clickhouse_user = "default";
    c.clickhouse_database = "stone_mill";
    c.http_port = 8080;
    c.thresholds = default_alert_thresholds();
    return c;
}

DEMConfig default_dem_config() {
    DEMConfig c{};
    c.dt = 1e-5;
    c.gravity = 9.81;
    c.damping = 0.99;
    c.roller_radius = 0.8;
    c.roller_width = 0.3;
    c.mill_radius = 2.0;
    c.static_friction = 0.5;
    c.dynamic_friction = 0.3;
    c.use_coarse_graining = false;
    c.coarse_scale = 4;
    c.coarse_radius_scale = 1.5874;
    c.use_spatial_grid = true;
    c.grid_cell_size = 0.05;
    c.default_moisture = 0.12;
    c.moisture_strength_factor = 0.85;
    c.moisture_cohesion_base = 500.0;
    return c;
}

BreakageModel default_breakage_model() {
    BreakageModel m{};
    m.type = BreakageFunctionType::TAVAR;
    m.selection_function_param = 0.5;
    m.breakage_distribution_param = 0.7;
    m.screening_efficiency = 0.85;
    return m;
}

OptimizationParams default_optimization_params() {
    OptimizationParams p{};
    p.mill_id = 1;
    std::memset(p.target_size_range, 0, OPT_PARAM_STR_LEN);
    std::memcpy(p.target_size_range, "0.1-0.5", 7);
    p.min_speed = 5.0;
    p.max_speed = 30.0;
    p.min_gap = 0.5;
    p.max_gap = 5.0;
    p.population_size = 50;
    p.max_generations = 100;
    p.mutation_rate = 0.1;
    p.crossover_rate = 0.8;
    return p;
}

AlertThresholds default_alert_thresholds() {
    AlertThresholds t{};
    t.wear_warning = 70.0;
    t.wear_critical = 90.0;
    t.low_yield = 5.0;
    t.min_speed = 3.0;
    t.max_speed = 35.0;
    t.min_pressure = 100.0;
    t.max_pressure = 1000.0;
    return t;
}

}

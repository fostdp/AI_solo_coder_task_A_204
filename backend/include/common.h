#pragma once

#include <string>
#include <vector>
#include <array>
#include <chrono>
#include <optional>
#include <mutex>
#include <atomic>
#include <cstring>

namespace stone_mill {

constexpr size_t GRAIN_SIZE_BINS = 6;
constexpr size_t SIZE_BINS = GRAIN_SIZE_BINS;
constexpr size_t OPT_PARAM_STR_LEN = 64;

struct GrainSizeDistribution {
    std::array<double, GRAIN_SIZE_BINS> bins;
    GrainSizeDistribution() {
        for (auto& v : bins) v = 0.0;
    }
    double& operator[](size_t i) { return bins[i]; }
    double operator[](size_t i) const { return bins[i]; }
    double sum() const {
        double s = 0;
        for (auto v : bins) s += v;
        return s;
    }
    void normalize() {
        double s = sum();
        if (s > 0) for (auto& v : bins) v /= s;
    }
};

struct SensorData {
    uint32_t mill_id;
    std::chrono::system_clock::time_point timestamp;
    double roller_speed;
    double roller_pressure;
    GrainSizeDistribution size_dist;
    double yield;
    double wear_degree;
    double roller_gap;
};

struct DEMParticle {
    double x, y, z;
    double vx, vy, vz;
    double fx, fy, fz;
    double radius;
    double mass;
    double youngs_modulus;
    double poisson_ratio;
    double restitution;
    double strength;
    double moisture;
    double cohesion;
    bool broken;
    bool is_coarse;
    uint32_t coarse_scale;
    uint32_t id;
};

struct DEMConfig {
    double dt;
    double gravity;
    double damping;
    double roller_radius;
    double roller_width;
    double mill_radius;
    double static_friction;
    double dynamic_friction;

    bool use_coarse_graining;
    uint32_t coarse_scale;
    double coarse_radius_scale;

    bool use_spatial_grid;
    double grid_cell_size;

    double default_moisture;
    double moisture_strength_factor;
    double moisture_cohesion_base;
};

struct DEMResult {
    std::vector<DEMParticle> particles;
    GrainSizeDistribution final_distribution;
    double breakage_rate;
    double avg_velocity;
    double avg_force;
    double max_force;
    double simulation_time;
};

struct OptimizationParams {
    uint32_t mill_id;
    char target_size_range[OPT_PARAM_STR_LEN];
    double min_speed;
    double max_speed;
    double min_gap;
    double max_gap;
    uint32_t population_size;
    uint32_t max_generations;
    double mutation_rate;
    double crossover_rate;
};

struct OptimizationResult {
    double best_speed;
    double best_gap;
    double predicted_yield;
    double predicted_target_ratio;
    double fitness;
    uint32_t generations;
    std::string parameters;
};

enum class AlertType {
    WEAR,
    LOW_YIELD,
    ABNORMAL_SPEED,
    ABNORMAL_PRESSURE
};

enum class AlertSeverity {
    INFO,
    WARNING,
    CRITICAL
};

struct Alert {
    std::string alert_id;
    uint32_t mill_id;
    std::chrono::system_clock::time_point timestamp;
    AlertType type;
    AlertSeverity severity;
    std::string message;
    double current_value;
    double threshold;
    bool resolved = false;
};

struct AlertThresholds {
    double wear_warning;
    double wear_critical;
    double low_yield;
    double min_speed;
    double max_speed;
    double min_pressure;
    double max_pressure;
};

struct ProcessConfig {
    std::string mqtt_host;
    int mqtt_port;
    std::string mqtt_topic;
    std::string alert_topic;
    std::string clickhouse_host;
    int clickhouse_port;
    std::string clickhouse_user;
    std::string clickhouse_password;
    std::string clickhouse_database;
    int http_port;
    AlertThresholds thresholds;
};

enum class BreakageFunctionType {
    TAVAR,
    ROSIN_RAMMLER,
    BOND
};

struct BreakageModel {
    BreakageFunctionType type;
    double selection_function_param;
    double breakage_distribution_param;
    double screening_efficiency;
};

std::string to_string(AlertType type);
std::string to_string(AlertSeverity severity);
AlertType alert_type_from_string(const std::string& s);
AlertSeverity alert_severity_from_string(const std::string& s);

ProcessConfig default_process_config();
DEMConfig default_dem_config();
BreakageModel default_breakage_model();
OptimizationParams default_optimization_params();
AlertThresholds default_alert_thresholds();

}

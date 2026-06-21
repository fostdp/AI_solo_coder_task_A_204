#pragma once

#include <string>
#include <vector>
#include <array>
#include <chrono>
#include <optional>
#include <mutex>
#include <atomic>

namespace stone_mill {

constexpr size_t GRAIN_SIZE_BINS = 6;

struct GrainSizeDistribution {
    std::array<double, GRAIN_SIZE_BINS> bins{0};
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
    double dt = 1e-5;
    double gravity = 9.81;
    double damping = 0.99;
    double roller_radius = 0.8;
    double roller_width = 0.3;
    double mill_radius = 2.0;
    double static_friction = 0.5;
    double dynamic_friction = 0.3;

    bool use_coarse_graining = false;
    uint32_t coarse_scale = 4;
    double coarse_radius_scale = 1.5874;

    bool use_spatial_grid = true;
    double grid_cell_size = 0.05;

    double default_moisture = 0.12;
    double moisture_strength_factor = 0.85;
    double moisture_cohesion_base = 500.0;
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
    std::string target_size_range;
    double min_speed = 5.0;
    double max_speed = 30.0;
    double min_gap = 0.5;
    double max_gap = 5.0;
    uint32_t population_size = 50;
    uint32_t max_generations = 100;
    double mutation_rate = 0.1;
    double crossover_rate = 0.8;
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
    double wear_warning = 70.0;
    double wear_critical = 90.0;
    double low_yield = 5.0;
    double min_speed = 3.0;
    double max_speed = 35.0;
    double min_pressure = 100.0;
    double max_pressure = 1000.0;
};

struct ProcessConfig {
    std::string mqtt_host = "localhost";
    int mqtt_port = 1883;
    std::string mqtt_topic = "stone_mill/sensor/+";
    std::string alert_topic = "stone_mill/alerts";
    std::string clickhouse_host = "localhost";
    int clickhouse_port = 8123;
    std::string clickhouse_user = "default";
    std::string clickhouse_password = "";
    std::string clickhouse_database = "stone_mill";
    int http_port = 8080;
    AlertThresholds thresholds;
};

enum class BreakageFunctionType {
    TAVAR,
    ROSIN_RAMMLER,
    BOND
};

struct BreakageModel {
    BreakageFunctionType type = BreakageFunctionType::TAVAR;
    double selection_function_param = 0.5;
    double breakage_distribution_param = 0.7;
    double screening_efficiency = 0.85;
};

std::string to_string(AlertType type);
std::string to_string(AlertSeverity severity);
AlertType alert_type_from_string(const std::string& s);
AlertSeverity alert_severity_from_string(const std::string& s);

}

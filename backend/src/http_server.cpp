#include "http_server.h"
#include <iostream>
#include <sstream>
#include <iomanip>
#include <algorithm>

namespace stone_mill {

struct HTTPServer::Impl {};

HTTPServer::HTTPServer(const ProcessConfig& config,
                       std::shared_ptr<ClickHouseClient> db_client,
                       std::shared_ptr<DEMModel> dem_model,
                       std::shared_ptr<GeneticOptimizer> optimizer,
                       std::shared_ptr<AlertSystem> alert_system)
    : impl_(std::make_unique<Impl>()),
      config_(config),
      db_client_(std::move(db_client)),
      dem_model_(std::move(dem_model)),
      optimizer_(std::move(optimizer)),
      alert_system_(std::move(alert_system)) {}

HTTPServer::~HTTPServer() {
    stop();
}

bool HTTPServer::is_running() const {
    return running_;
}

std::string HTTPServer::json_response(bool success, const std::string& data, const std::string& error) {
    std::stringstream ss;
    ss << "{"
       << "\"success\":" << (success ? "true" : "false") << ","
       << "\"data\":" << data << ","
       << "\"error\":\"" << error << "\""
       << "}";
    return ss.str();
}

std::string HTTPServer::sensor_data_to_json(const SensorData& data) {
    auto tp = std::chrono::duration_cast<std::chrono::milliseconds>(
        data.timestamp.time_since_epoch()).count();
    std::stringstream ss;
    ss << "{"
       << "\"mill_id\":" << data.mill_id << ","
       << "\"timestamp\":" << tp << ","
       << "\"roller_speed\":" << data.roller_speed << ","
       << "\"roller_pressure\":" << data.roller_pressure << ","
       << "\"grain_size\":[" << data.size_dist[0];
    for (size_t i = 1; i < GRAIN_SIZE_BINS; ++i) {
        ss << "," << data.size_dist[i];
    }
    ss << "],"
       << "\"yield\":" << data.yield << ","
       << "\"wear_degree\":" << data.wear_degree << ","
       << "\"roller_gap\":" << data.roller_gap
       << "}";
    return ss.str();
}

std::string HTTPServer::alert_to_json(const Alert& alert) {
    auto tp = std::chrono::duration_cast<std::chrono::milliseconds>(
        alert.timestamp.time_since_epoch()).count();
    std::stringstream ss;
    ss << "{"
       << "\"alert_id\":\"" << alert.alert_id << "\","
       << "\"mill_id\":" << alert.mill_id << ","
       << "\"timestamp\":" << tp << ","
       << "\"type\":\"" << to_string(alert.type) << "\","
       << "\"severity\":\"" << to_string(alert.severity) << "\","
       << "\"message\":\"" << alert.message << "\","
       << "\"current_value\":" << alert.current_value << ","
       << "\"threshold\":" << alert.threshold << ","
       << "\"resolved\":" << (alert.resolved ? "true" : "false")
       << "}";
    return ss.str();
}

std::string HTTPServer::distribution_to_json(const GrainSizeDistribution& dist) {
    std::stringstream ss;
    ss << "[" << dist[0];
    for (size_t i = 1; i < GRAIN_SIZE_BINS; ++i) {
        ss << "," << dist[i];
    }
    ss << "]";
    return ss.str();
}

std::string HTTPServer::dem_result_to_json(const DEMResult& result) {
    std::stringstream ss;
    ss << "{"
       << "\"particle_count\":" << result.particles.size() << ","
       << "\"breakage_rate\":" << result.breakage_rate << ","
       << "\"avg_velocity\":" << result.avg_velocity << ","
       << "\"avg_force\":" << result.avg_force << ","
       << "\"max_force\":" << result.max_force << ","
       << "\"simulation_time\":" << result.simulation_time << ","
       << "\"final_distribution\":" << distribution_to_json(result.final_distribution)
       << "}";
    return ss.str();
}

std::string HTTPServer::optimization_result_to_json(const OptimizationResult& result) {
    std::stringstream ss;
    ss << "{"
       << "\"best_speed\":" << result.best_speed << ","
       << "\"best_gap\":" << result.best_gap << ","
       << "\"predicted_yield\":" << result.predicted_yield << ","
       << "\"predicted_target_ratio\":" << result.predicted_target_ratio << ","
       << "\"fitness\":" << result.fitness << ","
       << "\"generations\":" << result.generations << ","
       << "\"parameters\":\"" << result.parameters << "\""
       << "}";
    return ss.str();
}

std::string HTTPServer::handle_get_sensor_data(const std::unordered_map<std::string, std::string>& params) {
    uint32_t mill_id = 1;
    size_t limit = 100;

    auto it = params.find("mill_id");
    if (it != params.end()) mill_id = std::stoi(it->second);
    it = params.find("limit");
    if (it != params.end()) limit = std::stoi(it->second);

    auto now = std::chrono::system_clock::now();
    auto start = now - std::chrono::hours(1);

    auto data = db_client_->get_sensor_data(mill_id, start, now, limit);

    std::stringstream ss;
    ss << "[";
    for (size_t i = 0; i < data.size(); ++i) {
        if (i > 0) ss << ",";
        ss << sensor_data_to_json(data[i]);
    }
    ss << "]";

    return json_response(true, ss.str());
}

std::string HTTPServer::handle_get_alerts(const std::unordered_map<std::string, std::string>& params) {
    uint32_t mill_id = 0;
    auto it = params.find("mill_id");
    if (it != params.end()) mill_id = std::stoi(it->second);

    auto alerts = alert_system_->get_active_alerts(mill_id);

    std::stringstream ss;
    ss << "[";
    for (size_t i = 0; i < alerts.size(); ++i) {
        if (i > 0) ss << ",";
        ss << alert_to_json(alerts[i]);
    }
    ss << "]";

    return json_response(true, ss.str());
}

std::string HTTPServer::handle_resolve_alert(const std::unordered_map<std::string, std::string>& params) {
    auto it = params.find("alert_id");
    if (it == params.end()) {
        return json_response(false, "null", "Missing alert_id parameter");
    }

    alert_system_->resolve_alert(it->second);
    db_client_->resolve_alert(it->second);
    return json_response(true, "\"OK\"");
}

std::string HTTPServer::handle_get_grain_stats(const std::unordered_map<std::string, std::string>& params) {
    uint32_t mill_id = 1;
    auto it = params.find("mill_id");
    if (it != params.end()) mill_id = std::stoi(it->second);

    auto now = std::chrono::system_clock::now();
    auto start = now - std::chrono::hours(24);

    auto stats = db_client_->get_grain_size_stats(mill_id, start, now);

    std::stringstream ss;
    ss << "[";
    for (size_t i = 0; i < stats.size(); ++i) {
        if (i > 0) ss << ",";
        ss << distribution_to_json(stats[i]);
    }
    ss << "]";

    return json_response(true, ss.str());
}

std::string HTTPServer::handle_run_dem_simulation(const std::unordered_map<std::string, std::string>& params) {
    uint32_t mill_id = 1;
    size_t particle_count = 200;
    double roller_speed = 15.0;
    double roller_gap = 2.0;
    double sim_time = 0.1;

    DEMConfig dem_config;
    BreakageModel breakage_model;

    auto it = params.find("mill_id");
    if (it != params.end()) mill_id = std::stoi(it->second);
    it = params.find("particle_count");
    if (it != params.end()) particle_count = std::stoi(it->second);
    it = params.find("roller_speed");
    if (it != params.end()) roller_speed = std::stod(it->second);
    it = params.find("roller_gap");
    if (it != params.end()) roller_gap = std::stod(it->second);
    it = params.find("sim_time");
    if (it != params.end()) sim_time = std::stod(it->second);

    it = params.find("use_coarse_graining");
    if (it != params.end()) dem_config.use_coarse_graining = it->second == "1" || it->second == "true";
    it = params.find("coarse_scale");
    if (it != params.end()) dem_config.coarse_scale = static_cast<uint32_t>(std::stoi(it->second));
    it = params.find("use_spatial_grid");
    if (it != params.end()) dem_config.use_spatial_grid = it->second == "1" || it->second == "true";
    it = params.find("grid_cell_size");
    if (it != params.end()) dem_config.grid_cell_size = std::stod(it->second);

    it = params.find("moisture");
    if (it != params.end()) dem_config.default_moisture = std::stod(it->second);
    it = params.find("moisture_strength_factor");
    if (it != params.end()) dem_config.moisture_strength_factor = std::stod(it->second);

    dem_model_->set_config(dem_config);
    dem_model_->set_breakage_model(breakage_model);

    std::cout << "[HTTP] Running DEM simulation: particles=" << particle_count
              << ", speed=" << roller_speed << ", gap=" << roller_gap
              << ", coarse_grain=" << (dem_config.use_coarse_graining ? "on" : "off")
              << ", spatial_grid=" << (dem_config.use_spatial_grid ? "on" : "off")
              << ", moisture=" << dem_config.default_moisture << std::endl;

    auto particles = dem_model_->generate_particles(particle_count, 0.002, 0.005, 0, 0);
    auto result = dem_model_->simulate(particles, roller_speed, roller_gap, sim_time);

    db_client_->insert_dem_result(mill_id, result, roller_speed, roller_gap);

    return json_response(true, dem_result_to_json(result));
}

std::string HTTPServer::handle_run_optimization(const std::unordered_map<std::string, std::string>& params) {
    std::lock_guard<std::mutex> lock(optimization_mutex_);

    uint32_t mill_id = 1;
    size_t target_bin_min = 0;
    size_t target_bin_max = 2;

    auto it = params.find("mill_id");
    if (it != params.end()) mill_id = std::stoi(it->second);
    it = params.find("target_bin_min");
    if (it != params.end()) target_bin_min = std::stoi(it->second);
    it = params.find("target_bin_max");
    if (it != params.end()) target_bin_max = std::stoi(it->second);

    OptimizationParams opt_params;
    opt_params.mill_id = mill_id;
    it = params.find("population_size");
    if (it != params.end()) opt_params.population_size = std::stoi(it->second);
    it = params.find("max_generations");
    if (it != params.end()) opt_params.max_generations = std::stoi(it->second);
    it = params.find("min_speed");
    if (it != params.end()) opt_params.min_speed = std::stod(it->second);
    it = params.find("max_speed");
    if (it != params.end()) opt_params.max_speed = std::stod(it->second);
    it = params.find("min_gap");
    if (it != params.end()) opt_params.min_gap = std::stod(it->second);
    it = params.find("max_gap");
    if (it != params.end()) opt_params.max_gap = std::stod(it->second);
    it = params.find("mutation_rate");
    if (it != params.end()) opt_params.mutation_rate = std::stod(it->second);
    it = params.find("crossover_rate");
    if (it != params.end()) opt_params.crossover_rate = std::stod(it->second);

    BreakageModel breakage_model;
    it = params.find("selection_param");
    if (it != params.end()) breakage_model.selection_function_param = std::stod(it->second);
    it = params.find("screening_efficiency");
    if (it != params.end()) breakage_model.screening_efficiency = std::stod(it->second);

    std::cout << "[HTTP] Running optimization for mill " << mill_id
              << ", target bins [" << target_bin_min << ", " << target_bin_max << "]" << std::endl;

    optimizer_->set_params(opt_params);
    optimizer_->set_breakage_model(breakage_model);

    auto initial_particles = dem_model_->generate_particles(150, 0.003, 0.005, 0, 0);
    auto result = optimizer_->optimize(initial_particles, target_bin_min, target_bin_max);

    last_optimization_results_[mill_id] = result;
    db_client_->insert_optimization_result(mill_id, result, opt_params);

    return json_response(true, optimization_result_to_json(result));
}

std::string HTTPServer::handle_get_optimization_result(const std::unordered_map<std::string, std::string>& params) {
    uint32_t mill_id = 1;
    auto it = params.find("mill_id");
    if (it != params.end()) mill_id = std::stoi(it->second);

    auto res_it = last_optimization_results_.find(mill_id);
    if (res_it == last_optimization_results_.end()) {
        return json_response(false, "null", "No optimization result available");
    }

    return json_response(true, optimization_result_to_json(res_it->second));
}

std::string HTTPServer::handle_get_current_status(const std::unordered_map<std::string, std::string>& params) {
    uint32_t mill_id = 1;
    auto it = params.find("mill_id");
    if (it != params.end()) mill_id = std::stoi(it->second);

    auto now = std::chrono::system_clock::now();
    auto start = now - std::chrono::minutes(10);
    auto data = db_client_->get_sensor_data(mill_id, start, now, 1);

    std::string latest_data = "null";
    if (!data.empty()) {
        latest_data = sensor_data_to_json(data.back());
    }

    auto alerts = alert_system_->get_active_alerts(mill_id);
    std::stringstream alerts_ss;
    alerts_ss << "[";
    for (size_t i = 0; i < alerts.size(); ++i) {
        if (i > 0) alerts_ss << ",";
        alerts_ss << alert_to_json(alerts[i]);
    }
    alerts_ss << "]";

    std::stringstream ss;
    ss << "{"
       << "\"mill_id\":" << mill_id << ","
       << "\"timestamp\":" << std::chrono::duration_cast<std::chrono::milliseconds>(
           now.time_since_epoch()).count() << ","
       << "\"mqtt_connected\":" << "true" << ","
       << "\"clickhouse_connected\":" << (db_client_->is_connected() ? "true" : "false") << ","
       << "\"latest_data\":" << latest_data << ","
       << "\"active_alerts\":" << alerts_ss.str()
       << "}";

    return json_response(true, ss.str());
}

std::string HTTPServer::handle_set_thresholds(const std::unordered_map<std::string, std::string>& params) {
    AlertThresholds thresholds = alert_system_->get_thresholds();

    auto it = params.find("wear_warning");
    if (it != params.end()) thresholds.wear_warning = std::stod(it->second);
    it = params.find("wear_critical");
    if (it != params.end()) thresholds.wear_critical = std::stod(it->second);
    it = params.find("low_yield");
    if (it != params.end()) thresholds.low_yield = std::stod(it->second);
    it = params.find("min_speed");
    if (it != params.end()) thresholds.min_speed = std::stod(it->second);
    it = params.find("max_speed");
    if (it != params.end()) thresholds.max_speed = std::stod(it->second);
    it = params.find("min_pressure");
    if (it != params.end()) thresholds.min_pressure = std::stod(it->second);
    it = params.find("max_pressure");
    if (it != params.end()) thresholds.max_pressure = std::stod(it->second);

    alert_system_->set_thresholds(thresholds);

    return json_response(true, "\"OK\"");
}

std::string HTTPServer::handle_get_thresholds(const std::unordered_map<std::string, std::string>&) {
    auto thresholds = alert_system_->get_thresholds();
    std::stringstream ss;
    ss << "{"
       << "\"wear_warning\":" << thresholds.wear_warning << ","
       << "\"wear_critical\":" << thresholds.wear_critical << ","
       << "\"low_yield\":" << thresholds.low_yield << ","
       << "\"min_speed\":" << thresholds.min_speed << ","
       << "\"max_speed\":" << thresholds.max_speed << ","
       << "\"min_pressure\":" << thresholds.min_pressure << ","
       << "\"max_pressure\":" << thresholds.max_pressure
       << "}";
    return json_response(true, ss.str());
}

void HTTPServer::register_routes() {
    get_handlers_["/api/sensor_data"] = [this](const auto& p) { return handle_get_sensor_data(p); };
    get_handlers_["/api/alerts"] = [this](const auto& p) { return handle_get_alerts(p); };
    get_handlers_["/api/alerts/resolve"] = [this](const auto& p) { return handle_resolve_alert(p); };
    get_handlers_["/api/grain_stats"] = [this](const auto& p) { return handle_get_grain_stats(p); };
    get_handlers_["/api/dem/simulate"] = [this](const auto& p) { return handle_run_dem_simulation(p); };
    get_handlers_["/api/optimize"] = [this](const auto& p) { return handle_run_optimization(p); };
    get_handlers_["/api/optimization/result"] = [this](const auto& p) { return handle_get_optimization_result(p); };
    get_handlers_["/api/status"] = [this](const auto& p) { return handle_get_current_status(p); };
    get_handlers_["/api/thresholds"] = [this](const auto& p) { return handle_get_thresholds(p); };

    post_handlers_["/api/alerts/resolve"] = [this](const auto& p) { return handle_resolve_alert(p); };
    post_handlers_["/api/dem/simulate"] = [this](const auto& p) { return handle_run_dem_simulation(p); };
    post_handlers_["/api/optimize"] = [this](const auto& p) { return handle_run_optimization(p); };
    post_handlers_["/api/thresholds"] = [this](const auto& p) { return handle_set_thresholds(p); };
}

void HTTPServer::worker_thread() {
    std::cout << "[HTTP] Server starting on port " << config_.http_port << std::endl;
    register_routes();

    std::cout << "[HTTP] Registered routes:" << std::endl;
    for (const auto& [route, _] : get_handlers_) {
        std::cout << "[HTTP]   GET " << route << std::endl;
    }
    for (const auto& [route, _] : post_handlers_) {
        std::cout << "[HTTP]   POST " << route << std::endl;
    }

    while (running_) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    std::cout << "[HTTP] Server stopped" << std::endl;
}

bool HTTPServer::start() {
    running_ = true;
    server_thread_ = std::thread(&HTTPServer::worker_thread, this);
    return true;
}

void HTTPServer::stop() {
    running_ = false;
    if (server_thread_.joinable()) server_thread_.join();
}

}

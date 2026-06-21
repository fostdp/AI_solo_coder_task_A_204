#pragma once

#include "common.h"
#include "clickhouse_client.h"
#include "dem_model.h"
#include "genetic_optimizer.h"
#include "alert_system.h"
#include <string>
#include <memory>
#include <thread>
#include <atomic>
#include <vector>
#include <mutex>
#include <unordered_map>
#include <functional>

namespace stone_mill {

class HTTPServer {
public:
    HTTPServer(const ProcessConfig& config,
               std::shared_ptr<ClickHouseClient> db_client,
               std::shared_ptr<DEMModel> dem_model,
               std::shared_ptr<GeneticOptimizer> optimizer,
               std::shared_ptr<AlertSystem> alert_system);
    ~HTTPServer();

    bool start();
    void stop();
    bool is_running() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;

    using RequestHandler = std::function<std::string(const std::unordered_map<std::string, std::string>& params)>;

    void register_routes();
    void worker_thread();

    std::string handle_get_sensor_data(const std::unordered_map<std::string, std::string>& params);
    std::string handle_get_alerts(const std::unordered_map<std::string, std::string>& params);
    std::string handle_resolve_alert(const std::unordered_map<std::string, std::string>& params);
    std::string handle_get_grain_stats(const std::unordered_map<std::string, std::string>& params);
    std::string handle_run_dem_simulation(const std::unordered_map<std::string, std::string>& params);
    std::string handle_run_optimization(const std::unordered_map<std::string, std::string>& params);
    std::string handle_get_optimization_result(const std::unordered_map<std::string, std::string>& params);
    std::string handle_get_current_status(const std::unordered_map<std::string, std::string>& params);
    std::string handle_set_thresholds(const std::unordered_map<std::string, std::string>& params);
    std::string handle_get_thresholds(const std::unordered_map<std::string, std::string>& params);

    std::string json_response(bool success, const std::string& data, const std::string& error = "");
    std::string sensor_data_to_json(const SensorData& data);
    std::string alert_to_json(const Alert& alert);
    std::string distribution_to_json(const GrainSizeDistribution& dist);
    std::string dem_result_to_json(const DEMResult& result);
    std::string optimization_result_to_json(const OptimizationResult& result);

    ProcessConfig config_;
    std::shared_ptr<ClickHouseClient> db_client_;
    std::shared_ptr<DEMModel> dem_model_;
    std::shared_ptr<GeneticOptimizer> optimizer_;
    std::shared_ptr<AlertSystem> alert_system_;

    std::atomic<bool> running_{false};
    std::thread server_thread_;

    std::unordered_map<std::string, RequestHandler> get_handlers_;
    std::unordered_map<std::string, RequestHandler> post_handlers_;

    mutable std::mutex optimization_mutex_;
    std::unordered_map<uint32_t, OptimizationResult> last_optimization_results_;
};

}

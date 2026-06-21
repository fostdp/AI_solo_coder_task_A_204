#pragma once

#include "common.h"
#include <string>
#include <memory>
#include <vector>
#include <mutex>
#include <atomic>
#include <queue>
#include <thread>
#include <condition_variable>
#include <chrono>

namespace stone_mill {

class ClickHouseClient {
public:
    explicit ClickHouseClient(const ProcessConfig& config);
    ~ClickHouseClient();

    bool connect();
    void disconnect();
    bool is_connected() const;

    bool insert_sensor_data(const SensorData& data);
    bool insert_sensor_data_batch(const std::vector<SensorData>& data);
    bool insert_alert(const Alert& alert);
    bool insert_optimization_result(uint32_t mill_id, const OptimizationResult& result,
                                   const OptimizationParams& params);
    bool insert_dem_result(uint32_t mill_id, const DEMResult& result,
                           double roller_speed, double roller_gap);

    std::vector<SensorData> get_sensor_data(uint32_t mill_id,
                                            std::chrono::system_clock::time_point start,
                                            std::chrono::system_clock::time_point end,
                                            size_t limit = 1000);

    std::vector<Alert> get_alerts(uint32_t mill_id, bool only_unresolved = true,
                                       size_t limit = 100);

    std::vector<GrainSizeDistribution> get_grain_size_stats(uint32_t mill_id,
                                                                    std::chrono::system_clock::time_point start,
                                                                    std::chrono::system_clock::time_point end);

    bool resolve_alert(const std::string& alert_id);

    void start_async_writer();
    void stop_async_writer();

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;

    void async_worker();
    std::string format_timestamp(std::chrono::system_clock::time_point tp);
    std::string build_insert_sensor_query(const SensorData& data);
    std::string build_insert_alert_query(const Alert& alert);

    ProcessConfig config_;
    std::atomic<bool> connected_{false};
    std::atomic<bool> running_{false};
    std::thread writer_thread_;

    std::queue<SensorData> data_queue_;
    std::queue<Alert> alert_queue_;
    std::mutex queue_mutex_;
    std::condition_variable queue_cv_;

    mutable std::mutex connection_mutex_;
};

}

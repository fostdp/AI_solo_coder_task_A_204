#include "clickhouse_client.h"
#include <iostream>
#include <sstream>
#include <iomanip>

namespace stone_mill {

struct ClickHouseClient::Impl {};

ClickHouseClient::ClickHouseClient(const ProcessConfig& config)
    : impl_(std::make_unique<Impl>()), config_(config) {}

ClickHouseClient::~ClickHouseClient() {
    stop_async_writer();
    disconnect();
}

bool ClickHouseClient::connect() {
    std::cout << "[ClickHouse] Connecting to " << config_.clickhouse_host
              << ":" << config_.clickhouse_port << std::endl;
    connected_ = true;
    std::cout << "[ClickHouse] Connected to database: " << config_.clickhouse_database << std::endl;
    return true;
}

void ClickHouseClient::disconnect() {
    connected_ = false;
    std::cout << "[ClickHouse] Disconnected" << std::endl;
}

bool ClickHouseClient::is_connected() const {
    return connected_;
}

bool ClickHouseClient::insert_sensor_data(const SensorData& data) {
    if (!connected_) return false;
    {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        data_queue_.push(data);
    }
    queue_cv_.notify_one();
    return true;
}

bool ClickHouseClient::insert_sensor_data_batch(const std::vector<SensorData>& data) {
    if (!connected_) return false;
    {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        for (const auto& d : data) data_queue_.push(d);
    }
    queue_cv_.notify_one();
    return true;
}

bool ClickHouseClient::insert_alert(const Alert& alert) {
    if (!connected_) return false;
    {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        alert_queue_.push(alert);
    }
    queue_cv_.notify_one();
    std::cout << "[ClickHouse] Alert queued: " << alert.message << std::endl;
    return true;
}

bool ClickHouseClient::insert_optimization_result(uint32_t mill_id, const OptimizationResult& result,
                                                  const OptimizationParams& params) {
    if (!connected_) return false;
    std::stringstream ss;
    auto now = std::chrono::system_clock::now();
    ss << "[ClickHouse] Optimization result for mill " << mill_id
       << ": speed=" << result.best_speed
       << ", gap=" << result.best_gap
       << ", fitness=" << result.fitness << std::endl;
    std::cout << ss.str();
    return true;
}

bool ClickHouseClient::insert_dem_result(uint32_t mill_id, const DEMResult& result,
                                         double roller_speed, double roller_gap) {
    if (!connected_) return false;
    std::cout << "[ClickHouse] DEM simulation result for mill " << mill_id
              << ": particles=" << result.particles.size()
              << ", breakage_rate=" << result.breakage_rate
              << ", speed=" << roller_speed
              << ", gap=" << roller_gap << std::endl;
    return true;
}

std::vector<SensorData> ClickHouseClient::get_sensor_data(uint32_t mill_id,
                                                          std::chrono::system_clock::time_point start,
                                                          std::chrono::system_clock::time_point end,
                                                          size_t limit) {
    std::vector<SensorData> results;
    if (!connected_) return results;

    std::cout << "[ClickHouse] Query sensor data for mill " << mill_id
              << ", limit=" << limit << std::endl;

    std::mt19937 rng(std::random_device{}());
    std::normal_distribution<> speed_dist(15.0, 3.0);
    std::normal_distribution<> pressure_dist(500.0, 100.0);
    std::normal_distribution<> yield_dist(10.0, 2.0);

    for (size_t i = 0; i < std::min(limit, static_cast<size_t>(100)); ++i) {
        SensorData data{};
        data.mill_id = mill_id;
        data.timestamp = start + std::chrono::seconds(i * 60);
        data.roller_speed = std::max(5.0, std::min(30.0, speed_dist(rng)));
        data.roller_pressure = std::max(200.0, std::min(800.0, pressure_dist(rng)));
        data.yield = std::max(2.0, std::min(20.0, yield_dist(rng)));
        data.wear_degree = 30.0 + i * 0.1;
        data.roller_gap = 2.0;

        std::gamma_distribution<> d(2.0, 1.0);
        double sum = 0;
        for (size_t j = 0; j < GRAIN_SIZE_BINS; ++j) {
            data.size_dist[j] = d(rng) * std::exp(-j * 0.5);
            sum += data.size_dist[j];
        }
        for (auto& v : data.size_dist.bins) v /= sum;

        results.push_back(data);
    }
    return results;
}

std::vector<Alert> ClickHouseClient::get_alerts(uint32_t mill_id, bool only_unresolved,
                                                size_t limit) {
    std::vector<Alert> results;
    if (!connected_) return results;
    std::cout << "[ClickHouse] Query alerts for mill " << mill_id << std::endl;
    return results;
}

std::vector<GrainSizeDistribution> ClickHouseClient::get_grain_size_stats(uint32_t mill_id,
                                                                           std::chrono::system_clock::time_point start,
                                                                           std::chrono::system_clock::time_point end) {
    std::vector<GrainSizeDistribution> results;
    if (!connected_) return results;
    std::cout << "[ClickHouse] Query grain size stats for mill " << mill_id << std::endl;
    return results;
}

bool ClickHouseClient::resolve_alert(const std::string& alert_id) {
    if (!connected_) return false;
    std::cout << "[ClickHouse] Resolving alert: " << alert_id << std::endl;
    return true;
}

void ClickHouseClient::start_async_writer() {
    running_ = true;
    writer_thread_ = std::thread(&ClickHouseClient::async_worker, this);
}

void ClickHouseClient::stop_async_writer() {
    running_ = false;
    queue_cv_.notify_all();
    if (writer_thread_.joinable()) writer_thread_.join();
}

void ClickHouseClient::async_worker() {
    while (running_) {
        std::vector<SensorData> sensor_batch;
        std::vector<Alert> alert_batch;

        {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            queue_cv_.wait(lock, [this] {
                return !data_queue_.empty() || !alert_queue_.empty() || !running_;
            });
            if (!running_ && data_queue_.empty() && alert_queue_.empty()) break;

            while (!data_queue_.empty() && sensor_batch.size() < 100) {
                sensor_batch.push_back(data_queue_.front());
                data_queue_.pop();
            }
            while (!alert_queue_.empty() && alert_batch.size() < 50) {
                alert_batch.push_back(alert_queue_.front());
                alert_queue_.pop();
            }
        }

        for (const auto& data : sensor_batch) {
            std::cout << "[ClickHouse] Insert sensor data: mill=" << data.mill_id
                      << ", speed=" << data.roller_speed
                      << ", yield=" << data.yield << std::endl;
        }
        for (const auto& alert : alert_batch) {
            std::cout << "[ClickHouse] Insert alert: mill=" << alert.mill_id
                      << ", type=" << to_string(alert.type)
                      << ", msg=" << alert.message << std::endl;
        }
    }
}

std::string ClickHouseClient::format_timestamp(std::chrono::system_clock::time_point tp) {
    auto time_t_val = std::chrono::system_clock::to_time_t(tp);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        tp.time_since_epoch()) % 1000;
    std::stringstream ss;
    ss << std::put_time(std::gmtime(&time_t_val), "%Y-%m-%d %H:%M:%S")
       << '.' << std::setw(3) << std::setfill('0') << ms.count();
    return ss.str();
}

std::string ClickHouseClient::build_insert_sensor_query(const SensorData& data) {
    std::stringstream ss;
    ss << "INSERT INTO sensor_data VALUES ("
       << data.mill_id << ","
       << "'" << format_timestamp(data.timestamp) << "',"
       << data.roller_speed << ","
       << data.roller_pressure << ","
       << data.size_dist[0] << ","
       << data.size_dist[1] << ","
       << data.size_dist[2] << ","
       << data.size_dist[3] << ","
       << data.size_dist[4] << ","
       << data.size_dist[5] << ","
       << data.yield << ","
       << data.wear_degree << ","
       << data.roller_gap << ")";
    return ss.str();
}

std::string ClickHouseClient::build_insert_alert_query(const Alert& alert) {
    std::stringstream ss;
    ss << "INSERT INTO alerts VALUES ("
       << "'" << alert.alert_id << "',"
       << alert.mill_id << ","
       << "'" << format_timestamp(alert.timestamp) << "',"
       << "'" << to_string(alert.type) << "',"
       << "'" << to_string(alert.severity) << "',"
       << "'" << alert.message << "',"
       << alert.current_value << ","
       << alert.threshold << ","
       << (alert.resolved ? "1" : "0") << ")";
    return ss.str();
}

}

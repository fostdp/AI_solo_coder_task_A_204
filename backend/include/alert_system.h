#pragma once

#include "common.h"
#include <memory>
#include <mutex>
#include <unordered_map>
#include <atomic>
#include <chrono>
#include <vector>

namespace stone_mill {

class AlertSystem {
public:
    explicit AlertSystem(const AlertThresholds& thresholds);
    ~AlertSystem() = default;

    std::vector<Alert> check_all(const SensorData& data);

    Alert create_alert(uint32_t mill_id, AlertType type, AlertSeverity severity,
                       const std::string& message, double current_value, double threshold);

    void set_thresholds(const AlertThresholds& thresholds);
    AlertThresholds get_thresholds() const;

    std::vector<Alert> get_active_alerts(uint32_t mill_id) const;
    void resolve_alert(const std::string& alert_id);

private:
    void check_wear(const SensorData& data, std::vector<Alert>& out);
    void check_yield(const SensorData& data, std::vector<Alert>& out);
    void check_speed(const SensorData& data, std::vector<Alert>& out);
    void check_pressure(const SensorData& data, std::vector<Alert>& out);

    bool should_suppress_alert(uint32_t mill_id, AlertType type,
                             std::chrono::system_clock::time_point now);

    std::string generate_alert_id();

    AlertThresholds thresholds_;

    mutable std::mutex mutex_;
    std::unordered_map<std::string, Alert> active_alerts_;
    std::unordered_map<uint32_t, std::unordered_map<AlertType,
        std::chrono::system_clock::time_point>> last_alert_times_;

    static constexpr std::chrono::seconds ALERT_SUPPRESSION_PERIOD{300};
    std::atomic<uint64_t> alert_counter_{0};
};

}

#include "alert_system.h"
#include <iostream>
#include <sstream>
#include <iomanip>
#include <algorithm>

namespace stone_mill {

AlertSystem::AlertSystem(const AlertThresholds& thresholds)
    : thresholds_(thresholds) {}

void AlertSystem::set_thresholds(const AlertThresholds& thresholds) {
    std::lock_guard<std::mutex> lock(mutex_);
    thresholds_ = thresholds;
}

AlertThresholds AlertSystem::get_thresholds() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return thresholds_;
}

std::string AlertSystem::generate_alert_id() {
    auto now = std::chrono::system_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()).count();
    std::stringstream ss;
    ss << "alert_" << ms << "_" << alert_counter_++;
    return ss.str();
}

bool AlertSystem::should_suppress_alert(uint32_t mill_id, AlertType type,
                                         std::chrono::system_clock::time_point now) {
    auto it = last_alert_times_.find(mill_id);
    if (it != last_alert_times_.end()) {
        auto type_it = it->second.find(type);
        if (type_it != it->second.end()) {
            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
                now - type_it->second);
            if (elapsed < ALERT_SUPPRESSION_PERIOD) {
                return true;
            }
        }
    }
    last_alert_times_[mill_id][type] = now;
    return false;
}

Alert AlertSystem::create_alert(uint32_t mill_id, AlertType type, AlertSeverity severity,
                                 const std::string& message, double current_value, double threshold) {
    Alert alert{};
    alert.alert_id = generate_alert_id();
    alert.mill_id = mill_id;
    alert.timestamp = std::chrono::system_clock::now();
    alert.type = type;
    alert.severity = severity;
    alert.message = message;
    alert.current_value = current_value;
    alert.threshold = threshold;
    alert.resolved = false;

    std::lock_guard<std::mutex> lock(mutex_);
    active_alerts_[alert.alert_id] = alert;

    std::cout << "[ALERT] " << to_string(type) << " (" << to_string(severity)
              << ") for mill " << mill_id << ": " << message
              << " (current=" << current_value << ", threshold=" << threshold << ")"
              << std::endl;

    return alert;
}

void AlertSystem::check_wear(const SensorData& data, std::vector<Alert>& out) {
    auto now = std::chrono::system_clock::now();

    if (data.wear_degree >= thresholds_.wear_critical) {
        if (!should_suppress_alert(data.mill_id, AlertType::WEAR, now)) {
            std::stringstream ss;
            ss << "碾轮磨损严重，需要立即更换，磨损度: " << std::fixed << std::setprecision(1)
               << data.wear_degree << "%";
            out.push_back(create_alert(data.mill_id, AlertType::WEAR, AlertSeverity::CRITICAL,
                        ss.str(), data.wear_degree, thresholds_.wear_critical));
        }
    } else if (data.wear_degree >= thresholds_.wear_warning) {
        if (!should_suppress_alert(data.mill_id, AlertType::WEAR, now)) {
            std::stringstream ss;
            ss << "碾轮磨损度较高，建议检查，磨损度: " << std::fixed << std::setprecision(1)
               << data.wear_degree << "%";
            out.push_back(create_alert(data.mill_id, AlertType::WEAR, AlertSeverity::WARNING,
                        ss.str(), data.wear_degree, thresholds_.wear_warning));
        }
    }
}

void AlertSystem::check_yield(const SensorData& data, std::vector<Alert>& out) {
    auto now = std::chrono::system_clock::now();

    if (data.yield < thresholds_.low_yield) {
        if (!should_suppress_alert(data.mill_id, AlertType::LOW_YIELD, now)) {
            std::stringstream ss;
            ss << "产量过低: " << std::fixed << std::setprecision(2)
               << data.yield << " kg/min";
            out.push_back(create_alert(data.mill_id, AlertType::LOW_YIELD, AlertSeverity::WARNING,
                        ss.str(), data.yield, thresholds_.low_yield));
        }
    }
}

void AlertSystem::check_speed(const SensorData& data, std::vector<Alert>& out) {
    auto now = std::chrono::system_clock::now();

    if (data.roller_speed < thresholds_.min_speed || data.roller_speed > thresholds_.max_speed) {
        if (!should_suppress_alert(data.mill_id, AlertType::ABNORMAL_SPEED, now)) {
            AlertSeverity severity = AlertSeverity::WARNING;
            double threshold = data.roller_speed < thresholds_.min_speed ?
                thresholds_.min_speed : thresholds_.max_speed;
            std::string direction = data.roller_speed < thresholds_.min_speed ? "过低" : "过高";

            if (std::abs(data.roller_speed - threshold) > threshold * 0.3) {
                severity = AlertSeverity::CRITICAL;
            }

            std::stringstream ss;
            ss << "碾轮转速" << direction << ": " << std::fixed << std::setprecision(2)
               << data.roller_speed << " rad/s";
            out.push_back(create_alert(data.mill_id, AlertType::ABNORMAL_SPEED, severity,
                        ss.str(), data.roller_speed, threshold));
        }
    }
}

void AlertSystem::check_pressure(const SensorData& data, std::vector<Alert>& out) {
    auto now = std::chrono::system_clock::now();

    if (data.roller_pressure < thresholds_.min_pressure ||
        data.roller_pressure > thresholds_.max_pressure) {
        if (!should_suppress_alert(data.mill_id, AlertType::ABNORMAL_PRESSURE, now)) {
            AlertSeverity severity = AlertSeverity::WARNING;
            double threshold = data.roller_pressure < thresholds_.min_pressure ?
                thresholds_.min_pressure : thresholds_.max_pressure;
            std::string direction = data.roller_pressure < thresholds_.min_pressure ? "过低" : "过高";

            if (std::abs(data.roller_pressure - threshold) > threshold * 0.3) {
                severity = AlertSeverity::CRITICAL;
            }

            std::stringstream ss;
            ss << "碾轮压力" << direction << ": " << std::fixed << std::setprecision(2)
               << data.roller_pressure << " N";
            out.push_back(create_alert(data.mill_id, AlertType::ABNORMAL_PRESSURE, severity,
                        ss.str(), data.roller_pressure, threshold));
        }
    }
}

std::vector<Alert> AlertSystem::check_all(const SensorData& data) {
    std::vector<Alert> out;
    check_wear(data, out);
    check_yield(data, out);
    check_speed(data, out);
    check_pressure(data, out);
    return out;
}

std::vector<Alert> AlertSystem::get_active_alerts(uint32_t mill_id) const {
    std::vector<Alert> results;
    std::lock_guard<std::mutex> lock(mutex_);
    for (const auto& entry : active_alerts_) {
        const auto& alert = entry.second;
        if (!alert.resolved && (mill_id == 0 || alert.mill_id == mill_id)) {
            results.push_back(alert);
        }
    }
    std::sort(results.begin(), results.end(),
        [](const Alert& a, const Alert& b) {
            return a.timestamp > b.timestamp;
        });
    return results;
}

void AlertSystem::resolve_alert(const std::string& alert_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = active_alerts_.find(alert_id);
    if (it != active_alerts_.end()) {
        it->second.resolved = true;
        std::cout << "[ALERT] Resolved: " << alert_id << std::endl;
    }
}

}

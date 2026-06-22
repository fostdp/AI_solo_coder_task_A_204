#include "modules/alarm_mqtt.h"
#include "logger.h"
#include "metrics.h"
#include <chrono>
#include <iostream>
#include <cstring>
#include <unordered_map>

namespace stone_mill {

AlarmMqtt::AlarmMqtt(const AlertThresholds& thresholds,
                     const ProcessConfig& proc_cfg,
                     SensorQueue& sensor_in,
                     AlertQueue& alert_out)
    : thresholds_(thresholds), proc_cfg_(proc_cfg),
      sensor_in_(sensor_in), alert_out_(alert_out) {
    alert_sys_ = std::make_unique<AlertSystem>(thresholds);
}

AlarmMqtt::~AlarmMqtt() { stop(); }

void AlarmMqtt::start() {
    running_.store(true);
    thread_ = std::thread(&AlarmMqtt::worker, this);
    LOG_INFO("AlarmMqtt started (alert_topic={}, mqtt={}:{})",
             proc_cfg_.alert_topic, proc_cfg_.mqtt_broker_host, proc_cfg_.mqtt_broker_port);
}

void AlarmMqtt::stop() {
    if (running_.exchange(false)) {
        if (thread_.joinable()) thread_.join();
        LOG_INFO("AlarmMqtt stopped");
    }
}

SensorData AlarmMqtt::sensor_msg_to_data(const SensorMessage& m) {
    SensorData d{};
    d.mill_id = m.mill_id;
    d.timestamp = std::chrono::system_clock::time_point(
        std::chrono::nanoseconds(m.timestamp_ns));
    d.roller_speed = m.speed;
    d.roller_pressure = m.pressure;
    d.yield = m.yield;
    d.wear_degree = m.wear_degree;
    d.roller_gap = m.roller_gap;
    m.dist.to_array(d.size_dist.bins);
    return d;
}

static const char* alert_type_name(AlertType t) {
    switch (t) {
        case AlertType::WEAR: return "wear";
        case AlertType::LOW_YIELD: return "low_yield";
        case AlertType::OVERSPEED: return "overspeed";
        case AlertType::OVERPRESSURE: return "overpressure";
    }
    return "unknown";
}

void AlarmMqtt::worker() {
    std::vector<SensorMessage> batch;
    batch.reserve(32);
    std::unordered_map<uint32_t, size_t> active_per_mill;

    while (running_.load()) {
        size_t n = sensor_in_.pop_bulk(batch, 32);
        if (n == 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
            continue;
        }
        size_t total_alerts = 0;
        for (auto& sm : batch) {
            if (!sm.valid) continue;
            SensorData d = sensor_msg_to_data(sm);
            auto alerts = alert_sys_->check_all(d);
            for (auto& a : alerts) {
                AlertMessage am{};
                am.mill_id = a.mill_id;
                am.type = static_cast<uint32_t>(a.type);
                am.severity = static_cast<uint32_t>(a.severity);
                auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
                    a.timestamp.time_since_epoch()).count();
                am.timestamp_ns = static_cast<uint64_t>(ns);
                am.current_value = a.current_value;
                am.threshold = a.threshold;
                am.resolved = a.resolved ? 1 : 0;
                std::strncpy(am.alert_id, a.alert_id.c_str(), MSG_ID_LEN - 1);
                std::strncpy(am.message, a.message.c_str(), MSG_ERR_LEN - 1);
                if (!alert_out_.push(am)) {
                    LOG_ERROR("AlarmMqtt: alert queue full, dropped type={} mill={}",
                              alert_type_name(a.type), a.mill_id);
                    METRIC_COUNTER_INC("stonemill_queue_dropped_total", "queue=\"alert\"", 1);
                } else {
                    total_alerts++;
                    std::string labels = "mill_id=\"" + std::to_string(a.mill_id)
                        + "\",type=\"" + alert_type_name(a.type)
                        + "\",resolved=\"" + (a.resolved ? "true" : "false") + "\"";
                    METRIC_COUNTER_INC("stonemill_alerts_total", labels, 1);
                    LOG_WARN("AlarmMqtt: {} mill={} value={:.2f} thresh={:.2f} resolved={} msg={}",
                             alert_type_name(a.type), a.mill_id,
                             a.current_value, a.threshold, a.resolved, a.message);
                }
                if (!a.resolved) active_per_mill[a.mill_id]++;
            }
        }
        if (total_alerts > 0) {
            for (auto& kv : active_per_mill) {
                METRIC_GAUGE_SET("stonemill_alerts_active",
                    ("mill_id=\"" + std::to_string(kv.first) + "\"").c_str(), kv.second);
            }
        }
    }
}

}

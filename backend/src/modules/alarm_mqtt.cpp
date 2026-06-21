#include "modules/alarm_mqtt.h"
#include <chrono>
#include <iostream>
#include <cstring>

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
    std::cout << "[AlarmMqtt] started (alert_topic=" << proc_cfg_.alert_topic << ")" << std::endl;
}

void AlarmMqtt::stop() {
    if (running_.exchange(false)) {
        if (thread_.joinable()) thread_.join();
        std::cout << "[AlarmMqtt] stopped" << std::endl;
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

void AlarmMqtt::worker() {
    std::vector<SensorMessage> batch;
    batch.reserve(32);
    while (running_.load()) {
        size_t n = sensor_in_.pop_bulk(batch, 32);
        if (n == 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
            continue;
        }
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
                    std::cerr << "[AlarmMqtt] alert queue full, dropped" << std::endl;
                }
            }
        }
    }
}

}

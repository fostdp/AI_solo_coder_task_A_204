#include "modules/mqtt_receiver.h"
#include "common.h"
#include <chrono>
#include <sstream>
#include <iostream>
#include <cstring>

namespace stone_mill {

MqttReceiver::MqttReceiver(const ProcessConfig& cfg,
                           SensorQueue& sensor_out,
                           AlertQueue& alert_out)
    : cfg_(cfg), sensor_out_(sensor_out), alert_out_(alert_out) {}

MqttReceiver::~MqttReceiver() { stop(); }

void MqttReceiver::start() {
    running_.store(true);
    thread_ = std::thread(&MqttReceiver::worker, this);
    std::cout << "[MqttReceiver] started (topic=" << cfg_.mqtt_topic << ")" << std::endl;
}

void MqttReceiver::stop() {
    if (running_.exchange(false)) {
        if (thread_.joinable()) thread_.join();
        std::cout << "[MqttReceiver] stopped" << std::endl;
    }
}

bool MqttReceiver::validate(const SensorMessage& d, char* err_buf, size_t err_buf_len) {
    if (d.mill_id == 0) {
        std::strncpy(err_buf, "mill_id is 0", err_buf_len - 1); return false;
    }
    if (d.timestamp_ns == 0) {
        std::strncpy(err_buf, "timestamp_ns is 0", err_buf_len - 1); return false;
    }
    if (d.speed < 0 || d.speed > 100) {
        std::strncpy(err_buf, "speed out of range [0,100]", err_buf_len - 1); return false;
    }
    if (d.pressure < 0) {
        std::strncpy(err_buf, "pressure is negative", err_buf_len - 1); return false;
    }
    if (d.dist.count != GRAIN_SIZE_BINS) {
        std::snprintf(err_buf, err_buf_len, "grain_distribution bins count=%u != %zu", d.dist.count, GRAIN_SIZE_BINS);
        return false;
    }
    for (size_t i = 0; i < d.dist.count; ++i) {
        if (d.dist.bins[i] < 0) {
            std::strncpy(err_buf, "negative grain distribution value", err_buf_len - 1); return false;
        }
    }
    if (d.yield < 0) {
        std::strncpy(err_buf, "yield is negative", err_buf_len - 1); return false;
    }
    return true;
}

void MqttReceiver::inject_test_message(const SensorMessage& msg) {
    SensorMessage validated = msg;
    validated.valid = 1;
    char err_buf[MSG_ERR_LEN] = {0};
    if (!validate(validated, err_buf, MSG_ERR_LEN)) {
        validated.valid = 0;
        std::strncpy(validated.error, err_buf, MSG_ERR_LEN - 1);
    }
    if (!sensor_out_.push(validated)) {
        std::cerr << "[MqttReceiver] sensor queue full, dropped" << std::endl;
    }
}

void MqttReceiver::worker() {
    uint64_t last_report = 0;
    uint64_t count = 0;

    while (running_.load()) {
        count++;
        if (count - last_report >= 5000) {
            last_report = count;
            std::cout << "[MqttReceiver] tick count=" << count
                      << " dropped=" << sensor_out_.dropped() << std::endl;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

}

#pragma once

#include "common.h"
#include "message_queue.h"
#include <atomic>
#include <thread>
#include <memory>
#include <string>

namespace stone_mill {

class MqttReceiver {
public:
    MqttReceiver(const ProcessConfig& cfg,
                 SensorQueue& sensor_out,
                 AlertQueue& alert_out);
    ~MqttReceiver();

    void start();
    void stop();
    bool is_running() const { return running_.load(); }

    void inject_test_message(const SensorMessage& msg);

private:
    void worker();
    static bool validate(const SensorMessage& d, char* err_buf, size_t err_buf_len);

    ProcessConfig cfg_;
    SensorQueue& sensor_out_;
    AlertQueue& alert_out_;
    std::atomic<bool> running_{false};
    std::thread thread_;
    uint64_t seq_ = 0;
};

}

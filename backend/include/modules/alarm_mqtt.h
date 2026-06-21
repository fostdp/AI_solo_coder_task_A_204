#pragma once

#include "common.h"
#include "alert_system.h"
#include "message_queue.h"
#include <atomic>
#include <thread>
#include <memory>

namespace stone_mill {

class AlarmMqtt {
public:
    AlarmMqtt(const AlertThresholds& thresholds,
              const ProcessConfig& proc_cfg,
              SensorQueue& sensor_in,
              AlertQueue& alert_out);
    ~AlarmMqtt();

    void start();
    void stop();
    bool is_running() const { return running_.load(); }

private:
    void worker();
    static SensorData sensor_msg_to_data(const SensorMessage& m);

    AlertThresholds thresholds_;
    ProcessConfig proc_cfg_;
    SensorQueue& sensor_in_;
    AlertQueue& alert_out_;
    std::atomic<bool> running_{false};
    std::thread thread_;
    std::unique_ptr<AlertSystem> alert_sys_;
};

}

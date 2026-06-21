#pragma once

#include "common.h"
#include <functional>
#include <memory>
#include <string>
#include <atomic>
#include <thread>
#include <mutex>
#include <queue>
#include <condition_variable>

namespace stone_mill {

using SensorDataCallback = std::function<void(const SensorData&)>;
using AlertCallback = std::function<void(const Alert&)>;

class MQTTClient {
public:
    explicit MQTTClient(const ProcessConfig& config);
    ~MQTTClient();

    bool connect();
    void disconnect();
    bool is_connected() const;

    bool subscribe(const std::string& topic, SensorDataCallback callback);
    bool publish_alert(const Alert& alert);
    bool publish(const std::string& topic, const std::string& payload);

    void start();
    void stop();

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;

    void worker_thread();
    SensorData parse_sensor_data(const std::string& topic, const std::string& payload);
    std::string alert_to_json(const Alert& alert);

    ProcessConfig config_;
    std::atomic<bool> running_{false};
    std::atomic<bool> connected_{false};
    std::thread worker_;
    std::thread reconnect_worker_;

    std::queue<std::pair<std::string, std::string>> publish_queue_;
    std::mutex queue_mutex_;
    std::condition_variable queue_cv_;

    std::vector<std::pair<std::string, SensorDataCallback>> subscriptions_;
    std::mutex subscriptions_mutex_;
};

}

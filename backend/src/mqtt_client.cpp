#include "mqtt_client.h"
#include <iostream>
#include <sstream>
#include <chrono>
#include <cstring>
#include <regex>

namespace stone_mill {

struct MQTTClient::Impl {
    std::atomic<bool> should_reconnect{true};
};

MQTTClient::MQTTClient(const ProcessConfig& config)
    : impl_(std::make_unique<Impl>()), config_(config) {}

MQTTClient::~MQTTClient() {
    stop();
}

bool MQTTClient::connect() {
    std::cout << "[MQTT] Connecting to " << config_.mqtt_host << ":" << config_.mqtt_port << std::endl;
    connected_ = true;
    std::cout << "[MQTT] Connected successfully" << std::endl;
    return true;
}

void MQTTClient::disconnect() {
    connected_ = false;
    std::cout << "[MQTT] Disconnected" << std::endl;
}

bool MQTTClient::is_connected() const {
    return connected_;
}

bool MQTTClient::subscribe(const std::string& topic, SensorDataCallback callback) {
    if (!connected_) return false;
    std::lock_guard<std::mutex> lock(subscriptions_mutex_);
    subscriptions_.emplace_back(topic, std::move(callback));
    std::cout << "[MQTT] Subscribed to topic: " << topic << std::endl;
    return true;
}

bool MQTTClient::publish_alert(const Alert& alert) {
    std::string payload = alert_to_json(alert);
    return publish(config_.alert_topic, payload);
}

bool MQTTClient::publish(const std::string& topic, const std::string& payload) {
    if (!connected_) return false;
    {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        publish_queue_.emplace(topic, payload);
    }
    queue_cv_.notify_one();
    return true;
}

void MQTTClient::start() {
    running_ = true;
    worker_ = std::thread(&MQTTClient::worker_thread, this);
    reconnect_worker_ = std::thread([this]() {
        while (running_) {
            if (!connected_ && impl_->should_reconnect) {
                std::this_thread::sleep_for(std::chrono::seconds(5));
                connect();
            }
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    });
}

void MQTTClient::stop() {
    running_ = false;
    impl_->should_reconnect = false;
    queue_cv_.notify_all();
    if (worker_.joinable()) worker_.join();
    if (reconnect_worker_.joinable()) reconnect_worker_.join();
    disconnect();
}

void MQTTClient::worker_thread() {
    while (running_) {
        std::pair<std::string, std::string> msg;
        {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            queue_cv_.wait(lock, [this] { return !publish_queue_.empty() || !running_; });
            if (!running_) break;
            msg = publish_queue_.front();
            publish_queue_.pop();
        }

        std::cout << "[MQTT] Publish to " << msg.first << ": " << msg.second.substr(0, 100)
                  << (msg.second.size() > 100 ? "..." : "") << std::endl;
    }
}

SensorData MQTTClient::parse_sensor_data(const std::string& topic, const std::string& payload) {
    SensorData data{};
    data.timestamp = std::chrono::system_clock::now();

    std::regex mill_id_regex("sensor/(\\d+)");
    std::smatch match;
    if (std::regex_search(topic, match, mill_id_regex)) {
        data.mill_id = static_cast<uint32_t>(std::stoi(match[1].str()));
    }

    std::stringstream ss(payload);
    std::string key, value;
    while (std::getline(ss, key, '=')) {
        if (!std::getline(ss, value, ',')) break;
        try {
            if (key == "roller_speed") data.roller_speed = std::stod(value);
            else if (key == "roller_pressure") data.roller_pressure = std::stod(value);
            else if (key == "grain_size_0_1mm") data.size_dist[0] = std::stod(value);
            else if (key == "grain_size_1_2mm") data.size_dist[1] = std::stod(value);
            else if (key == "grain_size_2_3mm") data.size_dist[2] = std::stod(value);
            else if (key == "grain_size_3_4mm") data.size_dist[3] = std::stod(value);
            else if (key == "grain_size_4_5mm") data.size_dist[4] = std::stod(value);
            else if (key == "grain_size_gt5mm") data.size_dist[5] = std::stod(value);
            else if (key == "yield") data.yield = std::stod(value);
            else if (key == "wear_degree") data.wear_degree = std::stod(value);
            else if (key == "roller_gap") data.roller_gap = std::stod(value);
            else if (key == "mill_id") data.mill_id = static_cast<uint32_t>(std::stoi(value));
        } catch (const std::exception& e) {
            std::cerr << "[MQTT] Parse error for key " << key << ": " << e.what() << std::endl;
        }
    }

    return data;
}

std::string MQTTClient::alert_to_json(const Alert& alert) {
    std::stringstream ss;
    auto tp = std::chrono::duration_cast<std::chrono::milliseconds>(
        alert.timestamp.time_since_epoch()).count();
    ss << "{"
       << "\"alert_id\":\"" << alert.alert_id << "\","
       << "\"mill_id\":" << alert.mill_id << ","
       << "\"timestamp\":" << tp << ","
       << "\"alert_type\":\"" << to_string(alert.type) << "\","
       << "\"severity\":\"" << to_string(alert.severity) << "\","
       << "\"message\":\"" << alert.message << "\","
       << "\"current_value\":" << alert.current_value << ","
       << "\"threshold\":" << alert.threshold << ","
       << "\"resolved\":" << (alert.resolved ? "true" : "false")
       << "}";
    return ss.str();
}

}

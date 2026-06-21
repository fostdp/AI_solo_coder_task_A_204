#pragma once

#include "common.h"
#include <memory>
#include <atomic>
#include <vector>
#include <chrono>
#include <optional>
#include <cstring>
#include <type_traits>
#include <mutex>
#include <queue>
#include <condition_variable>

#ifdef FALLBACK_MUTEX_QUEUE
namespace stone_mill {

template <typename T>
class MessageQueue {
public:
    explicit MessageQueue(size_t capacity = 65536)
        : capacity_(capacity), dropped_(0) {
        (void)capacity_;
    }

    bool push(const T& msg) {
        std::lock_guard<std::mutex> lk(mtx_);
        if (q_.size() >= capacity_) {
            dropped_.fetch_add(1, std::memory_order_relaxed);
            return false;
        }
        q_.push(msg);
        return true;
    }

    std::optional<T> pop() {
        std::lock_guard<std::mutex> lk(mtx_);
        if (q_.empty()) return std::nullopt;
        T v = q_.front();
        q_.pop();
        return v;
    }

    size_t pop_bulk(std::vector<T>& out, size_t max_count) {
        out.clear();
        out.reserve(max_count);
        std::lock_guard<std::mutex> lk(mtx_);
        size_t count = 0;
        while (count < max_count && !q_.empty()) {
            out.push_back(q_.front());
            q_.pop();
            ++count;
        }
        return count;
    }

    uint64_t dropped() const { return dropped_.load(std::memory_order_relaxed); }

private:
    size_t capacity_;
    std::atomic<uint64_t> dropped_;
    mutable std::mutex mtx_;
    std::queue<T> q_;
};

}
#else
#include <boost/lockfree/queue.hpp>

namespace stone_mill {

template <typename T>
class MessageQueue {
public:
    explicit MessageQueue(size_t capacity = 65536)
        : queue_(capacity), dropped_(0) {
        static_assert(std::is_trivially_copyable<T>::value,
            "MessageQueue<T> requires T to be trivially copyable");
        static_assert(std::is_trivially_default_constructible<T>::value,
            "MessageQueue<T> requires T to be trivially default constructible");
    }

    bool push(const T& msg) {
        if (queue_.push(msg)) return true;
        dropped_.fetch_add(1, std::memory_order_relaxed);
        return false;
    }

    std::optional<T> pop() {
        T msg{};
        if (queue_.pop(msg)) return msg;
        return std::nullopt;
    }

    size_t pop_bulk(std::vector<T>& out, size_t max_count) {
        out.clear();
        out.reserve(max_count);
        size_t count = 0;
        T msg{};
        while (count < max_count && queue_.pop(msg)) {
            out.push_back(msg);
            ++count;
        }
        return count;
    }

    uint64_t dropped() const { return dropped_.load(std::memory_order_relaxed); }

private:
    boost::lockfree::queue<T> queue_;
    std::atomic<uint64_t> dropped_;
};

}
#endif

constexpr size_t MSG_ERR_LEN = 128;
constexpr size_t MSG_ID_LEN = 48;
constexpr size_t MAX_DIST_BINS = 8;

struct PODDistribution {
    double bins[MAX_DIST_BINS];
    uint32_t count;

    void from_array(const std::array<double, GRAIN_SIZE_BINS>& src) {
        count = static_cast<uint32_t>(GRAIN_SIZE_BINS);
        for (size_t i = 0; i < GRAIN_SIZE_BINS && i < MAX_DIST_BINS; ++i) bins[i] = src[i];
    }
    void to_array(std::array<double, GRAIN_SIZE_BINS>& dst) const {
        for (size_t i = 0; i < GRAIN_SIZE_BINS; ++i) dst[i] = (i < count) ? bins[i] : 0.0;
    }
};

struct SensorMessage {
    uint32_t mill_id;
    uint32_t _pad0;
    uint64_t timestamp_ns;
    double speed;
    double pressure;
    double yield;
    double wear_degree;
    double roller_gap;
    double moisture;
    PODDistribution dist;
    int32_t valid;
    int32_t _pad1;
    char error[MSG_ERR_LEN];

    void set_error(const char* s) {
        std::strncpy(error, s, MSG_ERR_LEN - 1);
        error[MSG_ERR_LEN - 1] = 0;
        valid = 0;
    }
};

struct DEMRequest {
    enum TypeT : uint32_t {
        T_SIMULATE = 0,
        T_GENERATE = 1
    };

    uint32_t type;
    uint32_t mill_id;
    uint64_t request_id;
    uint32_t particle_count;
    uint32_t _pad;
    double roller_speed;
    double roller_gap;
    double sim_time;
    DEMConfig dem_cfg;
    BreakageModel brk_cfg;
};

struct DEMResponse {
    uint64_t request_id;
    uint32_t mill_id;
    int32_t success;
    PODDistribution final_dist;
    double breakage_rate;
    double avg_velocity;
    double avg_force;
    double max_force;
    double simulation_time;
    uint32_t particle_count;
    uint32_t _pad;
    char error[MSG_ERR_LEN];

    void set_error(const char* s) {
        std::strncpy(error, s, MSG_ERR_LEN - 1);
        error[MSG_ERR_LEN - 1] = 0;
        success = 0;
    }
};

struct OptimizeRequest {
    uint64_t request_id;
    uint32_t mill_id;
    uint32_t target_bin_min;
    uint32_t target_bin_max;
    uint32_t _pad;
    OptimizationParams params;
    BreakageModel brk_cfg;
};

struct OptimizeResponse {
    uint64_t request_id;
    uint32_t mill_id;
    int32_t success;
    double best_speed;
    double best_gap;
    double predicted_yield;
    double predicted_target_ratio;
    double fitness;
    uint32_t generations;
    uint32_t _pad;
    char error[MSG_ERR_LEN];

    void set_error(const char* s) {
        std::strncpy(error, s, MSG_ERR_LEN - 1);
        error[MSG_ERR_LEN - 1] = 0;
        success = 0;
    }
};

struct AlertMessage {
    uint32_t mill_id;
    uint32_t type;
    uint64_t timestamp_ns;
    uint32_t severity;
    int32_t resolved;
    double current_value;
    double threshold;
    char alert_id[MSG_ID_LEN];
    char message[MSG_ERR_LEN];
};

using SensorQueue = MessageQueue<SensorMessage>;
using DEMReqQueue = MessageQueue<DEMRequest>;
using DEMRespQueue = MessageQueue<DEMResponse>;
using OptReqQueue = MessageQueue<OptimizeRequest>;
using OptRespQueue = MessageQueue<OptimizeResponse>;
using AlertQueue = MessageQueue<AlertMessage>;

}

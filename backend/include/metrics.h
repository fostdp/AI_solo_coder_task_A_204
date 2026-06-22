#pragma once

#include <string>
#include <memory>
#include <atomic>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <chrono>

namespace stone_mill {

struct MetricSample {
    std::string name;
    std::string help;
    std::string type; // counter, gauge, histogram
    std::vector<std::pair<std::string, double>> values; // label -> value for gauge/counter
    std::vector<double> histogram_buckets;
    std::vector<std::pair<std::string, std::vector<uint64_t>>> histogram_values; // label -> bucket counts + sum
};

class Metrics {
public:
    static Metrics& instance();

    void start_http_server(int port = 9091);
    void stop_http_server();
    bool is_server_running() const { return http_running_.load(); }

    void counter_inc(const std::string& name, const std::string& labels, double amount = 1.0);
    void gauge_set(const std::string& name, const std::string& labels, double value);
    void histogram_observe(const std::string& name, const std::string& labels, double value);

    void declare_counter(const std::string& name, const std::string& help);
    void declare_gauge(const std::string& name, const std::string& help);
    void declare_histogram(const std::string& name, const std::string& help,
                           const std::vector<double>& buckets);

    std::string collect_prometheus_text();

    int http_port() const { return http_port_; }

private:
    Metrics();
    ~Metrics();

    void http_worker();

    struct MetricDef {
        std::string name;
        std::string help;
        std::string type;
        std::vector<double> buckets;
    };

    std::unordered_map<std::string, MetricDef> defs_;

    // counter/gauge: name#labels -> double
    std::unordered_map<std::string, std::atomic<double>> counters_;
    std::unordered_map<std::string, std::atomic<double>> gauges_;

    // histogram: name#labels -> (bucket_count array, total sum)
    struct HistogramState {
        std::vector<std::atomic<uint64_t>> buckets;
        std::atomic<double> sum{0.0};
        std::atomic<uint64_t> count{0};
    };
    std::unordered_map<std::string, std::unique_ptr<HistogramState>> histograms_;

    mutable std::mutex mu_;
    std::atomic<bool> http_running_{false};
    std::atomic<int> http_port_{0};
    std::thread http_thread_;
};

// Convenience wrapper macros
#define METRIC_COUNTER_INC(name, labels, amount)  ::stone_mill::Metrics::instance().counter_inc(name, labels, amount)
#define METRIC_GAUGE_SET(name, labels, value)     ::stone_mill::Metrics::instance().gauge_set(name, labels, value)
#define METRIC_HISTO(name, labels, value)         ::stone_mill::Metrics::instance().histogram_observe(name, labels, value)

}

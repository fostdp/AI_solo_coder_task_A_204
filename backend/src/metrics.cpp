#include "metrics.h"
#include "logger.h"
#include <sstream>
#include <iomanip>
#include <cstring>
#include <algorithm>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
using ssize_t = SSIZE_T;
using socklen_t = int;
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>
using SOCKET = int;
constexpr SOCKET INVALID_SOCKET = -1;
constexpr int SOCKET_ERROR = -1;
inline int closesocket(SOCKET s) { return ::close(s); }
#endif

namespace stone_mill {

Metrics& Metrics::instance() {
    static Metrics inst;
    return inst;
}

Metrics::Metrics() {
    declare_counter("stonemill_sensor_messages_total", "Total sensor messages received");
    declare_counter("stonemill_sensor_invalid_total", "Total invalid sensor messages");
    declare_counter("stonemill_alerts_total", "Total alerts generated");
    declare_counter("stonemill_dem_simulations_total", "Total DEM simulations run");
    declare_counter("stonemill_optimizations_total", "Total optimizations run");
    declare_counter("stonemill_queue_dropped_total", "Total messages dropped in queues");

    declare_gauge("stonemill_sensor_queue_size", "Current sensor queue size (approx)");
    declare_gauge("stonemill_active_particles", "Active DEM particles");
    declare_gauge("stonemill_alerts_active", "Currently active alerts");

    declare_histogram("stonemill_dem_simulation_seconds",
        "DEM simulation duration in seconds",
        {0.005, 0.01, 0.025, 0.05, 0.1, 0.25, 0.5, 1.0, 2.5, 5.0});
    declare_histogram("stonemill_optimization_seconds",
        "Optimization duration in seconds",
        {0.1, 0.5, 1.0, 2.5, 5.0, 10.0, 30.0, 60.0});
}

Metrics::~Metrics() { stop_http_server(); }

void Metrics::declare_counter(const std::string& name, const std::string& help) {
    std::lock_guard<std::mutex> lk(mu_);
    defs_[name] = MetricDef{name, help, "counter", {}};
}

void Metrics::declare_gauge(const std::string& name, const std::string& help) {
    std::lock_guard<std::mutex> lk(mu_);
    defs_[name] = MetricDef{name, help, "gauge", {}};
}

void Metrics::declare_histogram(const std::string& name, const std::string& help,
                                 const std::vector<double>& buckets) {
    std::lock_guard<std::mutex> lk(mu_);
    defs_[name] = MetricDef{name, help, "histogram", buckets};
}

static std::string make_key(const std::string& name, const std::string& labels) {
    return name + "#" + labels;
}

void Metrics::counter_inc(const std::string& name, const std::string& labels, double amount) {
    auto key = make_key(name, labels);
    auto it = counters_.find(key);
    if (it == counters_.end()) {
        counters_.emplace(key, std::atomic<double>{0.0});
        it = counters_.find(key);
    }
    it->second.fetch_add(amount, std::memory_order_relaxed);
}

void Metrics::gauge_set(const std::string& name, const std::string& labels, double value) {
    auto key = make_key(name, labels);
    auto it = gauges_.find(key);
    if (it == gauges_.end()) {
        gauges_.emplace(key, std::atomic<double>{0.0});
        it = gauges_.find(key);
    }
    it->second.store(value, std::memory_order_relaxed);
}

void Metrics::histogram_observe(const std::string& name, const std::string& labels, double value) {
    MetricDef def;
    {
        std::lock_guard<std::mutex> lk(mu_);
        auto it = defs_.find(name);
        if (it == defs_.end()) return;
        def = it->second;
    }

    auto key = make_key(name, labels);
    HistogramState* st = nullptr;
    {
        std::lock_guard<std::mutex> lk(mu_);
        auto it = histograms_.find(key);
        if (it == histograms_.end()) {
            auto ns = std::make_unique<HistogramState>();
            ns->buckets.resize(def.buckets.size() + 1);
            for (auto& b : ns->buckets) b.store(0);
            st = ns.get();
            histograms_.emplace(key, std::move(ns));
        } else {
            st = it->second.get();
        }
    }

    for (size_t i = 0; i < def.buckets.size(); ++i) {
        if (value <= def.buckets[i]) {
            st->buckets[i].fetch_add(1, std::memory_order_relaxed);
        }
    }
    st->buckets[def.buckets.size()].fetch_add(1, std::memory_order_relaxed); // +Inf bucket
    st->count.fetch_add(1, std::memory_order_relaxed);
    st->sum.fetch_add(value, std::memory_order_relaxed);
}

static std::string escape(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (char c : s) {
        if (c == '\\') out += "\\\\";
        else if (c == '"') out += "\\\"";
        else if (c == '\n') out += "\\n";
        else out += c;
    }
    return out;
}

std::string Metrics::collect_prometheus_text() {
    std::ostringstream os;
    std::lock_guard<std::mutex> lk(mu_);

    for (auto& entry : defs_) {
        const auto& def = entry.second;
        os << "# HELP " << def.name << " " << escape(def.help) << "\n";
        os << "# TYPE " << def.name << " " << def.type << "\n";

        if (def.type == "counter" || def.type == "gauge") {
            auto& map = (def.type == "counter") ? counters_ : gauges_;
            for (auto& ce : map) {
                auto pos = ce.first.find('#');
                if (pos == std::string::npos || ce.first.substr(0, pos) != def.name) continue;
                std::string labels = ce.first.substr(pos + 1);
                double v = ce.second.load(std::memory_order_relaxed);
                os << def.name;
                if (!labels.empty()) os << "{" << labels << "}";
                os << " " << std::fixed << std::setprecision(6) << v << "\n";
            }
        } else if (def.type == "histogram") {
            for (auto& he : histograms_) {
                auto pos = he.first.find('#');
                if (pos == std::string::npos || he.first.substr(0, pos) != def.name) continue;
                std::string labels = he.first.substr(pos + 1);
                auto* st = he.second.get();
                uint64_t acc = 0;
                for (size_t i = 0; i < def.buckets.size(); ++i) {
                    acc += st->buckets[i].load(std::memory_order_relaxed);
                    os << def.name << "_bucket{";
                    if (!labels.empty()) os << labels << ",";
                    os << "le=\"" << def.buckets[i] << "\"} " << acc << "\n";
                }
                acc += st->buckets[def.buckets.size()].load(std::memory_order_relaxed);
                os << def.name << "_bucket{";
                if (!labels.empty()) os << labels << ",";
                os << "le=\"+Inf\"} " << acc << "\n";
                os << def.name << "_sum";
                if (!labels.empty()) os << "{" << labels << "}";
                os << " " << std::fixed << std::setprecision(6)
                   << st->sum.load(std::memory_order_relaxed) << "\n";
                os << def.name << "_count";
                if (!labels.empty()) os << "{" << labels << "}";
                os << " " << st->count.load(std::memory_order_relaxed) << "\n";
            }
        }
    }

    return os.str();
}

void Metrics::start_http_server(int port) {
    if (http_running_.exchange(true)) return;
    http_port_.store(port);
    http_thread_ = std::thread(&Metrics::http_worker, this);
    LOG_INFO("Metrics HTTP server starting on port {}", port);
}

void Metrics::stop_http_server() {
    if (!http_running_.exchange(false)) return;
    if (http_thread_.joinable()) http_thread_.join();
}

void Metrics::http_worker() {
#ifdef _WIN32
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        LOG_ERROR("Metrics: WSAStartup failed");
        return;
    }
#endif

    SOCKET srv = ::socket(AF_INET, SOCK_STREAM, 0);
    if (srv == INVALID_SOCKET) {
        LOG_ERROR("Metrics: socket() failed");
        return;
    }

    int opt = 1;
#ifdef _WIN32
    ::setsockopt(srv, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));
#else
    ::setsockopt(srv, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
#endif

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(http_port_.load());

    if (::bind(srv, (sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
        LOG_ERROR("Metrics: bind() failed port={}", http_port_.load());
        closesocket(srv);
        return;
    }
    if (::listen(srv, 8) == SOCKET_ERROR) {
        LOG_ERROR("Metrics: listen() failed");
        closesocket(srv);
        return;
    }

    // Set non-blocking accept
#ifdef _WIN32
    u_long nonblock = 1;
    ioctlsocket(srv, FIONBIO, &nonblock);
#else
    int flags = fcntl(srv, F_GETFL, 0);
    fcntl(srv, F_SETFL, flags | O_NONBLOCK);
#endif

    LOG_INFO("Metrics HTTP server listening on :{}", http_port_.load());

    timeval tv{0, 50 * 1000}; // 50ms
    while (http_running_.load()) {
        fd_set rfds;
        FD_ZERO(&rfds);
        FD_SET(srv, &rfds);
        int n = ::select((int)srv + 1, &rfds, nullptr, nullptr, &tv);
        if (n <= 0) continue;

        sockaddr_in cli{};
        socklen_t cli_len = sizeof(cli);
        SOCKET c = ::accept(srv, (sockaddr*)&cli, &cli_len);
        if (c == INVALID_SOCKET) continue;

        char buf[2048] = {0};
        ::recv(c, buf, sizeof(buf) - 1, 0);

        std::string resp_body;
        std::string content_type = "text/plain; version=0.0.4; charset=utf-8";
        int status = 200;
        const char* status_text = "OK";

        if (strstr(buf, "GET /metrics") == buf ||
            strstr(buf, "GET / ") == buf) {
            resp_body = collect_prometheus_text();
        } else if (strstr(buf, "GET /health") == buf) {
            resp_body = "{\"status\":\"ok\"}\n";
            content_type = "application/json";
        } else {
            status = 404;
            status_text = "Not Found";
            resp_body = "404 Not Found\n";
        }

        std::ostringstream head;
        head << "HTTP/1.1 " << status << " " << status_text << "\r\n"
             << "Content-Type: " << content_type << "\r\n"
             << "Content-Length: " << resp_body.size() << "\r\n"
             << "Connection: close\r\n\r\n";
        std::string hd = head.str();
        ::send(c, hd.c_str(), (int)hd.size(), 0);
        ::send(c, resp_body.c_str(), (int)resp_body.size(), 0);
        closesocket(c);
    }

    closesocket(srv);
#ifdef _WIN32
    WSACleanup();
#endif
    LOG_INFO("Metrics HTTP server stopped");
}

}

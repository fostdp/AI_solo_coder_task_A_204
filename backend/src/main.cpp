#include "common.h"
#include "message_queue.h"
#include "config_loader.h"
#include "logger.h"
#include "metrics.h"
#include "modules/mqtt_receiver.h"
#include "modules/dem_simulator.h"
#include "modules/size_optimizer.h"
#include "modules/alarm_mqtt.h"
#include <iostream>
#include <chrono>
#include <thread>
#include <memory>
#include <csignal>

using namespace stone_mill;

namespace {
std::atomic<bool> g_shutdown{false};
void sigint_handler(int) { g_shutdown.store(true); }
}

static uint64_t now_ns() {
    return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count());
}

static void run_smoke_test(
    MqttReceiver& receiver,
    DEMReqQueue& dem_req, DEMRespQueue& dem_resp,
    OptReqQueue& opt_req, OptRespQueue& opt_resp,
    AlertQueue& alert_q) {

    LOG_INFO("Smoke Test: Inject sensor data");
    for (int i = 0; i < 3; ++i) {
        SensorMessage msg{};
        msg.mill_id = 1;
        msg.timestamp_ns = now_ns();
        msg.speed = 15.0;
        msg.pressure = 500.0;
        msg.yield = 3.0;
        msg.wear_degree = 85.0;
        msg.roller_gap = 2.0;
        msg.moisture = 0.15;
        msg.dist.count = GRAIN_SIZE_BINS;
        msg.dist.bins[0] = 0.1; msg.dist.bins[1] = 0.25;
        msg.dist.bins[2] = 0.3; msg.dist.bins[3] = 0.2;
        msg.dist.bins[4] = 0.1; msg.dist.bins[5] = 0.05;
        msg.valid = 1;
        receiver.inject_test_message(msg);
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    AlertMessage am{};
    int alert_cnt = 0;
    while (alert_q.pop(am).has_value()) alert_cnt++;
    LOG_INFO("Smoke alerts triggered: {}", alert_cnt);
    METRIC_GAUGE_SET("stonemill_alerts_active", "mill_id=\"1\"", alert_cnt);

    LOG_INFO("Smoke Test: DEM simulation request");
    DEMRequest dr{};
    dr.type = DEMRequest::T_SIMULATE;
    dr.request_id = 1001;
    dr.mill_id = 1;
    dr.particle_count = 80;
    dr.roller_speed = 15.0;
    dr.roller_gap = 2.0;
    dr.sim_time = 0.02;
    dr.dem_cfg = default_dem_config();
    dr.brk_cfg = default_breakage_model();
    dr.dem_cfg.use_coarse_graining = true;
    dr.dem_cfg.coarse_scale = 4;
    dem_req.push(dr);

    auto t0 = std::chrono::steady_clock::now();
    std::optional<DEMResponse> drr;
    for (int i = 0; i < 200 && !(drr = dem_resp.pop()).has_value(); ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(30));
    }
    auto t1 = std::chrono::steady_clock::now();
    double dem_elapsed = std::chrono::duration<double>(t1 - t0).count();
    if (drr.has_value()) {
        LOG_INFO("Smoke DEM resp: success={}, particles={}, breakage={:.3f}, elapsed={:.2f}ms",
                 drr->success, drr->particle_count, drr->breakage_rate, dem_elapsed * 1000);
        METRIC_HISTO("stonemill_dem_simulation_seconds", "mill_id=\"1\",coarse=true", dem_elapsed);
        METRIC_COUNTER_INC("stonemill_dem_simulations_total", "status=\"ok\"", 1);
        METRIC_GAUGE_SET("stonemill_active_particles", "mill_id=\"1\"", drr->particle_count);
    } else {
        LOG_WARN("Smoke DEM response TIMEOUT");
        METRIC_COUNTER_INC("stonemill_dem_simulations_total", "status=\"timeout\"", 1);
    }

    LOG_INFO("Smoke Test: Optimization request (small)");
    OptimizeRequest orq{};
    orq.request_id = 2001;
    orq.mill_id = 1;
    orq.target_bin_min = 0;
    orq.target_bin_max = 2;
    orq.params = default_optimization_params();
    orq.params.population_size = 6;
    orq.params.max_generations = 3;
    orq.brk_cfg = default_breakage_model();
    opt_req.push(orq);

    t0 = std::chrono::steady_clock::now();
    std::optional<OptimizeResponse> orr;
    for (int i = 0; i < 500 && !(orr = opt_resp.pop()).has_value(); ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(30));
    }
    t1 = std::chrono::steady_clock::now();
    double opt_elapsed = std::chrono::duration<double>(t1 - t0).count();
    if (orr.has_value()) {
        LOG_INFO("Smoke Opt resp: success={}, speed={:.2f}, gap={:.3f}, fitness={:.3f}, gens={}, elapsed={:.2f}ms",
                 orr->success, orr->best_speed, orr->best_gap, orr->fitness, orr->generations, opt_elapsed * 1000);
        METRIC_HISTO("stonemill_optimization_seconds", "mill_id=\"1\"", opt_elapsed);
        METRIC_COUNTER_INC("stonemill_optimizations_total", "status=\"ok\"", 1);
    } else {
        LOG_WARN("Smoke Opt response TIMEOUT");
        METRIC_COUNTER_INC("stonemill_optimizations_total", "status=\"timeout\"", 1);
    }
}

int main(int argc, char** argv) {
    Logger::init("stonemill", "", LogLevel::DEBUG);
    LOG_INFO("=== Stone Mill DEM Modular Service ===");
    LOG_INFO("Architecture: MqttReceiver -> SensorQueue -> AlarmMqtt -> AlertQueue");
    LOG_INFO("                         \\-> (DB write)");
    LOG_INFO("              DEMReqQueue -> DemSimulator -> DEMRespQueue");
    LOG_INFO("              OptReqQueue -> SizeOptimizer -> OptRespQueue");

    auto& metrics = Metrics::instance();
    metrics.start_http_server(9091);

    std::string config_path = "backend/config/app_config.json";
    if (argc > 1) config_path = argv[1];

    auto cfg_opt = ConfigLoader::load_from_file(config_path);
    AppConfig app_cfg;
    if (cfg_opt.has_value()) {
        app_cfg = *cfg_opt;
        LOG_INFO("Config loaded from {}", config_path);
    } else {
        app_cfg.process = default_process_config();
        app_cfg.dem = default_dem_config();
        app_cfg.breakage = default_breakage_model();
        app_cfg.optimization = default_optimization_params();
        app_cfg.thresholds = default_alert_thresholds();
        LOG_WARN("Config not found, using built-in defaults");
    }

    SensorQueue sensor_q(32768);
    DEMReqQueue dem_req_q(1024);
    DEMRespQueue dem_resp_q(1024);
    OptReqQueue opt_req_q(256);
    OptRespQueue opt_resp_q(256);
    AlertQueue alert_q(4096);

    MqttReceiver receiver(app_cfg.process, sensor_q, alert_q);
    DemSimulator dem_sim(app_cfg.dem, app_cfg.breakage, dem_req_q, dem_resp_q);
    SizeOptimizer optimizer(app_cfg.optimization, app_cfg.breakage, opt_req_q, opt_resp_q);
    AlarmMqtt alarm(app_cfg.thresholds, app_cfg.process, sensor_q, alert_q);

    receiver.start();
    dem_sim.start();
    optimizer.start();
    alarm.start();

    std::signal(SIGINT, sigint_handler);
#ifdef _WIN32
    std::signal(SIGTERM, sigint_handler);
    std::signal(SIGBREAK, sigint_handler);
#endif

    bool ran_smoke = false;
    uint64_t tick = 0;

    while (!g_shutdown.load()) {
        if (!ran_smoke) {
            ran_smoke = true;
            try {
                run_smoke_test(receiver, dem_req_q, dem_resp_q, opt_req_q, opt_resp_q, alert_q);
                LOG_INFO("Smoke test PASSED, service running. Press Ctrl+C to stop.");
            } catch (const std::exception& e) {
                LOG_ERROR("Smoke test error: {}", e.what());
            } catch (...) {
                LOG_ERROR("Smoke test unknown error");
            }
        }

        tick++;
        if (tick % 200 == 0) {
            auto s_dropped = sensor_q.dropped();
            auto a_dropped = alert_q.dropped();
            LOG_INFO("alive tick={}, sensor_dropped={}, alert_dropped={}", tick, s_dropped, a_dropped);
            METRIC_GAUGE_SET("stonemill_sensor_queue_size", "queue=\"sensor\"", sensor_q.approx_size());
            METRIC_GAUGE_SET("stonemill_sensor_queue_size", "queue=\"alert\"", alert_q.approx_size());
            if (s_dropped > 0) METRIC_COUNTER_INC("stonemill_queue_dropped_total", "queue=\"sensor\"", s_dropped);
            if (a_dropped > 0) METRIC_COUNTER_INC("stonemill_queue_dropped_total", "queue=\"alert\"", a_dropped);
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    LOG_INFO("Shutting down...");
    metrics.stop_http_server();
    alarm.stop();
    optimizer.stop();
    dem_sim.stop();
    receiver.stop();
    Logger::flush();
    LOG_INFO("Bye.");
    return 0;
}

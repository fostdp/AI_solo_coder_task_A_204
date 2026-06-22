#include "modules/dem_simulator.h"
#include "common.h"
#include "logger.h"
#include "metrics.h"
#include <chrono>
#include <iostream>
#include <cstring>

namespace stone_mill {

DemSimulator::DemSimulator(const DEMConfig& dem_cfg,
                           const BreakageModel& brk_cfg,
                           DEMReqQueue& req_in,
                           DEMRespQueue& resp_out)
    : dem_cfg_(dem_cfg), brk_cfg_(brk_cfg),
      req_in_(req_in), resp_out_(resp_out) {}

DemSimulator::~DemSimulator() { stop(); }

void DemSimulator::start() {
    running_.store(true);
    thread_ = std::thread(&DemSimulator::worker, this);
    LOG_INFO("DemSimulator started (coarse_graining={}, scale={})",
             dem_cfg_.use_coarse_graining, dem_cfg_.coarse_scale);
}

void DemSimulator::stop() {
    if (running_.exchange(false)) {
        if (thread_.joinable()) thread_.join();
        LOG_INFO("DemSimulator stopped");
    }
}

DEMResponse DemSimulator::handle_simulate(const DEMRequest& req) {
    DEMResponse resp{};
    resp.request_id = req.request_id;
    resp.mill_id = req.mill_id;
    resp.success = 0;
    auto t0 = std::chrono::steady_clock::now();
    try {
        DEMConfig cfg = dem_cfg_;
        if (req.dem_cfg.dt > 0) cfg = req.dem_cfg;
        BreakageModel brk = brk_cfg_;
        if (req.brk_cfg.selection_function_param > 0) brk = req.brk_cfg;

        DEMModel model(cfg, brk);
        model.generate_particles(req.particle_count > 0 ? req.particle_count : 100, req.roller_gap);
        DEMResult r = model.simulate(req.sim_time, req.roller_speed, req.roller_gap);

        resp.success = 1;
        resp.breakage_rate = r.breakage_rate;
        resp.avg_velocity = r.avg_velocity;
        resp.avg_force = r.avg_force;
        resp.max_force = r.max_force;
        resp.simulation_time = r.simulation_time;
        resp.particle_count = static_cast<uint32_t>(r.particles.size());
        resp.final_dist.from_array(r.final_distribution.bins);

        auto t1 = std::chrono::steady_clock::now();
        double elapsed = std::chrono::duration<double>(t1 - t0).count();
        LOG_DEBUG("DemSimulator simulate req_id={} particles={} breakage={:.3f} elapsed={:.3f}ms",
                  req.request_id, resp.particle_count, resp.breakage_rate, elapsed * 1000);
        std::string labels = "mill_id=\"" + std::to_string(req.mill_id)
            + "\",coarse=\"" + (cfg.use_coarse_graining ? "true" : "false") + "\"";
        METRIC_HISTO("stonemill_dem_simulation_seconds", labels, elapsed);
        METRIC_COUNTER_INC("stonemill_dem_simulations_total", "status=\"ok\"", 1);
        METRIC_GAUGE_SET("stonemill_active_particles",
            ("mill_id=\"" + std::to_string(req.mill_id) + "\"").c_str(), resp.particle_count);
    } catch (const std::exception& e) {
        resp.set_error(e.what());
        LOG_ERROR("DemSimulator simulate req_id={} error: {}", req.request_id, e.what());
        METRIC_COUNTER_INC("stonemill_dem_simulations_total", "status=\"error\"", 1);
    } catch (...) {
        resp.set_error("unknown exception");
        LOG_ERROR("DemSimulator simulate req_id={} unknown error", req.request_id);
        METRIC_COUNTER_INC("stonemill_dem_simulations_total", "status=\"error\"", 1);
    }
    return resp;
}

DEMResponse DemSimulator::handle_generate(const DEMRequest& req) {
    DEMResponse resp{};
    resp.request_id = req.request_id;
    resp.mill_id = req.mill_id;
    resp.success = 0;
    try {
        DEMConfig cfg = dem_cfg_;
        if (req.dem_cfg.dt > 0) cfg = req.dem_cfg;
        BreakageModel brk = brk_cfg_;
        if (req.brk_cfg.selection_function_param > 0) brk = req.brk_cfg;

        DEMModel model(cfg, brk);
        auto parts = model.generate_particles(req.particle_count > 0 ? req.particle_count : 100, req.roller_gap);
        auto dist = model.compute_size_distribution(parts);

        resp.success = 1;
        resp.breakage_rate = 0;
        resp.avg_velocity = 0;
        resp.avg_force = 0;
        resp.max_force = 0;
        resp.simulation_time = 0;
        resp.particle_count = static_cast<uint32_t>(parts.size());
        resp.final_dist.from_array(dist.bins);
        LOG_DEBUG("DemSimulator generate req_id={} particles={}", req.request_id, resp.particle_count);
    } catch (const std::exception& e) {
        resp.set_error(e.what());
        LOG_ERROR("DemSimulator generate req_id={} error: {}", req.request_id, e.what());
    } catch (...) {
        resp.set_error("unknown exception");
        LOG_ERROR("DemSimulator generate req_id={} unknown error", req.request_id);
    }
    return resp;
}

void DemSimulator::worker() {
    std::vector<DEMRequest> batch;
    batch.reserve(16);
    while (running_.load()) {
        size_t n = req_in_.pop_bulk(batch, 16);
        if (n == 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }
        for (auto& req : batch) {
            DEMResponse resp;
            if (req.type == DEMRequest::T_SIMULATE) {
                resp = handle_simulate(req);
            } else {
                resp = handle_generate(req);
            }
            if (!resp_out_.push(resp)) {
                LOG_ERROR("DemSimulator resp queue full, req_id={} dropped", req.request_id);
                METRIC_COUNTER_INC("stonemill_queue_dropped_total", "queue=\"dem_resp\"", 1);
            }
        }
    }
}

}

#include "modules/dem_simulator.h"
#include "common.h"
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
    std::cout << "[DemSimulator] started" << std::endl;
}

void DemSimulator::stop() {
    if (running_.exchange(false)) {
        if (thread_.joinable()) thread_.join();
        std::cout << "[DemSimulator] stopped" << std::endl;
    }
}

DEMResponse DemSimulator::handle_simulate(const DEMRequest& req) {
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
    } catch (const std::exception& e) {
        resp.set_error(e.what());
    } catch (...) {
        resp.set_error("unknown exception");
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
    } catch (const std::exception& e) {
        resp.set_error(e.what());
    } catch (...) {
        resp.set_error("unknown exception");
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
                std::cerr << "[DemSimulator] resp queue full, req_id="
                          << req.request_id << " dropped" << std::endl;
            }
        }
    }
}

}

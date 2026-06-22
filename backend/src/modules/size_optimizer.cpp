#include "modules/size_optimizer.h"
#include "logger.h"
#include "metrics.h"
#include <chrono>
#include <iostream>
#include <cstring>
#include <sstream>

namespace stone_mill {

SizeOptimizer::SizeOptimizer(const OptimizationParams& opt_cfg,
                             const BreakageModel& brk_cfg,
                             OptReqQueue& req_in,
                             OptRespQueue& resp_out)
    : opt_cfg_(opt_cfg), brk_cfg_(brk_cfg),
      req_in_(req_in), resp_out_(resp_out) {}

SizeOptimizer::~SizeOptimizer() { stop(); }

void SizeOptimizer::start() {
    running_.store(true);
    thread_ = std::thread(&SizeOptimizer::worker, this);
    LOG_INFO("SizeOptimizer started (pop={}, gens={})",
             opt_cfg_.population_size, opt_cfg_.max_generations);
}

void SizeOptimizer::stop() {
    if (running_.exchange(false)) {
        if (thread_.joinable()) thread_.join();
        LOG_INFO("SizeOptimizer stopped");
    }
}

OptimizationResult SizeOptimizer::run_optimization(const OptimizeRequest& req) {
    OptimizationParams p = opt_cfg_;
    if (req.params.population_size > 0) p = req.params;
    BreakageModel brk = brk_cfg_;
    if (req.brk_cfg.selection_function_param > 0) brk = req.brk_cfg;

    GeneticOptimizer ga(p, brk);
    return ga.optimize(req.target_bin_min, req.target_bin_max);
}

void SizeOptimizer::worker() {
    while (running_.load()) {
        auto req = req_in_.pop();
        if (!req.has_value()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            continue;
        }
        OptimizeResponse resp{};
        resp.request_id = req->request_id;
        resp.mill_id = req->mill_id;
        resp.success = 0;
        auto t0 = std::chrono::steady_clock::now();
        try {
            OptimizationResult r = run_optimization(*req);
            resp.success = 1;
            resp.best_speed = r.best_speed;
            resp.best_gap = r.best_gap;
            resp.predicted_yield = r.predicted_yield;
            resp.predicted_target_ratio = r.predicted_target_ratio;
            resp.fitness = r.fitness;
            resp.generations = r.generations;

            auto t1 = std::chrono::steady_clock::now();
            double elapsed = std::chrono::duration<double>(t1 - t0).count();
            LOG_INFO("SizeOptimizer req_id={} success speed={:.2f} gap={:.3f} fit={:.3f} gens={} {:.1f}ms",
                     req->request_id, resp.best_speed, resp.best_gap, resp.fitness,
                     resp.generations, elapsed * 1000);
            METRIC_HISTO("stonemill_optimization_seconds",
                ("mill_id=\"" + std::to_string(req->mill_id) + "\"").c_str(), elapsed);
            METRIC_COUNTER_INC("stonemill_optimizations_total", "status=\"ok\"", 1);
        } catch (const std::exception& e) {
            resp.set_error(e.what());
            LOG_ERROR("SizeOptimizer req_id={} error: {}", req->request_id, e.what());
            METRIC_COUNTER_INC("stonemill_optimizations_total", "status=\"error\"", 1);
        } catch (...) {
            resp.set_error("unknown exception");
            LOG_ERROR("SizeOptimizer req_id={} unknown error", req->request_id);
            METRIC_COUNTER_INC("stonemill_optimizations_total", "status=\"error\"", 1);
        }
        if (!resp_out_.push(resp)) {
            LOG_ERROR("SizeOptimizer resp queue full, dropped req_id={}", req->request_id);
            METRIC_COUNTER_INC("stonemill_queue_dropped_total", "queue=\"opt_resp\"", 1);
        }
    }
}

}

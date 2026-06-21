#include "modules/size_optimizer.h"
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
    std::cout << "[SizeOptimizer] started" << std::endl;
}

void SizeOptimizer::stop() {
    if (running_.exchange(false)) {
        if (thread_.joinable()) thread_.join();
        std::cout << "[SizeOptimizer] stopped" << std::endl;
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
        try {
            OptimizationResult r = run_optimization(*req);
            resp.success = 1;
            resp.best_speed = r.best_speed;
            resp.best_gap = r.best_gap;
            resp.predicted_yield = r.predicted_yield;
            resp.predicted_target_ratio = r.predicted_target_ratio;
            resp.fitness = r.fitness;
            resp.generations = r.generations;
        } catch (const std::exception& e) {
            resp.set_error(e.what());
        } catch (...) {
            resp.set_error("unknown exception");
        }
        if (!resp_out_.push(resp)) {
            std::cerr << "[SizeOptimizer] resp queue full, dropped" << std::endl;
        }
    }
}

}

#pragma once

#include "common.h"
#include "genetic_optimizer.h"
#include "message_queue.h"
#include <atomic>
#include <thread>
#include <memory>

namespace stone_mill {

class SizeOptimizer {
public:
    SizeOptimizer(const OptimizationParams& opt_cfg,
                  const BreakageModel& brk_cfg,
                  OptReqQueue& req_in,
                  OptRespQueue& resp_out);
    ~SizeOptimizer();

    void start();
    void stop();
    bool is_running() const { return running_.load(); }

private:
    void worker();
    OptimizationResult run_optimization(const OptimizeRequest& req);

    OptimizationParams opt_cfg_;
    BreakageModel brk_cfg_;
    OptReqQueue& req_in_;
    OptRespQueue& resp_out_;
    std::atomic<bool> running_{false};
    std::thread thread_;
};

}

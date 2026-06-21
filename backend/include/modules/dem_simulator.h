#pragma once

#include "common.h"
#include "dem_model.h"
#include "message_queue.h"
#include <atomic>
#include <thread>
#include <memory>

namespace stone_mill {

class DemSimulator {
public:
    DemSimulator(const DEMConfig& dem_cfg,
                 const BreakageModel& brk_cfg,
                 DEMReqQueue& req_in,
                 DEMRespQueue& resp_out);
    ~DemSimulator();

    void start();
    void stop();
    bool is_running() const { return running_.load(); }

private:
    void worker();
    DEMResponse handle_simulate(const DEMRequest& req);
    DEMResponse handle_generate(const DEMRequest& req);

    DEMConfig dem_cfg_;
    BreakageModel brk_cfg_;
    DEMReqQueue& req_in_;
    DEMRespQueue& resp_out_;
    std::atomic<bool> running_{false};
    std::thread thread_;
};

}

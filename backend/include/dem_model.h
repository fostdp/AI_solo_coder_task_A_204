#pragma once

#include "common.h"
#include <vector>
#include <random>
#include <functional>

namespace stone_mill {

class DEMModel {
public:
    DEMModel();
    ~DEMModel() = default;

    void set_config(const DEMConfig& config);
    void set_breakage_model(const BreakageModel& model);

    std::vector<DEMParticle> generate_particles(size_t count, double min_radius, double max_radius,
                                         double mill_center_x, double mill_center_y);

    DEMResult simulate(const std::vector<DEMParticle>& initial_particles,
                   double roller_speed, double roller_gap,
                   double total_simulation_time);

    GrainSizeDistribution compute_size_distribution(const std::vector<DEMParticle>& particles);

    void set_force_callback(std::function<void(const DEMParticle&, double)> callback);

private:
    struct Contact {
        size_t i, j;
        double nx, ny, nz;
        double overlap;
    };

    void apply_gravity(std::vector<DEMParticle>& particles);
    void compute_forces(std::vector<DEMParticle>& particles,
                     const std::vector<Contact>& contacts);
    void compute_wall_forces(std::vector<DEMParticle>& particles,
                         double roller_gap);
    void integrate(std::vector<DEMParticle>& particles);
    void handle_breakage(std::vector<DEMParticle>& particles,
                       std::mt19937& rng);

    std::vector<DEMParticle> break_particle(const DEMParticle& particle,
                                         size_t new_id_start,
                                         std::mt19937& rng);

    bool should_break(const DEMParticle& particle);
    double compute_contact_force(const DEMParticle& p1, const DEMParticle& p2,
                              double& nx, double& ny, double& nz,
                              double& overlap);
    void compute_roller_forces(std::vector<DEMParticle>& particles,
                           double roller_speed, double roller_gap);

    DEMConfig config_;
    BreakageModel breakage_model_;
    std::function<void(const DEMParticle&, double)> force_callback_;

    std::mt19937 rng_{std::random_device{}()};

    double roller_angle_ = 0.0;
    uint32_t next_particle_id_ = 0;
};

}

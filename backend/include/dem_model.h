#pragma once

#include "common.h"
#include <vector>
#include <random>
#include <functional>
#include <unordered_map>
#include <array>

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

    std::vector<DEMParticle> coarse_grain(const std::vector<DEMParticle>& fine_particles);
    std::vector<DEMParticle> reconstruct_fine(const std::vector<DEMParticle>& coarse_particles);

private:
    struct Contact {
        size_t i, j;
        double nx, ny, nz;
        double overlap;
    };

    struct GridCell {
        std::vector<size_t> particle_indices;
    };

    using SpatialGrid = std::unordered_map<int64_t, GridCell>;

    int64_t grid_key(int gx, int gy, int gz) const;
    void build_spatial_grid(const std::vector<DEMParticle>& particles, SpatialGrid& grid) const;
    void find_contacts_grid(const std::vector<DEMParticle>& particles,
                          const SpatialGrid& grid,
                          std::vector<Contact>& contacts) const;
    void find_contacts_brute(const std::vector<DEMParticle>& particles,
                           std::vector<Contact>& contacts) const;

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
    double compute_moisture_strength_factor(double moisture) const;
    double compute_cohesion_force(const DEMParticle& p1, const DEMParticle& p2,
                                  double nx, double ny, double nz) const;
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

    size_t grid_size_x_ = 0;
    size_t grid_size_y_ = 0;
    size_t grid_size_z_ = 0;
};

}

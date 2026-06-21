#include "dem_model.h"
#include <iostream>
#include <cmath>
#include <algorithm>

namespace stone_mill {

DEMModel::DEMModel() {
    next_particle_id_ = 0;
}

void DEMModel::set_config(const DEMConfig& config) {
    config_ = config;
}

void DEMModel::set_breakage_model(const BreakageModel& model) {
    breakage_model_ = model;
}

std::vector<DEMParticle> DEMModel::generate_particles(size_t count, double min_radius, double max_radius,
                                                      double mill_center_x, double mill_center_y) {
    std::vector<DEMParticle> particles;
    particles.reserve(count);

    std::uniform_real_distribution<> radius_dist(min_radius, max_radius);
    std::uniform_real_distribution<> angle_dist(0, 2 * M_PI);
    std::uniform_real_distribution<> rad_dist(0.1, config_.mill_radius * 0.7);

    for (size_t i = 0; i < count; ++i) {
        double angle = angle_dist(rng_);
        double rad = rad_dist(rng_);
        double r = radius_dist(rng_);

        DEMParticle p{};
        p.id = next_particle_id_++;
        p.x = mill_center_x + rad * std::cos(angle);
        p.y = mill_center_y + rad * std::sin(angle);
        p.z = r + 0.01;
        p.vx = p.vy = p.vz = 0;
        p.fx = p.fy = p.fz = 0;
        p.radius = r;
        p.mass = (4.0 / 3.0) * M_PI * std::pow(r, 3) * 1200;
        p.youngs_modulus = 1e9;
        p.poisson_ratio = 0.3;
        p.restitution = 0.3;
        p.strength = 5e6 + std::normal_distribution<double>(0, 1e6)(rng_);
        p.broken = false;
        particles.push_back(p);
    }
    return particles;
}

DEMResult DEMModel::simulate(const std::vector<DEMParticle>& initial_particles,
                             double roller_speed, double roller_gap,
                             double total_simulation_time) {
    DEMResult result;
    result.particles = initial_particles;
    roller_angle_ = 0.0;

    size_t num_steps = static_cast<size_t>(total_simulation_time / config_.dt);
    double sim_time = 0;
    double max_force = 0;
    double total_force = 0;
    double total_velocity = 0;
    size_t force_samples = 0;
    size_t initial_count = initial_particles.size();
    size_t initial_broken_count = 0;
    for (const auto& p : initial_particles) if (p.broken) initial_broken_count++;

    std::cout << "[DEM] Starting simulation: " << num_steps << " steps, dt=" << config_.dt
              << ", particles=" << initial_particles.size() << std::endl;

    for (size_t step = 0; step < num_steps; ++step) {
        sim_time += config_.dt;
        roller_angle_ += roller_speed * config_.dt;

        apply_gravity(result.particles);
        compute_roller_forces(result.particles, roller_speed, roller_gap);
        compute_wall_forces(result.particles, roller_gap);

        std::vector<Contact> contacts;
        for (size_t i = 0; i < result.particles.size(); ++i) {
            if (result.particles[i].broken) continue;
            for (size_t j = i + 1; j < result.particles.size(); ++j) {
                if (result.particles[j].broken) continue;
                const auto& p1 = result.particles[i];
                const auto& p2 = result.particles[j];
                double dx = p2.x - p1.x;
                double dy = p2.y - p1.y;
                double dz = p2.z - p1.z;
                double dist = std::sqrt(dx*dx + dy*dy + dz*dz);
                double min_dist = p1.radius + p2.radius;

                if (dist < min_dist && dist > 1e-8) {
                    Contact c{i, j, dx/dist, dy/dist, dz/dist, min_dist - dist};
                    contacts.push_back(c);
                    double force = compute_contact_force(p1, p2, c.nx, c.ny, c.nz, c.overlap);
                    max_force = std::max(max_force, force);
                    total_force += force;
                    force_samples++;
                }
            }
        }

        compute_forces(result.particles, contacts);
        integrate(result.particles);
        handle_breakage(result.particles, rng_);

        for (const auto& p : result.particles) {
            if (!p.broken) {
                total_velocity += std::sqrt(p.vx*p.vx + p.vy*p.vy + p.vz*p.vz);
            }
        }

        if (step % 1000 == 0) {
            size_t broken_count = 0;
            for (const auto& p : result.particles) if (p.broken) broken_count++;
            std::cout << "[DEM] Step " << step << "/" << num_steps
                      << ", particles=" << result.particles.size()
                      << ", broken=" << (broken_count - initial_broken_count) << std::endl;
        }
    }

    size_t final_broken_count = 0;
    for (const auto& p : result.particles) if (p.broken) final_broken_count++;

    result.final_distribution = compute_size_distribution(result.particles);
    result.breakage_rate = static_cast<double>(final_broken_count - initial_broken_count) /
                          static_cast<double>(initial_count > 0 ? initial_count : 1);
    result.avg_velocity = total_velocity / static_cast<double>(num_steps * std::max(initial_count, static_cast<size_t>(1)));
    result.avg_force = force_samples > 0 ? total_force / force_samples : 0;
    result.max_force = max_force;
    result.simulation_time = sim_time;

    std::cout << "[DEM] Simulation complete: breakage_rate=" << result.breakage_rate
              << ", avg_force=" << result.avg_force
              << ", max_force=" << result.max_force << std::endl;

    return result;
}

GrainSizeDistribution DEMModel::compute_size_distribution(const std::vector<DEMParticle>& particles) {
    GrainSizeDistribution dist;
    double total_mass = 0;

    std::array<double, GRAIN_SIZE_BINS> bin_masses{0};
    std::array<double, GRAIN_SIZE_BINS> bin_edges{0.001, 0.002, 0.003, 0.004, 0.005, 1.0};

    for (const auto& p : particles) {
        if (p.broken) continue;
        double mass = p.mass;
        total_mass += mass;
        double diameter = 2 * p.radius;

        for (size_t i = 0; i < GRAIN_SIZE_BINS; ++i) {
            if (diameter <= bin_edges[i]) {
                bin_masses[i] += mass;
                break;
            }
        }
    }

    if (total_mass > 0) {
        for (size_t i = 0; i < GRAIN_SIZE_BINS; ++i) {
            dist[i] = bin_masses[i] / total_mass;
        }
    }

    return dist;
}

void DEMModel::set_force_callback(std::function<void(const DEMParticle&, double)> callback) {
    force_callback_ = std::move(callback);
}

void DEMModel::apply_gravity(std::vector<DEMParticle>& particles) {
    for (auto& p : particles) {
        if (p.broken) continue;
        p.fz -= p.mass * config_.gravity;
    }
}

double DEMModel::compute_contact_force(const DEMParticle& p1, const DEMParticle& p2,
                                       double& nx, double& ny, double& nz,
                                       double& overlap) {
    double E_star = 1.0 / ((1 - p1.poisson_ratio*p1.poisson_ratio) / p1.youngs_modulus +
                           (1 - p2.poisson_ratio*p2.poisson_ratio) / p2.youngs_modulus);
    double R_star = 1.0 / (1.0 / p1.radius + 1.0 / p2.radius);
    double a = std::sqrt(R_star * overlap);
    double kn = (4.0 / 3.0) * E_star * std::sqrt(R_star);
    double normal_force = kn * std::pow(overlap, 1.5);
    double damping = 0.1;
    double rel_vel = (p2.vx - p1.vx) * nx + (p2.vy - p1.vy) * ny + (p2.vz - p1.vz) * nz;
    double damping_force = -damping * rel_vel * std::sqrt(p1.mass * p2.mass / (p1.mass + p2.mass));
    return std::max(0.0, normal_force + damping_force);
}

void DEMModel::compute_forces(std::vector<DEMParticle>& particles,
                              const std::vector<Contact>& contacts) {
    for (const auto& c : contacts) {
        auto& p1 = particles[c.i];
        auto& p2 = particles[c.j];
        double force = compute_contact_force(p1, p2, c.nx, c.ny, c.nz, c.overlap);
        p1.fx += force * c.nx;
        p1.fy += force * c.ny;
        p1.fz += force * c.nz;
        p2.fx -= force * c.nx;
        p2.fy -= force * c.ny;
        p2.fz -= force * c.nz;

        double tx = -c.nz, ty = 0, tz = c.nx;
        double shear_force = force * 0.3;
        p1.fx += shear_force * tx;
        p1.fz += shear_force * tz;
        p2.fx -= shear_force * tx;
        p2.fz -= shear_force * tz;
    }
}

void DEMModel::compute_wall_forces(std::vector<DEMParticle>& particles, double roller_gap) {
    double ground_y = 0;
    double mill_center_x = 0;
    double mill_center_y = 0;

    for (auto& p : particles) {
        if (p.broken) continue;

        if (p.z - p.radius < ground_y) {
            double overlap = p.radius - p.z + ground_y;
            double E = p.youngs_modulus;
            double R = p.radius;
            double kn = (4.0 / 3.0) * E * std::sqrt(R);
            double normal_force = kn * std::pow(overlap, 1.5);
            p.fz += normal_force;
            double friction = config_.dynamic_friction * normal_force;
            p.fx -= std::copysign(friction, p.vx);
            p.fy -= std::copysign(friction, p.vy);
        }

        double dx = p.x - mill_center_x;
        double dy = p.y - mill_center_y;
        double dist = std::sqrt(dx*dx + dy*dy);
        double wall_radius = config_.mill_radius;

        if (dist + p.radius > wall_radius) {
            double overlap = dist + p.radius - wall_radius;
            double nx = dx / dist, ny = dy / dist;
            double kn = (4.0 / 3.0) * p.youngs_modulus * std::sqrt(p.radius);
            double normal_force = kn * std::pow(overlap, 1.5);
            p.fx -= normal_force * nx;
            p.fy -= normal_force * ny;
        }
    }
}

void DEMModel::compute_roller_forces(std::vector<DEMParticle>& particles,
                                     double roller_speed, double roller_gap) {
    double roller_center_x = config_.mill_radius * 0.5 * std::cos(roller_angle_);
    double roller_center_y = config_.mill_radius * 0.5 * std::sin(roller_angle_);
    double roller_center_z = roller_gap + config_.roller_radius;

    for (auto& p : particles) {
        if (p.broken) continue;

        double dx = p.x - roller_center_x;
        double dy = p.y - roller_center_y;
        double dz = p.z - roller_center_z;

        if (std::abs(dx) < config_.roller_width && dz < 0) {
            double dist_y = std::abs(dy);
            if (dist_y < config_.roller_radius + p.radius) {
                double overlap = config_.roller_radius + p.radius - dist_y;
                double ny = dy > 0 ? -1.0 : 1.0;

                double E_star = p.youngs_modulus / (1 - p.poisson_ratio * p.poisson_ratio);
                double R_star = 1.0 / (1.0 / p.radius + 1.0 / config_.roller_radius);
                double kn = (4.0 / 3.0) * E_star * std::sqrt(R_star);
                double normal_force = kn * std::pow(overlap, 1.5);

                p.fy += normal_force * ny;

                double roller_tangential_speed = roller_speed * config_.roller_radius;
                double tangential_dir = -std::sin(roller_angle_);
                double shear_force = config_.static_friction * normal_force;
                p.fx += shear_force * tangential_dir * std::cos(roller_angle_);

                if (force_callback_) {
                    force_callback_(p, normal_force);
                }
            }
        }
    }
}

void DEMModel::integrate(std::vector<DEMParticle>& particles) {
    for (auto& p : particles) {
        if (p.broken) continue;

        double ax = p.fx / p.mass;
        double ay = p.fy / p.mass;
        double az = p.fz / p.mass;

        p.vx += ax * config_.dt;
        p.vy += ay * config_.dt;
        p.vz += az * config_.dt;

        p.vx *= config_.damping;
        p.vy *= config_.damping;
        p.vz *= config_.damping;

        p.x += p.vx * config_.dt;
        p.y += p.vy * config_.dt;
        p.z += p.vz * config_.dt;

        p.fx = p.fy = p.fz = 0;
    }
}

bool DEMModel::should_break(const DEMParticle& particle) {
    double contact_stress = std::sqrt(particle.fx*particle.fx +
                                      particle.fy*particle.fy +
                                      particle.fz*particle.fz) /
                           (M_PI * particle.radius * particle.radius);

    double selection_prob = breakage_model_.selection_function_param *
                            std::pow(contact_stress / particle.strength, 2.0);

    if (contact_stress > particle.strength) {
        return true;
    }

    return std::uniform_real_distribution<double>(0, 1)(rng_) < selection_prob;
}

std::vector<DEMParticle> DEMModel::break_particle(const DEMParticle& particle,
                                                  size_t new_id_start,
                                                  std::mt19937& rng) {
    std::vector<DEMParticle> fragments;
    size_t num_fragments = 3 + std::uniform_int_distribution<size_t>(0, 3)(rng);

    double parent_volume = (4.0 / 3.0) * M_PI * std::pow(particle.radius, 3);
    double fragment_volume = parent_volume / num_fragments;
    double fragment_radius = std::pow(3 * fragment_volume / (4 * M_PI), 1.0 / 3.0);

    std::uniform_real_distribution<> angle_dist(0, 2 * M_PI);
    std::normal_distribution<> vel_dist(0, 1.0);

    for (size_t i = 0; i < num_fragments; ++i) {
        double theta = angle_dist(rng);
        double phi = std::acos(2 * std::uniform_real_distribution<double>(0, 1)(rng) - 1);

        DEMParticle frag{};
        frag.id = static_cast<uint32_t>(new_id_start + i);
        frag.x = particle.x + particle.radius * 0.3 * std::sin(phi) * std::cos(theta);
        frag.y = particle.y + particle.radius * 0.3 * std::sin(phi) * std::sin(theta);
        frag.z = particle.z + particle.radius * 0.3 * std::cos(phi);
        frag.vx = particle.vx + vel_dist(rng);
        frag.vy = particle.vy + vel_dist(rng);
        frag.vz = particle.vz + vel_dist(rng);
        frag.fx = frag.fy = frag.fz = 0;
        frag.radius = fragment_radius * (0.7 + std::uniform_real_distribution<double>(0, 0.6)(rng));
        frag.mass = (4.0 / 3.0) * M_PI * std::pow(frag.radius, 3) * 1200;
        frag.youngs_modulus = particle.youngs_modulus;
        frag.poisson_ratio = particle.poisson_ratio;
        frag.restitution = particle.restitution;
        frag.strength = particle.strength * (0.8 + std::uniform_real_distribution<double>(0, 0.4)(rng));
        frag.broken = false;
        fragments.push_back(frag);
    }

    return fragments;
}

void DEMModel::handle_breakage(std::vector<DEMParticle>& particles, std::mt19937& rng) {
    std::vector<size_t> to_break;
    for (size_t i = 0; i < particles.size(); ++i) {
        if (!particles[i].broken && should_break(particles[i])) {
            to_break.push_back(i);
        }
    }

    if (to_break.empty()) return;

    size_t new_id_start = next_particle_id_;
    std::vector<DEMParticle> new_particles;

    for (size_t idx : to_break) {
        particles[idx].broken = true;
        auto fragments = break_particle(particles[idx], new_id_start, rng);
        new_particles.insert(new_particles.end(), fragments.begin(), fragments.end());
        new_id_start += fragments.size();
    }

    next_particle_id_ = new_id_start;
    particles.insert(particles.end(), new_particles.begin(), new_particles.end());
}

}

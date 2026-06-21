#include "dem_model.h"
#include <iostream>
#include <cmath>
#include <algorithm>
#include <numeric>

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

int64_t DEMModel::grid_key(int gx, int gy, int gz) const {
    const int64_t OFFSET = 100000;
    return (static_cast<int64_t>(gx + OFFSET) << 40) |
           (static_cast<int64_t>(gy + OFFSET) << 20) |
           static_cast<int64_t>(gz + OFFSET);
}

void DEMModel::build_spatial_grid(const std::vector<DEMParticle>& particles, SpatialGrid& grid) const {
    grid.clear();
    double cell_size = config_.grid_cell_size;

    for (size_t i = 0; i < particles.size(); ++i) {
        const auto& p = particles[i];
        if (p.broken) continue;

        int gx = static_cast<int>(std::floor(p.x / cell_size));
        int gy = static_cast<int>(std::floor(p.y / cell_size));
        int gz = static_cast<int>(std::floor(p.z / cell_size));

        int gx_min = gx - static_cast<int>(std::ceil(p.radius / cell_size));
        int gx_max = gx + static_cast<int>(std::ceil(p.radius / cell_size));
        int gy_min = gy - static_cast<int>(std::ceil(p.radius / cell_size));
        int gy_max = gy + static_cast<int>(std::ceil(p.radius / cell_size));
        int gz_min = gz - static_cast<int>(std::ceil(p.radius / cell_size));
        int gz_max = gz + static_cast<int>(std::ceil(p.radius / cell_size));

        for (int cx = gx_min; cx <= gx_max; ++cx) {
            for (int cy = gy_min; cy <= gy_max; ++cy) {
                for (int cz = gz_min; cz <= gz_max; ++cz) {
                    int64_t key = grid_key(cx, cy, cz);
                    grid[key].particle_indices.push_back(i);
                }
            }
        }
    }
}

void DEMModel::find_contacts_grid(const std::vector<DEMParticle>& particles,
                                   const SpatialGrid& grid,
                                   std::vector<Contact>& contacts) const {
    contacts.clear();
    double cell_size = config_.grid_cell_size;
    std::vector<std::pair<size_t, size_t>> checked_pairs;
    checked_pairs.reserve(particles.size() * 4);

    for (const auto& entry : grid) {
        const auto& cell = entry.second;
        if (cell.particle_indices.size() < 2) continue;

        for (size_t a = 0; a < cell.particle_indices.size(); ++a) {
            size_t i = cell.particle_indices[a];
            const auto& p1 = particles[i];
            if (p1.broken) continue;

            for (size_t b = a + 1; b < cell.particle_indices.size(); ++b) {
                size_t j = cell.particle_indices[b];
                const auto& p2 = particles[j];
                if (p2.broken) continue;
                if (i >= j) continue;

                double dx = p2.x - p1.x;
                double dy = p2.y - p1.y;
                double dz = p2.z - p1.z;
                double dist_sq = dx*dx + dy*dy + dz*dz;
                double min_dist = p1.radius + p2.radius;

                if (dist_sq < min_dist * min_dist && dist_sq > 1e-16) {
                    double dist = std::sqrt(dist_sq);
                    Contact c{i, j, dx/dist, dy/dist, dz/dist, min_dist - dist};
                    contacts.push_back(c);
                }
            }
        }
    }

    std::sort(contacts.begin(), contacts.end(),
              [](const Contact& a, const Contact& b) {
                  return a.i < b.i || (a.i == b.i && a.j < b.j);
              });
    auto last = std::unique(contacts.begin(), contacts.end(),
                            [](const Contact& a, const Contact& b) {
                                return a.i == b.i && a.j == b.j;
                            });
    contacts.erase(last, contacts.end());
}

void DEMModel::find_contacts_brute(const std::vector<DEMParticle>& particles,
                                    std::vector<Contact>& contacts) const {
    contacts.clear();
    for (size_t i = 0; i < particles.size(); ++i) {
        if (particles[i].broken) continue;
        for (size_t j = i + 1; j < particles.size(); ++j) {
            if (particles[j].broken) continue;
            const auto& p1 = particles[i];
            const auto& p2 = particles[j];
            double dx = p2.x - p1.x;
            double dy = p2.y - p1.y;
            double dz = p2.z - p1.z;
            double dist = std::sqrt(dx*dx + dy*dy + dz*dz);
            double min_dist = p1.radius + p2.radius;

            if (dist < min_dist && dist > 1e-8) {
                Contact c{i, j, dx/dist, dy/dist, dz/dist, min_dist - dist};
                contacts.push_back(c);
            }
        }
    }
}

std::vector<DEMParticle> DEMModel::generate_particles(size_t count, double min_radius, double max_radius,
                                                      double mill_center_x, double mill_center_y) {
    std::vector<DEMParticle> particles;
    particles.reserve(count);

    std::uniform_real_distribution<> radius_dist(min_radius, max_radius);
    std::uniform_real_distribution<> angle_dist(0, 2 * M_PI);
    std::uniform_real_distribution<> rad_dist(0.1, config_.mill_radius * 0.7);
    std::normal_distribution<> moisture_dist(config_.default_moisture, 0.02);

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
        p.moisture = std::max(0.05, std::min(0.25, moisture_dist(rng_)));
        p.cohesion = config_.moisture_cohesion_base * p.moisture * p.moisture;
        p.broken = false;
        p.is_coarse = false;
        p.coarse_scale = 1;
        particles.push_back(p);
    }
    return particles;
}

std::vector<DEMParticle> DEMModel::coarse_grain(const std::vector<DEMParticle>& fine_particles) {
    if (!config_.use_coarse_graining || config_.coarse_scale <= 1) {
        return fine_particles;
    }

    std::vector<DEMParticle> coarse_particles;
    size_t scale = config_.coarse_scale;
    double r_scale = config_.coarse_radius_scale;

    size_t num_coarse = fine_particles.size() / scale;
    coarse_particles.reserve(num_coarse);

    std::vector<size_t> indices(fine_particles.size());
    std::iota(indices.begin(), indices.end(), 0);
    std::shuffle(indices.begin(), indices.end(), rng_);

    for (size_t g = 0; g < num_coarse; ++g) {
        double avg_x = 0, avg_y = 0, avg_z = 0;
        double avg_vx = 0, avg_vy = 0, avg_vz = 0;
        double total_mass = 0;
        double avg_radius = 0;
        double avg_strength = 0;
        double avg_moisture = 0;
        double avg_cohesion = 0;
        double avg_youngs = 0;
        double avg_poisson = 0;
        double avg_restitution = 0;
        bool any_broken = false;

        for (size_t k = 0; k < scale && g * scale + k < fine_particles.size(); ++k) {
            const auto& fp = fine_particles[indices[g * scale + k]];
            double m = fp.mass;
            avg_x += fp.x * m;
            avg_y += fp.y * m;
            avg_z += fp.z * m;
            avg_vx += fp.vx * m;
            avg_vy += fp.vy * m;
            avg_vz += fp.vz * m;
            total_mass += m;
            avg_radius += fp.radius;
            avg_strength += fp.strength;
            avg_moisture += fp.moisture;
            avg_cohesion += fp.cohesion;
            avg_youngs += fp.youngs_modulus;
            avg_poisson += fp.poisson_ratio;
            avg_restitution += fp.restitution;
            if (fp.broken) any_broken = true;
        }

        if (total_mass > 0) {
            avg_x /= total_mass;
            avg_y /= total_mass;
            avg_z /= total_mass;
            avg_vx /= total_mass;
            avg_vy /= total_mass;
            avg_vz /= total_mass;
        }

        DEMParticle cp{};
        cp.id = next_particle_id_++;
        cp.x = avg_x;
        cp.y = avg_y;
        cp.z = avg_z;
        cp.vx = avg_vx;
        cp.vy = avg_vy;
        cp.vz = avg_vz;
        cp.fx = cp.fy = cp.fz = 0;
        cp.radius = (avg_radius / scale) * r_scale;
        cp.mass = total_mass;
        cp.youngs_modulus = avg_youngs / scale;
        cp.poisson_ratio = avg_poisson / scale;
        cp.restitution = avg_restitution / scale;
        cp.strength = avg_strength / scale * std::sqrt(static_cast<double>(scale));
        cp.moisture = avg_moisture / scale;
        cp.cohesion = avg_cohesion / scale * static_cast<double>(scale);
        cp.broken = any_broken;
        cp.is_coarse = true;
        cp.coarse_scale = scale;

        coarse_particles.push_back(cp);
    }

    return coarse_particles;
}

std::vector<DEMParticle> DEMModel::reconstruct_fine(const std::vector<DEMParticle>& coarse_particles) {
    if (!config_.use_coarse_graining || config_.coarse_scale <= 1) {
        return coarse_particles;
    }

    std::vector<DEMParticle> fine_particles;
    size_t scale = config_.coarse_scale;

    for (const auto& cp : coarse_particles) {
        if (!cp.is_coarse) {
            fine_particles.push_back(cp);
            continue;
        }

        double fine_r = cp.radius / config_.coarse_radius_scale;
        double fine_mass = cp.mass / scale;
        double fine_strength = cp.strength * std::sqrt(1.0 / scale) * scale;

        std::uniform_real_distribution<> angle_dist(0, 2 * M_PI);
        std::normal_distribution<> vel_jitter(0, 0.1);

        for (size_t k = 0; k < scale; ++k) {
            double theta = angle_dist(rng_);
            double phi = std::acos(2 * std::uniform_real_distribution<double>(0, 1)(rng_) - 1);
            double offset = fine_r * 0.5;

            DEMParticle fp{};
            fp.id = next_particle_id_++;
            fp.x = cp.x + offset * std::sin(phi) * std::cos(theta);
            fp.y = cp.y + offset * std::sin(phi) * std::sin(theta);
            fp.z = cp.z + offset * std::cos(phi);
            fp.vx = cp.vx + vel_jitter(rng_);
            fp.vy = cp.vy + vel_jitter(rng_);
            fp.vz = cp.vz + vel_jitter(rng_);
            fp.fx = fp.fy = fp.fz = 0;
            fp.radius = fine_r * (0.9 + std::uniform_real_distribution<double>(0, 0.2)(rng_));
            fp.mass = fine_mass;
            fp.youngs_modulus = cp.youngs_modulus * scale;
            fp.poisson_ratio = cp.poisson_ratio;
            fp.restitution = cp.restitution;
            fp.strength = fine_strength * (0.9 + std::uniform_real_distribution<double>(0, 0.2)(rng_));
            fp.moisture = cp.moisture;
            fp.cohesion = cp.cohesion / scale;
            fp.broken = cp.broken;
            fp.is_coarse = false;
            fp.coarse_scale = 1;

            fine_particles.push_back(fp);
        }
    }

    return fine_particles;
}

DEMResult DEMModel::simulate(const std::vector<DEMParticle>& initial_particles,
                             double roller_speed, double roller_gap,
                             double total_simulation_time) {
    DEMResult result;

    std::vector<DEMParticle> working_particles = initial_particles;

    if (config_.use_coarse_graining && config_.coarse_scale > 1) {
        working_particles = coarse_grain(initial_particles);
        std::cout << "[DEM] Coarse-graining: " << initial_particles.size()
                  << " -> " << working_particles.size()
                  << " particles (scale=" << config_.coarse_scale << ")" << std::endl;
    }

    result.particles = working_particles;
    roller_angle_ = 0.0;

    size_t num_steps = static_cast<size_t>(total_simulation_time / config_.dt);
    double sim_time = 0;
    double max_force = 0;
    double total_force = 0;
    double total_velocity = 0;
    size_t force_samples = 0;
    size_t initial_count = working_particles.size();
    size_t initial_broken_count = 0;
    for (const auto& p : working_particles) if (p.broken) initial_broken_count++;

    std::cout << "[DEM] Starting simulation: " << num_steps << " steps, dt=" << config_.dt
              << ", particles=" << working_particles.size()
              << ", spatial_grid=" << (config_.use_spatial_grid ? "on" : "off")
              << ", coarse_grain=" << (config_.use_coarse_graining ? "on" : "off") << std::endl;

    SpatialGrid grid;

    for (size_t step = 0; step < num_steps; ++step) {
        sim_time += config_.dt;
        roller_angle_ += roller_speed * config_.dt;

        apply_gravity(result.particles);
        compute_roller_forces(result.particles, roller_speed, roller_gap);
        compute_wall_forces(result.particles, roller_gap);

        std::vector<Contact> contacts;
        if (config_.use_spatial_grid) {
            build_spatial_grid(result.particles, grid);
            find_contacts_grid(result.particles, grid, contacts);
        } else {
            find_contacts_brute(result.particles, contacts);
        }

        for (const auto& c : contacts) {
            const auto& p1 = result.particles[c.i];
            const auto& p2 = result.particles[c.j];
            double force = std::abs(p1.fx * c.nx + p1.fy * c.ny + p1.fz * c.nz);
            max_force = std::max(max_force, force);
            total_force += force;
            force_samples++;
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
                      << ", broken=" << (broken_count - initial_broken_count)
                      << ", contacts=" << contacts.size() << std::endl;
        }
    }

    if (config_.use_coarse_graining && config_.coarse_scale > 1) {
        auto fine_particles = reconstruct_fine(result.particles);
        result.particles = fine_particles;
        std::cout << "[DEM] Reconstructed fine particles: " << fine_particles.size() << std::endl;
    }

    size_t final_broken_count = 0;
    for (const auto& p : result.particles) if (p.broken) final_broken_count++;

    size_t original_initial_count = initial_particles.size();
    size_t original_initial_broken = 0;
    for (const auto& p : initial_particles) if (p.broken) original_initial_broken++;

    result.final_distribution = compute_size_distribution(result.particles);
    result.breakage_rate = static_cast<double>(final_broken_count - original_initial_broken) /
                          static_cast<double>(original_initial_count > 0 ? original_initial_count : 1);
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
        double mass = p.mass * (p.is_coarse ? static_cast<double>(p.coarse_scale) : 1.0);
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

double DEMModel::compute_moisture_strength_factor(double moisture) const {
    double ref_moisture = config_.default_moisture;
    double delta = moisture - ref_moisture;
    double factor = 1.0 + config_.moisture_strength_factor * delta / ref_moisture;
    return std::max(0.3, std::min(1.5, factor));
}

double DEMModel::compute_cohesion_force(const DEMParticle& p1, const DEMParticle& p2,
                                         double nx, double ny, double nz) const {
    double avg_cohesion = (p1.cohesion + p2.cohesion) * 0.5;
    double avg_radius = (p1.radius + p2.radius) * 0.5;
    double contact_area = M_PI * avg_radius * avg_radius * 0.1;
    return avg_cohesion * contact_area;
}

double DEMModel::compute_contact_force(const DEMParticle& p1, const DEMParticle& p2,
                                       double& nx, double& ny, double& nz,
                                       double& overlap) {
    double E_star = 1.0 / ((1 - p1.poisson_ratio*p1.poisson_ratio) / p1.youngs_modulus +
                           (1 - p2.poisson_ratio*p2.poisson_ratio) / p2.youngs_modulus);
    double R_star = 1.0 / (1.0 / p1.radius + 1.0 / p2.radius);
    double kn = (4.0 / 3.0) * E_star * std::sqrt(R_star);
    double normal_force = kn * std::pow(overlap, 1.5);

    double damping = 0.1;
    double rel_vel = (p2.vx - p1.vx) * nx + (p2.vy - p1.vy) * ny + (p2.vz - p1.vz) * nz;
    double damping_force = -damping * rel_vel * std::sqrt(p1.mass * p2.mass / (p1.mass + p2.mass));

    double cohesion = compute_cohesion_force(p1, p2, nx, ny, nz);

    double scale = p1.is_coarse || p2.is_coarse ?
                    std::max(static_cast<double>(p1.coarse_scale),
                             static_cast<double>(p2.coarse_scale)) : 1.0;

    return std::max(0.0, (normal_force + damping_force - cohesion) * scale);
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
        double shear_force = force * config_.dynamic_friction;
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

                double moisture_factor = compute_moisture_strength_factor(p.moisture);
                normal_force *= moisture_factor;

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

    double moisture_factor = compute_moisture_strength_factor(particle.moisture);
    double effective_strength = particle.strength * moisture_factor;

    double selection_prob = breakage_model_.selection_function_param *
                            std::pow(contact_stress / effective_strength, 2.0);

    if (contact_stress > effective_strength) {
        return true;
    }

    if (particle.moisture > 0.2) {
        selection_prob *= 0.7;
    } else if (particle.moisture < 0.08) {
        selection_prob *= 1.3;
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
        frag.moisture = particle.moisture * (0.95 + std::uniform_real_distribution<double>(0, 0.1)(rng));
        frag.cohesion = particle.cohesion * 0.8;
        frag.broken = false;
        frag.is_coarse = particle.is_coarse;
        frag.coarse_scale = particle.coarse_scale;
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

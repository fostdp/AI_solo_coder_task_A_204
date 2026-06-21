#include "genetic_optimizer.h"
#include <iostream>
#include <algorithm>
#include <cmath>
#include <sstream>

namespace stone_mill {

GeneticOptimizer::GeneticOptimizer() = default;

void GeneticOptimizer::set_dem_model(std::shared_ptr<DEMModel> model) {
    dem_model_ = std::move(model);
}

void GeneticOptimizer::set_params(const OptimizationParams& params) {
    params_ = params;
}

void GeneticOptimizer::set_breakage_model(const BreakageModel& model) {
    breakage_model_ = model;
}

void GeneticOptimizer::set_fitness_callback(std::function<double(const Individual&)> callback) {
    fitness_callback_ = std::move(callback);
}

std::vector<GeneticOptimizer::Individual> GeneticOptimizer::initialize_population() {
    std::vector<Individual> population;
    population.reserve(params_.population_size);

    std::uniform_real_distribution<> speed_dist(params_.min_speed, params_.max_speed);
    std::uniform_real_distribution<> gap_dist(params_.min_gap, params_.max_gap);

    for (size_t i = 0; i < params_.population_size; ++i) {
        Individual ind{};
        ind.roller_speed = speed_dist(rng_);
        ind.roller_gap = gap_dist(rng_);
        ind.fitness = 0;
        ind.target_ratio = 0;
        ind.yield = 0;
        population.push_back(ind);
    }
    return population;
}

double GeneticOptimizer::evaluate_fitness(Individual& ind,
                                           const std::vector<DEMParticle>& initial_particles,
                                           size_t target_bin_min, size_t target_bin_max) {
    if (!dem_model_) return 0;

    dem_model_->set_breakage_model(breakage_model_);
    DEMResult result = dem_model_->simulate(initial_particles, ind.roller_speed,
                                            ind.roller_gap, 0.05);

    double target_ratio = 0;
    for (size_t i = target_bin_min; i <= target_bin_max && i < GRAIN_SIZE_BINS; ++i) {
        target_ratio += result.final_distribution[i];
    }

    double yield_estimate = result.breakage_rate * 10.0;
    double screening_efficiency = breakage_model_.screening_efficiency;
    double effective_yield = yield_estimate * screening_efficiency;

    double fitness = target_ratio * 0.6 + (effective_yield / 20.0) * 0.4;

    if (target_ratio < 0.3) {
        fitness *= 0.5;
    }

    if (fitness_callback_) {
        fitness = fitness_callback_(ind);
    }

    ind.target_ratio = target_ratio;
    ind.yield = effective_yield;
    ind.fitness = fitness;

    return fitness;
}

void GeneticOptimizer::tournament_selection(const std::vector<Individual>& population,
                                            std::vector<Individual>& mating_pool) {
    mating_pool.clear();
    mating_pool.reserve(population.size());

    const size_t tournament_size = 3;
    std::uniform_int_distribution<> idx_dist(0, population.size() - 1);

    for (size_t i = 0; i < population.size(); ++i) {
        size_t best_idx = idx_dist(rng_);
        for (size_t j = 1; j < tournament_size; ++j) {
            size_t idx = idx_dist(rng_);
            if (population[idx].fitness > population[best_idx].fitness) {
                best_idx = idx;
            }
        }
        mating_pool.push_back(population[best_idx]);
    }
}

void GeneticOptimizer::crossover(const Individual& parent1, const Individual& parent2,
                                 Individual& child1, Individual& child2) {
    std::uniform_real_distribution<> dist(0, 1);

    if (dist(rng_) < params_.crossover_rate) {
        double alpha = dist(rng_);
        child1.roller_speed = alpha * parent1.roller_speed + (1 - alpha) * parent2.roller_speed;
        child1.roller_gap = alpha * parent1.roller_gap + (1 - alpha) * parent2.roller_gap;

        child2.roller_speed = (1 - alpha) * parent1.roller_speed + alpha * parent2.roller_speed;
        child2.roller_gap = (1 - alpha) * parent1.roller_gap + alpha * parent2.roller_gap;
    } else {
        child1 = parent1;
        child2 = parent2;
    }
}

void GeneticOptimizer::mutate(Individual& ind) {
    std::uniform_real_distribution<> dist(0, 1);

    if (dist(rng_) < params_.mutation_rate) {
        std::normal_distribution<> speed_mut(0, (params_.max_speed - params_.min_speed) * 0.1);
        ind.roller_speed += speed_mut(rng_);
        ind.roller_speed = std::max(params_.min_speed, std::min(params_.max_speed, ind.roller_speed));
    }

    if (dist(rng_) < params_.mutation_rate) {
        std::normal_distribution<> gap_mut(0, (params_.max_gap - params_.min_gap) * 0.1);
        ind.roller_gap += gap_mut(rng_);
        ind.roller_gap = std::max(params_.min_gap, std::min(params_.max_gap, ind.roller_gap));
    }
}

std::vector<GeneticOptimizer::Individual> GeneticOptimizer::elitism(
    const std::vector<Individual>& population, double elitism_rate) {

    std::vector<Individual> sorted = population;
    std::sort(sorted.begin(), sorted.end());

    size_t elite_count = static_cast<size_t>(sorted.size() * elitism_rate);
    std::vector<Individual> elites(sorted.begin(), sorted.begin() + elite_count);
    return elites;
}

OptimizationResult GeneticOptimizer::extract_result(const Individual& best, uint32_t generations) {
    OptimizationResult result{};
    result.best_speed = best.roller_speed;
    result.best_gap = best.roller_gap;
    result.predicted_yield = best.yield;
    result.predicted_target_ratio = best.target_ratio;
    result.fitness = best.fitness;
    result.generations = generations;

    std::stringstream ss;
    ss << "population_size=" << params_.population_size
       << ",max_generations=" << params_.max_generations
       << ",mutation_rate=" << params_.mutation_rate
       << ",crossover_rate=" << params_.crossover_rate
       << ",selection_param=" << breakage_model_.selection_function_param
       << ",screening_efficiency=" << breakage_model_.screening_efficiency;
    result.parameters = ss.str();

    return result;
}

OptimizationResult GeneticOptimizer::optimize(const std::vector<DEMParticle>& initial_particles,
                                              size_t target_bin_min, size_t target_bin_max) {
    generations_.clear();

    std::cout << "[GA] Starting optimization: target bins [" << target_bin_min
              << ", " << target_bin_max << "]" << std::endl;

    auto population = initialize_population();

    std::vector<Individual> mating_pool;
    mating_pool.reserve(population.size());

    Individual best_individual{};
    best_individual.fitness = -1;
    uint32_t best_generation = 0;

    for (uint32_t gen = 0; gen < params_.max_generations; ++gen) {
        std::cout << "[GA] Generation " << gen << "/" << params_.max_generations << std::endl;

        for (auto& ind : population) {
            evaluate_fitness(ind, initial_particles, target_bin_min, target_bin_max);
        }

        generations_.push_back(population);

        auto it = std::max_element(population.begin(), population.end(),
            [](const Individual& a, const Individual& b) {
                return a.fitness < b.fitness;
            });

        if (it != population.end() && it->fitness > best_individual.fitness) {
            best_individual = *it;
            best_generation = gen;
            std::cout << "[GA] New best: fitness=" << best_individual.fitness
                      << ", speed=" << best_individual.roller_speed
                      << ", gap=" << best_individual.roller_gap
                      << ", target_ratio=" << best_individual.target_ratio << std::endl;
        }

        double avg_fitness = 0;
        for (const auto& ind : population) avg_fitness += ind.fitness;
        avg_fitness /= population.size();

        std::cout << "[GA] Gen " << gen << ": best=" << it->fitness
                  << ", avg=" << avg_fitness << std::endl;

        if (gen == params_.max_generations - 1) break;

        auto elites = elitism(population);

        tournament_selection(population, mating_pool);

        std::vector<Individual> new_population = elites;

        while (new_population.size() < population.size()) {
            std::uniform_int_distribution<> idx_dist(0, mating_pool.size() - 1);
            size_t p1 = idx_dist(rng_);
            size_t p2 = idx_dist(rng_);
            while (p2 == p1) p2 = idx_dist(rng_);

            Individual c1{}, c2{};
            crossover(mating_pool[p1], mating_pool[p2], c1, c2);
            mutate(c1);
            mutate(c2);
            c1.fitness = 0;
            c2.fitness = 0;

            new_population.push_back(c1);
            if (new_population.size() < population.size()) {
                new_population.push_back(c2);
            }
        }

        population = std::move(new_population);
    }

    std::cout << "[GA] Optimization complete: best fitness=" << best_individual.fitness
              << " at generation " << best_generation << std::endl;

    return extract_result(best_individual, best_generation + 1);
}

}

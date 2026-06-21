#pragma once

#include "common.h"
#include "dem_model.h"
#include <vector>
#include <functional>
#include <random>
#include <memory>

namespace stone_mill {

class GeneticOptimizer {
public:
    struct Individual {
        double roller_speed;
        double roller_gap;
        double fitness;
        double target_ratio;
        double yield;

        bool operator<(const Individual& other) const {
            return fitness > other.fitness;
        }
    };

    GeneticOptimizer();
    GeneticOptimizer(const OptimizationParams& params, const BreakageModel& model);
    ~GeneticOptimizer() = default;

    void set_dem_model(std::shared_ptr<DEMModel> model);
    void set_params(const OptimizationParams& params);
    void set_breakage_model(const BreakageModel& model);

    OptimizationResult optimize(const std::vector<DEMParticle>& initial_particles,
                                size_t target_bin_min, size_t target_bin_max);

    OptimizationResult optimize(size_t target_bin_min, size_t target_bin_max);

    void set_fitness_callback(std::function<double(const Individual&)> callback);

    const std::vector<std::vector<Individual>>& get_generations() const { return generations_; }

private:
    std::vector<Individual> initialize_population();
    double evaluate_fitness(Individual& ind, const std::vector<DEMParticle>& initial_particles,
                          size_t target_bin_min, size_t target_bin_max);

    void tournament_selection(const std::vector<Individual>& population,
                           std::vector<Individual>& mating_pool);

    void crossover(const Individual& parent1, const Individual& parent2,
                  Individual& child1, Individual& child2);

    void mutate(Individual& ind);

    std::vector<Individual> elitism(const std::vector<Individual>& population,
                                   double elitism_rate = 0.1);

    OptimizationResult extract_result(const Individual& best, uint32_t generations);

    std::shared_ptr<DEMModel> dem_model_;
    OptimizationParams params_;
    BreakageModel breakage_model_;
    std::function<double(const Individual&)> fitness_callback_;

    std::vector<std::vector<Individual>> generations_;
    std::mt19937 rng_{std::random_device{}()};
};

}

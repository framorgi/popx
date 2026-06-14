#pragma once
#include "common.h"
#include "i_agents_manager.h"
#include "i_config.h"
#include "i_logger.h"
#include "pop.h"
#include "random_utility.h"

#include <memory>
#include <vector>
class PopsManager : public IAgentsManager {
  public:
    PopsManager(std::shared_ptr<IWorld> world, std::shared_ptr<ILogger> logger, std::shared_ptr<IConfig> config);
    /// @brief Create and register a new agent
    bool spawn_population() override;

    /// @brief Remove an agent
    void update_cycle() override;

    [[nodiscard]] int get_alive_count() const override;
    [[nodiscard]] int get_dead_count() const override {
        return dead_count_;
    }
    [[nodiscard]] int get_total_births() const override {
        return total_births_;
    }
    [[nodiscard]] int get_total_deaths() const override {
        return total_deaths_;
    }
    [[nodiscard]] bool get_forced_respawn_active() const override {
        return forced_respawn_active_;
    }
    [[nodiscard]] int get_forced_respawn_min_alive() const override {
        return static_cast<int>(MinPopulationAllowed);
    }
    [[nodiscard]] unsigned get_generation_count() const override {
        return generation_count_;
    }
    [[nodiscard]] std::vector<PopSnapshot> get_pops_snapshot() const override;

    void trigger_new_generation() override;
    void reset_population_state() override;

  private:
    std::vector<std::shared_ptr<Pop>> pops_;
    std::shared_ptr<IWorld> world_;
    std::shared_ptr<ILogger> logger_;
    std::shared_ptr<IConfig> config_;
    int dead_count_ = 0;
    int total_births_ = 0;
    int total_deaths_ = 0;
    bool forced_respawn_active_ = false;
    unsigned generation_count_ = 0;
    uint64_t cycle_counter_ = 0;
    ///--------------------------------------------------------------------------
    /// Buckets for managing agent states (sense, think, act)
    ///--------------------------------------------------------------------------
    std::vector<std::shared_ptr<Pop>> sense_bucket_;
    std::vector<std::shared_ptr<Pop>> think_bucket_;
    std::vector<std::shared_ptr<Pop>> act_bucket_;
    ///--------------------------------------------------------------------------
    /// Rotates the buckets to cycle agent states
    ///--------------------------------------------------------------------------
    void rotate_buckets();
    ///--------------------------------------------------------------------------
    /// @brief Attempts to spawn an offspring of parent into an adjacent free cell.
    ///--------------------------------------------------------------------------
    void try_reproduce(std::shared_ptr<Pop>& parent);

    [[nodiscard]] double score_pop_for_newgen(const std::shared_ptr<Pop>& pop) const;
    bool try_spawn_random_position(const std::shared_ptr<Pop>& pop, RandomUtility& random_util);
};

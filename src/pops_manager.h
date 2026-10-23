#pragma once
#include "common.h"
#include "i_agents_manager.h"
#include "i_config.h"
#include "i_logger.h"
#include "i_stats.h"
#include "pop.h"
#include "random_utility.h"

#include <tbb/task_arena.h>

#include <deque>
#include <memory>
#include <string>
#include <unordered_set>
#include <vector>
class PopsManager : public IAgentsManager {
  public:
    PopsManager(std::shared_ptr<IWorld> world, std::shared_ptr<ILogger> logger, std::shared_ptr<IConfig> config);
    /// @brief Create and register a new agent
    bool spawn_population() override;
    bool recover_from_extinction() override;

    /// @brief Remove an agent
    void update_cycle() override;

    [[nodiscard]] int get_alive_count() const override;
    [[nodiscard]] uint64_t get_dead_count() const override {
        return dead_count_;
    }
    [[nodiscard]] uint64_t get_total_births() const override {
        return total_births_;
    }
    [[nodiscard]] uint64_t get_total_deaths() const override {
        return total_deaths_;
    }
    [[nodiscard]] bool get_forced_respawn_active() const override {
        return forced_respawn_active_;
    }
    [[nodiscard]] int get_forced_respawn_min_alive() const override {
        return static_cast<int>(std::min(MinPopulationAllowed, config_->get_max_population_allowed()));
    }
    [[nodiscard]] unsigned get_generation_count() const override {
        return generation_count_;
    }
    [[nodiscard]] uint64_t get_repro_processed_this_tick() const override {
        return repro_processed_this_tick_;
    }
    [[nodiscard]] uint64_t get_repro_backlog_size() const override {
        return repro_backlog_.size();
    }
    [[nodiscard]] uint64_t get_repro_backlog_peak() const override {
        return repro_backlog_peak_;
    }
    nlohmann::json reproduction_stats() const {
        return {{"eligible_agent_ticks", repro_eligible_agent_ticks_},
                {"cap_blocked_agent_ticks", repro_cap_blocked_agent_ticks_},
                {"cap_blocked_ticks", repro_cap_blocked_ticks_},
                {"attempts", repro_attempts_},
                {"no_space", repro_no_space_},
                {"births", repro_births_},
                {"dispersed_births", repro_dispersed_births_},
                {"recovery_bank_size", recovery_bank_.size()},
                {"recovery_bank_capacity", recovery_bank_capacity_},
                {"capacity", config_->get_max_population_allowed()}};
    }
    [[nodiscard]] uint64_t get_repro_dropped_total() const override {
        return repro_dropped_total_;
    }
    [[nodiscard]] std::vector<PopSnapshot> get_pops_snapshot() const override;

    void trigger_new_generation() override;
    void reset_population_state() override;

    void set_stats(std::shared_ptr<IStats> stats) {
        stats_ = std::move(stats);
    }
    nlohmann::json telemetry(bool detailed) const;
    nlohmann::json inspect(const std::string& id) const;

  private:
    friend class CheckpointCodec;
    std::shared_ptr<IStats> stats_;
    // Narrow access for the standalone dynamic audit and its regression fixtures.
    friend class SimulationAudit;

    std::vector<std::shared_ptr<Pop>> pops_;
    std::shared_ptr<IWorld> world_;
    std::shared_ptr<ILogger> logger_;
    std::shared_ptr<IConfig> config_;
    /// @brief Bounds concurrency of the read-only-world sense/think phases.
    tbb::task_arena phase_arena_;
    uint64_t dead_count_ = 0;
    uint64_t total_births_ = 0;
    uint64_t total_deaths_ = 0;
    bool forced_respawn_active_ = false;
    unsigned generation_count_ = 0;
    uint64_t cycle_counter_ = 0;
    std::string repro_next_id_;
    std::size_t repro_next_index_ = 0;
    uint64_t repro_processed_this_tick_ = 0;
    uint64_t repro_dropped_total_ = 0;
    uint64_t repro_eligible_agent_ticks_ = 0, repro_cap_blocked_agent_ticks_ = 0, repro_cap_blocked_ticks_ = 0;
    uint64_t repro_attempts_ = 0, repro_no_space_ = 0, repro_births_ = 0, repro_dispersed_births_ = 0;
    uint64_t repro_backlog_peak_ = 0;

    // Compact hereditary records, never live/dead Pop objects or neural activation state.
    static constexpr std::size_t recovery_bank_capacity_ = 64;
    struct RecoverySeed {
        Genome genome;
        PhyT phy;
        std::string parent_id;
        std::string profile_key;
        bool parent_in_run = true; // False after restoring a checkpoint without its old event history.
    };
    std::deque<RecoverySeed> recovery_bank_;
    void remember_for_recovery(const Pop& pop);

    struct ReproBacklogEntry {
        std::weak_ptr<Pop> pop;
        std::string pop_id;
    };

    std::deque<ReproBacklogEntry> repro_backlog_;
    std::unordered_set<std::string> repro_backlog_ids_;
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
    /// @brief Attempts placement in successive rings up to the configured dispersal radius.
    ///--------------------------------------------------------------------------
    void try_reproduce(std::shared_ptr<Pop>& parent);
    void collect_reproducer_candidates();
    void process_reproduction_backlog();
    bool enqueue_reproducer_candidate(const std::shared_ptr<Pop>& pop);
    void clear_reproduction_backlog();

    [[nodiscard]] double score_pop_for_newgen(const std::shared_ptr<Pop>& pop) const;
};

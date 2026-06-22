#pragma once
#include "common.h"
#include "i_agent.h"
#include "pop_snapshot.h"

#include <cstdint>
#include <vector>

class IAgentsManager {
  public:
    virtual ~IAgentsManager() = default;

    /// @brief Create and register a new agent
    virtual bool spawn_population() = 0;

    /// @brief Advance one simulation cycle
    virtual void update_cycle() = 0;

    /// @brief Return the number of currently alive agents.
    [[nodiscard]] virtual int get_alive_count() const = 0;

    /// @brief Return the cumulative number of agents that have died since spawn.
    [[nodiscard]] virtual int get_dead_count() const = 0;

    /// @brief Return the cumulative number of births since simulation start/restart.
    [[nodiscard]] virtual int get_total_births() const = 0;

    /// @brief Return the cumulative number of deaths since simulation start/restart.
    [[nodiscard]] virtual int get_total_deaths() const = 0;

    /// @brief Return a snapshot of all alive agents (copied; safe to read from GUI thread).
    [[nodiscard]] virtual std::vector<PopSnapshot> get_pops_snapshot() const = 0;

    /// @brief Trigger a new generation selection step.
    virtual void trigger_new_generation() = 0;

    /// @brief True while population is below minimum alive threshold and forced respawn logic is active.
    [[nodiscard]] virtual bool get_forced_respawn_active() const = 0;

    /// @brief Minimum alive threshold used by forced respawn logic.
    [[nodiscard]] virtual int get_forced_respawn_min_alive() const = 0;

    /// @brief Current generation index, incremented whenever NewGen is applied.
    [[nodiscard]] virtual unsigned get_generation_count() const = 0;

    /// @brief Number of reproduction attempts processed during the last tick.
    [[nodiscard]] virtual uint64_t get_repro_processed_this_tick() const = 0;

    /// @brief Current backlog size for pending reproducers.
    [[nodiscard]] virtual uint64_t get_repro_backlog_size() const = 0;

    /// @brief Peak backlog size observed since start/restart.
    [[nodiscard]] virtual uint64_t get_repro_backlog_peak() const = 0;

    /// @brief Total candidates dropped because repro backlog was full.
    [[nodiscard]] virtual uint64_t get_repro_dropped_total() const = 0;

    /// @brief Clear internal state so simulation can restart from zero.
    virtual void reset_population_state() = 0;
};

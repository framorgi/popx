#pragma once
#include "common.h"
#include "pop_snapshot.h"

#include <chrono>
#include <cstdint>
#include <string>
#include <vector>

using StatsClock = std::chrono::steady_clock;
using StatsTimePoint = StatsClock::time_point;

///--------------------------------------------------------------------------
/// @brief One sampled record — captured every N simulation ticks.
///--------------------------------------------------------------------------
struct TickRecord {
    uint64_t tick = 0;
    unsigned generation = 0;
    int alive = 0;
    int dead = 0;
    int total_births = 0;
    int total_deaths = 0;
    int births_window = 0;
    int deaths_window = 0;
    double birth_rate_per_sec = 0.0;
    double death_rate_per_sec = 0.0;
    OrganicsT world_organics = {};
    int64_t tick_duration_us = 0;  ///< microseconds for one simulator::update()
    int64_t frame_duration_us = 0; ///< total frame time in microseconds
    double fps = 0.0;
    double avg_movement_energy_loss_active = 0.0;
    double avg_metabolism_energy_loss_active = 0.0;
    double avg_reproduction_energy_loss_active = 0.0;
    double avg_respiration_energy_gain_active = 0.0;
    double avg_thermoregolation_energy_loss_active = 0.0;
};

///--------------------------------------------------------------------------
/// @brief Aggregate summary produced at end_session().
///--------------------------------------------------------------------------
struct SessionSummary {
    StatsTimePoint start_time = {};
    StatsTimePoint end_time = {};
    uint64_t total_ticks = 0;
    unsigned current_generation = 0;
    int peak_alive = 0;
    int total_dead = 0;
    int64_t avg_tick_us = 0;
    int64_t avg_frame_us = 0;
    double avg_fps = 0.0;
    int total_births = 0;
    int total_deaths = 0;
    double avg_birth_rate_per_sec = 0.0;
    double avg_death_rate_per_sec = 0.0;
    OrganicsT last_organics = {}; ///< most recently sampled world organics
    double avg_movement_energy_loss_active = 0.0;
    double avg_metabolism_energy_loss_active = 0.0;
    double avg_reproduction_energy_loss_active = 0.0;
    double avg_respiration_energy_gain_active = 0.0;
    double avg_thermoregolation_energy_loss_active = 0.0;
};

///--------------------------------------------------------------------------
/// @brief Pure interface for the statistics collector.
///        Implementations must be non-blocking so they never stall the sim.
///--------------------------------------------------------------------------
class IStats {
  public:
    virtual ~IStats() = default;

    virtual void begin_session() = 0;
    virtual void end_session() = 0;

    virtual void record_tick(uint64_t tick, unsigned generation, int alive, int dead, int total_births,
                             int total_deaths, const OrganicsT& world_organics,
                             const std::vector<PopSnapshot>& pops_snapshot, std::chrono::microseconds tick_dur) = 0;

    virtual void record_frame(std::chrono::microseconds frame_dur) = 0;

    [[nodiscard]] virtual const std::vector<TickRecord>& get_history() const = 0;
    [[nodiscard]] virtual SessionSummary get_summary() const = 0;

    virtual void set_rate_window_samples(std::size_t samples) = 0;
    [[nodiscard]] virtual std::size_t get_rate_window_samples() const = 0;

    [[nodiscard]] virtual bool save_to_json(const std::string& path) = 0;
};

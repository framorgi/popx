#pragma once
#include "i_stats.h"

///--------------------------------------------------------------------------
/// @brief Concrete stats collector.
///        Stores up to kMaxHistory TickRecords; older records are dropped.
///--------------------------------------------------------------------------
class Stats : public IStats {
  public:
    Stats() = default;

    void begin_session() override;
    void end_session() override;

    void record_tick(uint64_t tick, unsigned generation, int alive, int dead, int total_births, int total_deaths,
                     const OrganicsT& world_organics, std::chrono::microseconds tick_dur) override;

    void record_frame(std::chrono::microseconds frame_dur) override;

    [[nodiscard]] const std::vector<TickRecord>& get_history() const override {
        return history_;
    }
    [[nodiscard]] SessionSummary get_summary() const override;

    void set_rate_window_samples(std::size_t samples) override;
    [[nodiscard]] std::size_t get_rate_window_samples() const override {
        return rate_window_samples_;
    }

    [[nodiscard]] bool save_to_json(const std::string& path) override;

  private:
    static constexpr std::size_t kMaxHistory = 50'000;

    StatsTimePoint start_time_ = {};
    StatsTimePoint end_time_ = {};
    bool session_active_ = false;
    std::vector<TickRecord> history_;
    int64_t last_frame_dur_us_ = 0;
    int peak_alive_ = 0;
    unsigned current_generation_ = 0;
    int current_dead_ = 0;
    int total_births_ = 0;
    int total_deaths_ = 0;
    int64_t total_tick_us_ = 0;
    int64_t total_frame_us_ = 0;
    double total_birth_rate_ = 0.0;
    double total_death_rate_ = 0.0;
    std::size_t rate_window_samples_ = 10;
};

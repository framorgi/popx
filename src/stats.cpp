#include "stats.h"

#include <fstream>
#include <nlohmann/json.hpp>

void Stats::begin_session() {
    start_time_ = StatsClock::now();
    session_active_ = true;
    history_.clear();
    history_.reserve(512);
    peak_alive_ = 0;
    current_generation_ = 0;
    current_dead_ = 0;
    total_births_ = 0;
    total_deaths_ = 0;
    total_tick_us_ = 0;
    total_frame_us_ = 0;
    total_birth_rate_ = 0.0;
    total_death_rate_ = 0.0;
}

void Stats::end_session() {
    if (!session_active_)
        return;
    end_time_ = StatsClock::now();
    session_active_ = false;
}

void Stats::record_tick(uint64_t tick, unsigned generation, int alive, int dead, int total_births, int total_deaths,
                        const OrganicsT& world_organics, std::chrono::microseconds tick_dur) {
    // Rolling window: when full, drop the oldest quarter to amortise the erase cost.
    if (history_.size() >= kMaxHistory) {
        const std::size_t drop = kMaxHistory / 4;
        history_.erase(history_.begin(), history_.begin() + static_cast<std::ptrdiff_t>(drop));
    }

    TickRecord r;
    r.tick = tick;
    r.generation = generation;
    r.alive = alive;
    r.dead = dead;
    r.total_births = total_births;
    r.total_deaths = total_deaths;
    r.world_organics = world_organics;
    r.tick_duration_us = tick_dur.count();
    r.frame_duration_us = last_frame_dur_us_;
    r.fps = last_frame_dur_us_ > 0 ? (1'000'000.0 / static_cast<double>(last_frame_dur_us_)) : 0.0;

    if (!history_.empty()) {
        const std::size_t base_idx =
            (history_.size() > rate_window_samples_) ? history_.size() - rate_window_samples_ : 0;
        const TickRecord& prev = history_[base_idx];
        const auto dt_ticks = static_cast<double>(std::max<uint64_t>(1, tick - prev.tick));
        const auto dt_sec = dt_ticks / 100.0;
        r.births_window = total_births - prev.total_births;
        r.deaths_window = total_deaths - prev.total_deaths;
        r.birth_rate_per_sec = dt_sec > 0.0 ? (static_cast<double>(r.births_window) / dt_sec) : 0.0;
        r.death_rate_per_sec = dt_sec > 0.0 ? (static_cast<double>(r.deaths_window) / dt_sec) : 0.0;
    }

    history_.push_back(r);

    if (alive > peak_alive_)
        peak_alive_ = alive;
    current_generation_ = generation;
    current_dead_ = dead;
    total_births_ = total_births;
    total_deaths_ = total_deaths;
    total_tick_us_ += tick_dur.count();
    total_birth_rate_ += r.birth_rate_per_sec;
    total_death_rate_ += r.death_rate_per_sec;
}

void Stats::record_frame(std::chrono::microseconds frame_dur) {
    last_frame_dur_us_ = frame_dur.count();
    total_frame_us_ += frame_dur.count();
}

SessionSummary Stats::get_summary() const {
    SessionSummary s;
    s.start_time = start_time_;
    s.end_time = session_active_ ? StatsClock::now() : end_time_;
    s.total_ticks = history_.empty() ? 0 : history_.back().tick;
    s.current_generation = current_generation_;
    s.peak_alive = peak_alive_;
    s.total_dead = current_dead_;
    s.total_births = total_births_;
    s.total_deaths = total_deaths_;
    const int64_t n = static_cast<int64_t>(history_.size());
    s.avg_tick_us = n > 0 ? total_tick_us_ / n : 0;
    s.avg_frame_us = n > 0 ? total_frame_us_ / n : 0;
    s.avg_fps = s.avg_frame_us > 0 ? (1'000'000.0 / static_cast<double>(s.avg_frame_us)) : 0.0;
    s.avg_birth_rate_per_sec = n > 0 ? total_birth_rate_ / static_cast<double>(n) : 0.0;
    s.avg_death_rate_per_sec = n > 0 ? total_death_rate_ / static_cast<double>(n) : 0.0;
    s.last_organics = history_.empty() ? OrganicsT{} : history_.back().world_organics;
    return s;
}

void Stats::set_rate_window_samples(std::size_t samples) {
    rate_window_samples_ = std::max<std::size_t>(1, samples);
}

bool Stats::save_to_json(const std::string& path) {
    using json = nlohmann::json;

    auto summary = get_summary();
    auto epoch_ms = [](StatsTimePoint tp) {
        return std::chrono::duration_cast<std::chrono::milliseconds>(tp.time_since_epoch()).count();
    };

    auto organics_to_json = [](const OrganicsT& o) {
        return json{{"glucose", o.c6h12o6}, {"lipids", o.lipids}, {"o2", o.o2},      {"co2", o.co2},
                    {"h2o", o.h2o},         {"n2", o.n2},         {"caco3", o.caco3}};
    };

    json j;
    j["summary"] = {{"start_time_ms", epoch_ms(summary.start_time)},
                    {"end_time_ms", epoch_ms(summary.end_time)},
                    {"total_ticks", summary.total_ticks},
                    {"generation", summary.current_generation},
                    {"peak_alive", summary.peak_alive},
                    {"total_dead", summary.total_dead},
                    {"avg_tick_us", summary.avg_tick_us},
                    {"avg_frame_us", summary.avg_frame_us},
                    {"avg_fps", summary.avg_fps},
                    {"total_births", summary.total_births},
                    {"total_deaths", summary.total_deaths},
                    {"avg_birth_rate_per_sec", summary.avg_birth_rate_per_sec},
                    {"avg_death_rate_per_sec", summary.avg_death_rate_per_sec},
                    {"rate_window_samples", static_cast<uint64_t>(rate_window_samples_)},
                    {"last_organics", organics_to_json(summary.last_organics)}};

    json records = json::array();
    for (const auto& r : history_) {
        records.push_back({{"tick", r.tick},
                           {"generation", r.generation},
                           {"alive", r.alive},
                           {"dead", r.dead},
                           {"total_births", r.total_births},
                           {"total_deaths", r.total_deaths},
                           {"births_window", r.births_window},
                           {"deaths_window", r.deaths_window},
                           {"birth_rate_per_sec", r.birth_rate_per_sec},
                           {"death_rate_per_sec", r.death_rate_per_sec},
                           {"tick_duration_us", r.tick_duration_us},
                           {"frame_duration_us", r.frame_duration_us},
                           {"fps", r.fps},
                           {"organics", organics_to_json(r.world_organics)}});
    }
    j["records"] = std::move(records);

    std::ofstream file(path);
    if (!file.is_open()) {
        return false;
    }

    file << j.dump(2);
    if (!file.good()) {
        return false;
    }

    file.flush();
    return file.good();
}

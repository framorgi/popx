#include "stats.h"

#include <algorithm>
#include <cmath>
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
    total_movement_energy_loss_active_ = 0.0;
    total_metabolism_energy_loss_active_ = 0.0;
    total_reproduction_energy_loss_active_ = 0.0;
    total_respiration_energy_gain_active_ = 0.0;
    total_thermoregolation_energy_loss_active_ = 0.0;
    total_offspring_count_ = 0.0;
    total_lifetime_distance_ = 0.0;
    max_lifetime_distance_ = 0.0;
    total_corr_offspring_lifetime_ = 0.0;
    total_body_temperature_ = 0.0;
    total_abs_temp_delta_opt_ = 0.0;
}

void Stats::end_session() {
    if (!session_active_)
        return;
    end_time_ = StatsClock::now();
    session_active_ = false;
}

void Stats::record_tick(uint64_t tick, unsigned generation, int alive, int dead, int total_births, int total_deaths,
                        const OrganicsT& world_organics, const std::vector<PopSnapshot>& pops_snapshot,
                        std::chrono::microseconds tick_dur) {
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

    if (!pops_snapshot.empty()) {
        double movement_sum = 0.0;
        double metabolism_sum = 0.0;
        double reproduction_sum = 0.0;
        double respiration_sum = 0.0;
        double thermoreg_sum = 0.0;
        double offspring_sum = 0.0;
        double distance_sum = 0.0;
        double temp_sum = 0.0;
        double abs_temp_delta_opt_sum = 0.0;
        double max_distance = 0.0;

        double offspring_sq_sum = 0.0;
        double distance_sq_sum = 0.0;
        double offspring_distance_sum = 0.0;

        for (const auto& pop : pops_snapshot) {
            movement_sum += pop.total_movement_energy_loss;
            metabolism_sum += pop.total_metabolism_energy_loss;
            reproduction_sum += pop.total_reproduction_energy_loss;
            respiration_sum += pop.total_respiration_energy_gain;
            thermoreg_sum += pop.total_thermoregolation_energy_loss;
            offspring_sum += static_cast<double>(pop.offspring);
            distance_sum += pop.lifetime_distance;
            temp_sum += pop.body_temperature;
            abs_temp_delta_opt_sum += std::abs(pop.body_temperature - pop.opt_temperature);
            max_distance = std::max(max_distance, pop.lifetime_distance);

            offspring_sq_sum += static_cast<double>(pop.offspring) * static_cast<double>(pop.offspring);
            distance_sq_sum += pop.lifetime_distance * pop.lifetime_distance;
            offspring_distance_sum += static_cast<double>(pop.offspring) * pop.lifetime_distance;
        }
        const double denom = static_cast<double>(pops_snapshot.size());
        r.avg_movement_energy_loss_active = movement_sum / denom;
        r.avg_metabolism_energy_loss_active = metabolism_sum / denom;
        r.avg_reproduction_energy_loss_active = reproduction_sum / denom;
        r.avg_respiration_energy_gain_active = respiration_sum / denom;
        r.avg_thermoregolation_energy_loss_active = thermoreg_sum / denom;
        r.avg_offspring_count = offspring_sum / denom;
        r.avg_lifetime_distance = distance_sum / denom;
        r.max_lifetime_distance = max_distance;
        r.avg_body_temperature = temp_sum / denom;
        r.avg_abs_temp_delta_opt = abs_temp_delta_opt_sum / denom;

        const double ex = r.avg_offspring_count;
        const double ey = r.avg_lifetime_distance;
        const double ex2 = offspring_sq_sum / denom;
        const double ey2 = distance_sq_sum / denom;
        const double exy = offspring_distance_sum / denom;
        const double var_x = std::max(0.0, ex2 - (ex * ex));
        const double var_y = std::max(0.0, ey2 - (ey * ey));
        const double cov_xy = exy - (ex * ey);
        const double corr_denom = std::sqrt(var_x * var_y);
        r.corr_offspring_lifetime = corr_denom > 1e-12 ? (cov_xy / corr_denom) : 0.0;
    }

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
    total_movement_energy_loss_active_ += r.avg_movement_energy_loss_active;
    total_metabolism_energy_loss_active_ += r.avg_metabolism_energy_loss_active;
    total_reproduction_energy_loss_active_ += r.avg_reproduction_energy_loss_active;
    total_respiration_energy_gain_active_ += r.avg_respiration_energy_gain_active;
    total_thermoregolation_energy_loss_active_ += r.avg_thermoregolation_energy_loss_active;
    total_offspring_count_ += r.avg_offspring_count;
    total_lifetime_distance_ += r.avg_lifetime_distance;
    max_lifetime_distance_ = std::max(max_lifetime_distance_, r.max_lifetime_distance);
    total_corr_offspring_lifetime_ += r.corr_offspring_lifetime;
    total_body_temperature_ += r.avg_body_temperature;
    total_abs_temp_delta_opt_ += r.avg_abs_temp_delta_opt;
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
    s.avg_movement_energy_loss_active = n > 0 ? total_movement_energy_loss_active_ / static_cast<double>(n) : 0.0;
    s.avg_metabolism_energy_loss_active = n > 0 ? total_metabolism_energy_loss_active_ / static_cast<double>(n) : 0.0;
    s.avg_reproduction_energy_loss_active =
        n > 0 ? total_reproduction_energy_loss_active_ / static_cast<double>(n) : 0.0;
    s.avg_respiration_energy_gain_active = n > 0 ? total_respiration_energy_gain_active_ / static_cast<double>(n) : 0.0;
    s.avg_thermoregolation_energy_loss_active =
        n > 0 ? total_thermoregolation_energy_loss_active_ / static_cast<double>(n) : 0.0;
    s.avg_offspring_count = n > 0 ? total_offspring_count_ / static_cast<double>(n) : 0.0;
    s.avg_lifetime_distance = n > 0 ? total_lifetime_distance_ / static_cast<double>(n) : 0.0;
    s.max_lifetime_distance = max_lifetime_distance_;
    s.avg_corr_offspring_lifetime = n > 0 ? total_corr_offspring_lifetime_ / static_cast<double>(n) : 0.0;
    s.avg_body_temperature = n > 0 ? total_body_temperature_ / static_cast<double>(n) : 0.0;
    s.avg_abs_temp_delta_opt = n > 0 ? total_abs_temp_delta_opt_ / static_cast<double>(n) : 0.0;
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
                    {"avg_movement_energy_loss_active", summary.avg_movement_energy_loss_active},
                    {"avg_metabolism_energy_loss_active", summary.avg_metabolism_energy_loss_active},
                    {"avg_reproduction_energy_loss_active", summary.avg_reproduction_energy_loss_active},
                    {"avg_respiration_energy_gain_active", summary.avg_respiration_energy_gain_active},
                    {"avg_thermoregolation_energy_loss_active", summary.avg_thermoregolation_energy_loss_active},
                    {"avg_offspring_count", summary.avg_offspring_count},
                    {"avg_lifetime_distance", summary.avg_lifetime_distance},
                    {"max_lifetime_distance", summary.max_lifetime_distance},
                    {"avg_corr_offspring_lifetime", summary.avg_corr_offspring_lifetime},
                    {"avg_body_temperature", summary.avg_body_temperature},
                    {"avg_abs_temp_delta_opt", summary.avg_abs_temp_delta_opt},
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
                           {"avg_movement_energy_loss_active", r.avg_movement_energy_loss_active},
                           {"avg_metabolism_energy_loss_active", r.avg_metabolism_energy_loss_active},
                           {"avg_reproduction_energy_loss_active", r.avg_reproduction_energy_loss_active},
                           {"avg_respiration_energy_gain_active", r.avg_respiration_energy_gain_active},
                           {"avg_thermoregolation_energy_loss_active", r.avg_thermoregolation_energy_loss_active},
                           {"avg_offspring_count", r.avg_offspring_count},
                           {"avg_lifetime_distance", r.avg_lifetime_distance},
                           {"max_lifetime_distance", r.max_lifetime_distance},
                           {"corr_offspring_lifetime", r.corr_offspring_lifetime},
                           {"avg_body_temperature", r.avg_body_temperature},
                           {"avg_abs_temp_delta_opt", r.avg_abs_temp_delta_opt},
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

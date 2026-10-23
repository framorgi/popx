#pragma once
#include "common.h"
#include "i_config.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

struct ScoreObservation {
    unsigned age;
    int offspring;
    unsigned mitochondria;
    float energy, previous_energy;
    double temperature;
};

/// @brief Per-agent reward, recent baseline and selection policy; no world writes or inherited history.
class Scorer {
  public:
    static constexpr float age_weight = 0.10f, energy_weight = 0.30f, delta_weight = 0.35f,
                           offspring_weight = 0.05f, temperature_weight = 0.20f;
    struct Components {
        float age = 0, energy = 0, delta = 0, offspring = 0, temperature = 0;
    };
    /// @brief Preserve the existing ecological weights, explicitly in one place.
    static Components components(const ScoreObservation& o, const IConfig& config) {
        const float capacity = std::max(1e-6f, std::max(1u, o.mitochondria) *
                                                 static_cast<float>(config.get_phy_energy_per_respiration()));
        const float thermal_range = static_cast<float>(config.get_phy_efficiency_half_range_c());
        return {age_weight * std::clamp(float(o.age) / MaxAge, 0.0f, 1.0f),
                energy_weight * std::clamp((o.energy / MaxEnergy - 0.25f) / 0.75f, -1.0f, 1.0f),
                delta_weight * std::clamp((o.energy - o.previous_energy) / capacity, -1.0f, 1.0f),
                offspring_weight * std::clamp(float(o.offspring) / 25.0f, 0.0f, 1.0f),
                temperature_weight * (2.0f * std::exp(-std::log(2.0f) * std::pow(
                    (static_cast<float>(o.temperature) - static_cast<float>(config.get_phy_opt_temperature()))
                    / thermal_range, 2.0f)) - 1.0f)};
    }
    static float reward(const ScoreObservation& o, const IConfig& config) {
        const auto c = components(o, config);
        const float result = c.age + c.energy + c.delta + c.offspring + c.temperature;
        return std::isfinite(result) ? std::clamp(result, -1.0f, 1.0f) : -1.0f;
    }
    /// @brief Reward relative to the prior running mean, then update that mean. First observation is neutral.
    float observe(const ScoreObservation& o, const IConfig& config) {
        components_ = components(o, config);
        reward_ = reward(o, config);
        if (!observations_)
            baseline_ = reward_;
        signal_ = std::clamp(reward_ - baseline_, -1.0f, 1.0f);
        baseline_ += static_cast<float>(config.get_learning_baseline_rate()) * (reward_ - baseline_);
        ++observations_;
        return signal_;
    }
    static double selection(unsigned offspring, float energy) {
        return double(offspring) + 0.01 * energy;
    }
    void reset() { *this = Scorer{}; }
    float raw_reward() const { return reward_; }
    float baseline() const { return baseline_; }
    float signal() const { return signal_; }
    const Components& last_components() const { return components_; }

  private:
    friend class CheckpointCodec;
    uint64_t observations_ = 0;
    float reward_ = 0, baseline_ = 0, signal_ = 0;
    Components components_;
};

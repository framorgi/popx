#pragma once
#include <array>
#include <cstdint>
#include <nlohmann/json.hpp>
#include <string_view>

/// @brief Lifecycle kinds cannot be confused with transport message kinds.
enum class LifeEvent { Founder, Birth, Death, Cull, Censored };
inline std::string_view event_name(LifeEvent kind) {
    switch (kind) {
        case LifeEvent::Founder:
            return "founder";
        case LifeEvent::Birth:
            return "birth";
        case LifeEvent::Death:
            return "death";
        case LifeEvent::Cull:
            return "cull";
        case LifeEvent::Censored:
            return "censored";
    }
    return "unknown";
}

/// @brief Immutable terminal/initial state; never owns a live Pop.
struct AgentEvent {
    LifeEvent kind;
    nlohmann::json agent;
};

/// @brief Extensible metadata is separated from typed counters and conserved quantities.
struct SessionRecord {
    nlohmann::json metadata;
};
struct BalanceRecord {
    std::array<int64_t, 6> initial_resources, final_resources, death_overflow, resource_residual;
    uint64_t regenerated_glucose, regenerated_o2;
    double initial_energy, final_energy, dead_energy_sink, energy_residual;
};
struct SampleRecord {
    uint64_t tick;
    std::size_t alive;
    double wall_seconds, process_cpu_seconds;
    BalanceRecord balance;
    nlohmann::json measurements;
};
struct DiagnosticResult {
    nlohmann::json measurements;
};

/// @brief Central observer. Calls occur on the simulation thread, after parallel phases join.
class IStats {
  public:
    virtual ~IStats() = default;
    virtual void record_session(SessionRecord session) = 0;
    virtual void record_event(AgentEvent event) = 0;
    virtual void record_sample(SampleRecord sample) = 0;
    virtual void record_diagnostic(DiagnosticResult result) = 0;
};

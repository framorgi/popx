#pragma once
#include "i_communicator.h"
#include "i_stats.h"
#include "i_world.h"

#include <chrono>
#include <ctime>
#include <deque>
#include <memory>

class PopsManager;
/// @brief Authoritative observations and lifetime ledgers, independent of GUI and transport.
class Stats final : public IStats {
  public:
    explicit Stats(std::unique_ptr<ICommunicator> communicator = {});
    void begin_session(nlohmann::json metadata) {
        record_session({std::move(metadata)});
    }
    void record_session(SessionRecord session) override;
    void record_sample(SampleRecord sample) override;
    void record_event(AgentEvent event) override;
    void record_tick_cost(int64_t microseconds);
    void record_frame(int64_t work_us, int64_t interval_us);
    void sample(uint64_t tick, const PopsManager& agents, const IWorld& world);
    void end_session(uint64_t tick, const std::string& reason, const PopsManager& agents, const IWorld& world);
    void publish_status(nlohmann::json status);
    void publish(const std::string& type, nlohmann::json data);
    void record_diagnostic(DiagnosticResult result) override;
    void record_environment(const IWorld& world);
    std::vector<nlohmann::json> commands();
    nlohmann::json transport_status() const;
    nlohmann::json performance() const;
    bool flush(unsigned milliseconds);
    const nlohmann::json& latest() const {
        return latest_;
    }
    const std::string& run_id() const {
        return run_id_;
    }
    void set_tick(uint64_t tick) {
        tick_ = tick;
    }

  private:
    std::unique_ptr<ICommunicator> communicator_;
    std::string run_id_;
    uint64_t tick_ = 0, samples_ = 0, frames_ = 0, measured_ticks_ = 0;
    int64_t tick_us_ = 0, frame_us_ = 0, frame_interval_us_ = 0;
    nlohmann::json latest_, closed_, initial_, counts_, metadata_, sample_sums_;
    uint64_t peak_alive_ = 0, completed_age_ = 0;
    double peak_distance_ = 0, correlation_sum_ = 0;
    std::deque<nlohmann::json> rate_history_;
    std::chrono::steady_clock::time_point started_;
    std::clock_t cpu_started_ = 0;
};

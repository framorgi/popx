#include "simulator.h"

#include "i_agents_manager.h"
#include "i_world.h"
#include "run_clock.h"

#include <stdexcept>
#include <thread>
#include <utility>

Simulator::Simulator(std::shared_ptr<IWorld> world, std::shared_ptr<IAgentsManager> agents_manager,
                     std::shared_ptr<ILogger> logger)
    : world_(std::move(world)), agents_manager_(std::move(agents_manager)), logger_(std::move(logger)) {}
void Simulator::init() {
    tick_count_ = 0;
    state_ = "running";
    reason_.clear();
    // Initialise cells before registering agents; world init clears occupants.
    world_->init();
    agents_manager_->spawn_population();
}
void Simulator::update() {
    if (state_ != "running")
        return;
    // start simulator tick

    agents_manager_->update_cycle();
    { world_->update_cycle(); }

    // simulator tick finished
    tick_count_++;
    if (RunClock::expired(tick_count_, duration_minutes_)) {
        state_ = "finished";
        reason_ = "duration";
    } else if (agents_manager_->get_alive_count() == 0) {
        if (duration_minutes_ == 0 && agents_manager_->recover_from_extinction()) {
            return;
        }
        state_ = "finished";
        reason_ = "extinction";
    }
    // std::this_thread::sleep_for(std::chrono::milliseconds(20)); // Simulate work with a sleep
}

std::shared_ptr<IWorld> Simulator::get_world() const {
    return world_;
}

bool Simulator::update_environment() {
    // TODO: Add environment-specific updates (resource growth, weather, etc.)
    return world_->update_cycle();
}

bool Simulator::update_agents() {
    agents_manager_->update_cycle();
    return true;
}

void Simulator::command(const std::string& name, const nlohmann::json& arguments) {
    if (name == "speed") {
        if (!arguments.at("value").is_number_integer())
            throw std::invalid_argument("Speed must be an integer");
        const auto value = arguments.at("value").get<int64_t>();
        if (value != 1 && value != 2)
            throw std::invalid_argument("Speed must be 1 or 2");
        speed_ = value;
        return;
    }
    if (name == "pause" && state_ == "running")
        state_ = "paused";
    else if (name == "resume" && state_ == "paused")
        state_ = "running";
    else if (name == "stop" && state_ != "finished") {
        state_ = "finished";
        reason_ = "stop";
    } else if (name == "newgen" && state_ != "finished") {
        agents_manager_->trigger_new_generation();
        if (agents_manager_->get_alive_count() == 0) {
            if (duration_minutes_ == 0 && agents_manager_->recover_from_extinction())
                return;
            state_ = "finished";
            reason_ = "extinction";
        }
    } else
        throw std::invalid_argument("Command unavailable in current state: " + name);
}

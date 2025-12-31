#include "simulator.h"

#include <thread>
#include <utility>

#include "i_agents_manager.h"
#include "i_world.h"

Simulator::Simulator(std::shared_ptr<IWorld> world, std::shared_ptr<IAgentsManager > agents_manager,
                     std::shared_ptr<ILogger> logger)
    : world_(std::move(world)), agents_manager_(std::move(agents_manager)), logger_(std::move(logger)) {}
void Simulator::init() {
    agents_manager_->spawn_population();
    world_->init();
}
void Simulator::update() {
    agents_manager_->update_cycle();
    // TODO: Implement simulation tick (call update_agents() and update_environment())
    std::this_thread::sleep_for(std::chrono::milliseconds(800)); // Simulate work with a sleep  
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

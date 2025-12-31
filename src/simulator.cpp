#include "simulator.h"

#include "i_world.h"

Simulator::Simulator(std::shared_ptr<GridWorld> world, std::shared_ptr<PopsManager> agents_manager,
                     std::shared_ptr<ILogger> logger)
    : world_(world), agents_manager_(agents_manager), logger_(logger) {}
void Simulator::init() {
    agents_manager_->spawn_population();
    world_->init();
}
void Simulator::update() {
    // TODO: Implement simulation tick (call update_agents() and update_environment())
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

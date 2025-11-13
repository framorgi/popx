#include "simulator.h"

simulator::simulator( std::shared_ptr<grid_world> world, std::shared_ptr<pops_manager> agents_manager, std::shared_ptr<i_logger> logger ) : world_(world), agents_manager_(agents_manager), logger_(logger) 
{
}
void simulator::init() 
{

    agents_manager_->spawn_population();
    world_->init();

}
void simulator::update()

{
   
}

std::shared_ptr<i_world> simulator::get_world() const {
    return world_;
}

bool simulator::update_environment()
{

    return world_->update_cycle();
}

bool simulator::update_agents()
{
    agents_manager_->update_cycle();
    return true;
}


#include "pops_manager.h"

pops_manager::pops_manager(std::shared_ptr<i_world> world,std::shared_ptr<i_logger> logger) : world_(world), logger_(logger)
{
}

bool pops_manager::spawn_population()
{
    // Example: Spawn a new pop agent and add it to the population
    std::shared_ptr<random_utility> random_util = std::make_shared<random_utility>();

    std::shared_ptr<pop> new_pop = std::make_shared<pop>();
    pops_.push_back(new_pop);
    std::shared_ptr<pop> new_pop2 = std::make_shared<pop>();
    pops_.push_back(new_pop2);
    for (auto& pop : pops_)
    {
        if (pop->is_alive())
        { 
            int w= random_util->rnd_int(0,world_->get_width());
            int h= random_util->rnd_int(0,world_->get_height());
            logger_->debug("Spawning agent at random position (" + std::to_string(w) + ", " + std::to_string(h) + ").");
           while (!(pop->try_spawn(world_, Position{w,h})))
            {
                logger_->warning("Position (" + std::to_string(w) + ", " + std::to_string(h) + ") is occupied. Retrying...");
            }

            pop->init();
            logger_->info("Agent spawned successfully at position (" + std::to_string(w) + ", " + std::to_string(h) + ").");

        }
        else
        {
            // Handle dead agents -- a strang case in wich agent is dead before spawning
            logger_->error("Attempted to spawn a dead agent.");
        }
    }

    return true;
}

void pops_manager::update_cycle()
{
    for (auto& pop : pops_) 
    {
        if (pop->is_alive())
        {
            // Update logic for alive agents
        }
        else
        {
            // Handle dead agents
        }
    }

}

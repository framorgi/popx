#include "pops_manager.h"

PopsManager::PopsManager(std::shared_ptr<IWorld> world,std::shared_ptr<ILogger> logger) : world_(world), logger_(logger)
{
}

bool PopsManager::spawn_population()
{
    // Example: Spawn a new pop agent and add it to the population
    std::shared_ptr<RandomUtility> random_util = std::make_shared<RandomUtility>();
    
    std::shared_ptr<Pop> new_pop = std::make_shared<Pop>();
    pops_.push_back(new_pop);
    std::shared_ptr<Pop> new_pop2 = std::make_shared<Pop>();
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

void PopsManager::update_cycle()
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

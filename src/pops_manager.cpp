#include "pops_manager.h"

#include <utility>

PopsManager::PopsManager(std::shared_ptr<IWorld> world, std::shared_ptr<ILogger> logger)
    : world_(std::move(world)), logger_(std::move(logger)) {}

bool PopsManager::spawn_population() {
    // Example: Spawn a new pop agent and add it to the population
    std::shared_ptr<RandomUtility> random_util = std::make_shared<RandomUtility>();
    for (int i = 0; i < 100; ++i) {
        std::shared_ptr<Pop> new_pop = std::make_shared<Pop>(world_, logger_);
        pops_.push_back(new_pop);
    }
    for (auto& pop : pops_) {
        if (pop->is_alive()) {
            int w = random_util->rnd_int(0, world_->get_width());
            int h = random_util->rnd_int(0, world_->get_height());
            logger_->debug("Spawning agent at random position (" + std::to_string(w) + ", " + std::to_string(h) + ").");
            while (!(pop->try_spawn(Position{w, h}))) {
                logger_->warning("Position (" + std::to_string(w) + ", " + std::to_string(h) +
                                 ") is occupied. Retrying...");
                w = random_util->rnd_int(0, world_->get_width());
                h = random_util->rnd_int(0, world_->get_height());
            }

            pop->init();
            logger_->info("Agent spawned successfully at position (" + std::to_string(w) + ", " + std::to_string(h) +
                          ").");

        } else {
            // Handle dead agents -- a strang case in wich agent is dead before spawning
            logger_->error("Attempted to spawn a dead agent.");
        }
    }

    return true;
}

void PopsManager::update_cycle() {
    for (auto& pop : pops_) {
        if (pop->is_alive()) {
            // TODO: Implement update logic for alive agents (sense, think, act cycle)
            Position p = pop->get_position();

            std::shared_ptr<RandomUtility> random_util = std::make_shared<RandomUtility>();
            p.y += random_util->rnd_int(-3, 3);
            p.x += random_util->rnd_int(-3, 3);
            logger_->debug("Moving agent from (" + std::to_string(pop->get_position().x) + ", " +
                           std::to_string(pop->get_position().y) + ") to (" + std::to_string(p.x) + ", " +
                           std::to_string(p.y) + ").");
            pop->try_move(p);
        } else {
            // TODO: Handle dead agents (remove from vector, cleanup resources)
        }
    }
}

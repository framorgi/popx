#include "pops_manager.h"

#include <utility>

PopsManager::PopsManager(std::shared_ptr<IWorld> world, std::shared_ptr<ILogger> logger)
    : world_(std::move(world)), logger_(std::move(logger)) {
    sense_bucket_.reserve(MAX_ENTITIES);
    think_bucket_.reserve(MAX_ENTITIES);
    act_bucket_.reserve(MAX_ENTITIES);
}

bool PopsManager::spawn_population() {
    // Example: Spawn a new pop agent and add it to the population
    std::shared_ptr<RandomUtility> random_util = std::make_shared<RandomUtility>();
    for (int i = 0; i < MAX_ENTITIES; ++i) {
        std::shared_ptr<Pop> new_pop = std::make_shared<Pop>(world_, logger_);
        pops_.push_back(new_pop);
        int phase = rand() % 3;

        if (phase == 0) {
            sense_bucket_.push_back(new_pop);
        } else if (phase == 1) {
            think_bucket_.push_back(new_pop);
        } else {
            act_bucket_.push_back(new_pop);
        }
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
    // 1. SENSE
    for (auto& pop : sense_bucket_) {
        if (pop->is_alive()) {
            pop->sense();
        }
    }

    // 2. THINK
    for (auto& pop : think_bucket_) {
        if (pop->is_alive()) {
            pop->think();
        }
    }

    // 3. ACT
    for (auto& pop : act_bucket_) {
        if (pop->is_alive()) {
            pop->act();
        }
    }

    // for (auto& pop : pops_) {
    //     if (pop->is_alive()) {
    //         pop->update();
    //     } else {
    //         // TODO: Handle dead agents (remove from vector, cleanup resources)
    //     }
    // }

    rotate_buckets();
}

void PopsManager::rotate_buckets() {
    std::swap(sense_bucket_, think_bucket_);
    std::swap(think_bucket_, act_bucket_);
}
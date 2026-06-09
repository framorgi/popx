#include "pops_manager.h"

#include <algorithm>
#include <execution>
#include <utility>

PopsManager::PopsManager(std::shared_ptr<IWorld> world, std::shared_ptr<ILogger> logger,
                         std::shared_ptr<IConfig> config)
    : world_(std::move(world)), logger_(std::move(logger)), config_(std::move(config)) {
    sense_bucket_.reserve(config_->get_start_population());
    think_bucket_.reserve(config_->get_start_population());
    act_bucket_.reserve(config_->get_start_population());
}

bool PopsManager::spawn_population() {
    // Example: Spawn a new pop agent and add it to the population
    std::shared_ptr<RandomUtility> random_util = std::make_shared<RandomUtility>();
    for (int i = 0; i < config_->get_start_population(); ++i) {
        std::shared_ptr<Pop> new_pop = std::make_shared<Pop>(world_, logger_, config_);
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
            while (!(pop->try_spawn(PositionT{w, h}))) {
                logger_->debug("Position (" + std::to_string(w) + ", " + std::to_string(h) +
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
    // 1. SENSE — read-only on world, writes only per-agent sensor_values_ → safe to parallelise
    std::for_each(std::execution::par_unseq, sense_bucket_.begin(), sense_bucket_.end(),
                  [](const std::shared_ptr<Pop>& pop) {
                      if (pop->is_alive())
                          pop->sense();
                  });

    // 2. THINK — pure local brain computation, no shared state → safe to parallelise
    std::for_each(std::execution::par_unseq, think_bucket_.begin(), think_bucket_.end(),
                  [](const std::shared_ptr<Pop>& pop) {
                      if (pop->is_alive())
                          pop->think();
                  });

    // 3. ACT — writes world (move, feromones, kill) → must remain sequential
    for (auto& pop : act_bucket_) {
        if (pop->is_alive()) {
            pop->act();
        }
    }

    // Update all agents; those that die here will be despawned below
    for (auto& pop : pops_) {
        if (pop->is_alive()) {
            pop->update();
            if (!pop->is_alive()) {
                logger_->info("An agent has died at position (" + std::to_string(pop->get_position().x) + ", " +
                              std::to_string(pop->get_position().y) + ").");
            }
        }
    }

    // Despawn dead agents: remove reference from world, then erase from all containers
    auto is_dead = [](const std::shared_ptr<Pop>& p) { return !p->is_alive(); };

    for (auto& pop : pops_) {
        if (!pop->is_alive()) {
            world_->remove_entity(pop);
        }
    }

    auto erase_dead = [&](std::vector<std::shared_ptr<Pop>>& bucket) {
        bucket.erase(std::remove_if(bucket.begin(), bucket.end(), is_dead), bucket.end());
    };

    erase_dead(sense_bucket_);
    erase_dead(think_bucket_);
    erase_dead(act_bucket_);
    erase_dead(pops_);

    // Reproduction phase: snapshot parents ready to reproduce, then spawn offspring
    std::vector<std::shared_ptr<Pop>> reproducers;
    for (auto& pop : pops_) {
        if (pop->wants_to_reproduce()) {
            reproducers.push_back(pop);
        } else {
            if (pops_.size() < MinPopulationAllowed && reproducers.size() < (MinPopulationAllowed - pops_.size()))

            {
                logger_->warning("Population is critically low (" + std::to_string(pops_.size()) +
                                 " agents). Forcing reproduction of agent at position (" +
                                 std::to_string(pop->get_position().x) + ", " + std::to_string(pop->get_position().y) +
                                 ").");
                reproducers.push_back(pop);
            }
        }
    }
    for (auto& parent : reproducers) {
        if (pops_.size() < MaxPopulationAllowed)
            try_reproduce(parent);
    }

    rotate_buckets();
}

void PopsManager::try_reproduce(std::shared_ptr<Pop>& parent) {
    auto child = std::make_shared<Pop>(world_, logger_, config_, parent->make_offspring_genome());

    // Try all 8 adjacent cells (cardinal first, then diagonal)
    const int dx[] = {0, 1, 0, -1, 1, 1, -1, -1};
    const int dy[] = {-1, 0, 1, 0, -1, 1, 1, -1};
    const PositionT pp = parent->get_position();

    for (int i = 0; i < 8; ++i) {
        PositionT candidate{pp.x + dx[i], pp.y + dy[i]};
        if (child->try_spawn(candidate)) {
            unsigned g = 0, w = 0, ca = 0, co = 0;
            parent->donate_resources(g, w, ca, co);
            child->set_resources(g, w, ca, co);

            pops_.push_back(child);
            const int phase = rand() % 3;
            if (phase == 0)
                sense_bucket_.push_back(child);
            else if (phase == 1)
                think_bucket_.push_back(child);
            else
                act_bucket_.push_back(child);

            logger_->info("Incrementing offspring count for parent at (" + std::to_string(pp.x) + "," +
                          std::to_string(pp.y) + ").");
            parent->increment_offspring_count();

            logger_->info("Offspring spawned at (" + std::to_string(candidate.x) + "," + std::to_string(candidate.y) +
                          ") from parent at (" + std::to_string(pp.x) + "," + std::to_string(pp.y) + ").");
            return;
        }
    }
    // No free adjacent cell this cycle — parent retains resources, will retry next cycle.
}

void PopsManager::rotate_buckets() {
    std::swap(sense_bucket_, think_bucket_);
    std::swap(think_bucket_, act_bucket_);
}
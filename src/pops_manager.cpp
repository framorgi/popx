#include "pops_manager.h"

#include <algorithm>
#include <cmath>
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
            ++total_births_;
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
    ++cycle_counter_;
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
            ++dead_count_;
            ++total_deaths_;
        }
    }

    auto erase_dead = [&](std::vector<std::shared_ptr<Pop>>& bucket) {
        bucket.erase(std::remove_if(bucket.begin(), bucket.end(), is_dead), bucket.end());
    };

    erase_dead(sense_bucket_);
    erase_dead(think_bucket_);
    erase_dead(act_bucket_);
    erase_dead(pops_);

    forced_respawn_active_ = pops_.size() < MinPopulationAllowed;

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

    if (config_->get_newgen_enabled()) {
        const unsigned interval = std::max<unsigned>(1, config_->get_newgen_interval_ticks());
        if (cycle_counter_ % interval == 0) {
            trigger_new_generation();
        }
    }
}

void PopsManager::try_reproduce(std::shared_ptr<Pop>& parent) {
    // Temporarily increase mutation rate for offspring creation, then reset to original rate
    auto old_mutation_rate = config_->get_point_mutation_rate();
    if (pops_.size() < MinPopulationAllowed) {
        auto temp_mutation_rate = old_mutation_rate * 10; // Increase mutation rate by 10% for each offspring
        config_->set_point_mutation_rate(temp_mutation_rate);
    }
    auto child =
        std::make_shared<Pop>(world_, logger_, config_, parent->make_offspring_genome(), parent->make_offspring_phy());

    config_->set_point_mutation_rate(old_mutation_rate);
    // Try all 8 adjacent cells (cardinal first, then diagonal)
    const int dx[] = {0, 1, 0, -1, 1, 1, -1, -1};
    const int dy[] = {-1, 0, 1, 0, -1, 1, 1, -1};
    const PositionT pp = parent->get_position();

    for (int i = 0; i < 8; ++i) {
        PositionT candidate{pp.x + dx[i], pp.y + dy[i]};
        if (child->try_spawn(candidate)) {
            unsigned g = 0, w = 0, ca = 0, co2 = 0;
            parent->donate_resources(g, w, ca, co2);
            child->set_resources(g, w, ca, co2);

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
            ++total_births_;

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

int PopsManager::get_alive_count() const {
    return static_cast<int>(pops_.size());
}

double PopsManager::score_pop_for_newgen(const std::shared_ptr<Pop>& pop) const {
    if (!pop) {
        return -1.0;
    }
    // Default criterion: offspring count (easy to replace with reward-based score).
    return static_cast<double>(pop->get_offspring_count()) + 0.01 * static_cast<double>(pop->get_energy());
}

bool PopsManager::try_spawn_random_position(const std::shared_ptr<Pop>& pop, RandomUtility& random_util) {
    if (!pop) {
        return false;
    }
    constexpr int max_attempts = 2048;
    for (int attempt = 0; attempt < max_attempts; ++attempt) {
        int w = random_util.rnd_int(0, world_->get_width());
        int h = random_util.rnd_int(0, world_->get_height());
        if (pop->try_spawn(PositionT{w, h})) {
            return true;
        }
    }
    return false;
}

void PopsManager::trigger_new_generation() {
    if (pops_.empty()) {
        return;
    }

    std::vector<std::shared_ptr<Pop>> alive;
    alive.reserve(pops_.size());
    for (const auto& pop : pops_) {
        if (pop && pop->is_alive()) {
            alive.push_back(pop);
        }
    }
    if (alive.empty()) {
        return;
    }

    std::sort(alive.begin(), alive.end(), [&](const std::shared_ptr<Pop>& a, const std::shared_ptr<Pop>& b) {
        return score_pop_for_newgen(a) > score_pop_for_newgen(b);
    });

    const auto ratio = std::clamp(config_->get_newgen_survival_ratio(), 0.01, 1.0);
    const int min_survivors = std::max(1u, config_->get_newgen_min_survivors());
    int survivors_n = static_cast<int>(std::round(static_cast<double>(alive.size()) * ratio));
    survivors_n = std::clamp(survivors_n, min_survivors, static_cast<int>(alive.size()));

    std::vector<std::shared_ptr<Pop>> survivors(alive.begin(), alive.begin() + survivors_n);

    for (auto& pop : alive) {
        world_->remove_entity(pop);
    }

    const int culled = static_cast<int>(alive.size()) - survivors_n;
    dead_count_ += culled;
    total_deaths_ += culled;
    pops_.clear();
    sense_bucket_.clear();
    think_bucket_.clear();
    act_bucket_.clear();

    RandomUtility random_util;
    for (auto& pop : survivors) {
        if (!try_spawn_random_position(pop, random_util)) {
            continue;
        }
        pops_.push_back(pop);
        const int phase = rand() % 3;
        if (phase == 0) {
            sense_bucket_.push_back(pop);
        } else if (phase == 1) {
            think_bucket_.push_back(pop);
        } else {
            act_bucket_.push_back(pop);
        }
    }

    ++generation_count_;
    for (const auto& pop : pops_) {
        if (pop) {
            pop->serialize_brain(generation_count_);
        }
    }

    logger_->info("NewGen applied. generation=" + std::to_string(generation_count_) +
                  " survivors=" + std::to_string(pops_.size()) + " dead_total=" + std::to_string(dead_count_));
}

void PopsManager::reset_population_state() {
    for (auto& pop : pops_) {
        if (pop) {
            world_->remove_entity(pop);
        }
    }
    pops_.clear();
    sense_bucket_.clear();
    think_bucket_.clear();
    act_bucket_.clear();
    dead_count_ = 0;
    total_births_ = 0;
    total_deaths_ = 0;
    forced_respawn_active_ = false;
    generation_count_ = 0;
    cycle_counter_ = 0;
}

std::vector<PopSnapshot> PopsManager::get_pops_snapshot() const {
    std::vector<PopSnapshot> snaps;
    snaps.reserve(pops_.size());
    for (const auto& pop : pops_) {
        if (!pop || !pop->is_alive())
            continue;
        PopSnapshot s;
        s.pop_id = pop->get_pop_id();
        s.age = pop->get_age();
        s.energy = pop->get_energy();
        s.pos = pop->get_position();
        s.mitochondrions = pop->get_phy().mitochondrions;
        s.chloroplasts = pop->get_phy().chloroplasts;
        s.sensitiveness = pop->get_phy().sensitiveness;
        s.adipose_stock_max = pop->get_phy().adipose_stock_max;
        s.body_temperature = pop->get_body_temperature();
        s.glucose = pop->get_glucose();
        s.water = pop->get_water();
        s.o2 = pop->get_o2();
        s.co2 = pop->get_co2();
        s.calcium = pop->get_calcium();
        s.lipids = pop->get_lipids();
        s.learning_score = pop->get_learning_score();
        s.total_connections = pop->get_brain_total_connections();
        s.useful_connections = pop->get_brain_useful_connections(0.05f);
        s.top_active_connections = pop->get_brain_top_active_connections(0.05f, 128);
        s.genetic_color = pop->get_genetic_color();
        s.offspring = pop->get_offspring_count();
        s.is_photosynthetic = pop->get_phy().chloroplasts > pop->get_phy().mitochondrions;
        snaps.push_back(s);
    }
    return snaps;
}
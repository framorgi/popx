#include "pops_manager.h"

#include <tbb/parallel_for.h>

#include <algorithm>
#include <cmath>
#include <limits>
#include <random>
#include <stdexcept>
#include <utility>

PopsManager::PopsManager(std::shared_ptr<IWorld> world, std::shared_ptr<ILogger> logger,
                         std::shared_ptr<IConfig> config)
    : world_(std::move(world)), logger_(std::move(logger)), config_(std::move(config)),
      phase_arena_(static_cast<int>(
          std::clamp(config_->get_num_threads(), 1u, static_cast<unsigned>(std::numeric_limits<int>::max())))) {
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
    std::size_t available = 0;
    for (int y = 0; y < world_->get_height(); ++y)
        for (int x = 0; x < world_->get_width(); ++x)
            available += world_->is_free({x, y});
    if (available < pops_.size())
        throw std::runtime_error("Not enough free cells for the population");
    for (auto& pop : pops_) {
        if (pop->is_alive()) {
            int w = random_util->rnd_int(0, world_->get_width());
            int h = random_util->rnd_int(0, world_->get_height());
            logger_->debug("Spawning agent at random position (" + std::to_string(w) + ", " + std::to_string(h) + ").");
            unsigned attempts = 0;
            while (!(pop->try_spawn(PositionT{w, h}))) {
                if (++attempts > 1000) {
                    bool found = false;
                    for (int y = 0; y < world_->get_height() && !found; ++y)
                        for (int x = 0; x < world_->get_width(); ++x)
                            if (world_->is_free({x, y})) {
                                w = x;
                                h = y;
                                found = true;
                                break;
                            }
                    if (!found)
                        throw std::runtime_error("No free spawn cell");
                    continue;
                }
                logger_->debug("Position (" + std::to_string(w) + ", " + std::to_string(h) +
                               ") is occupied. Retrying...");
                w = random_util->rnd_int(0, world_->get_width());
                h = random_util->rnd_int(0, world_->get_height());
            }

            pop->init();
            ++total_births_;
            if (stats_)
                stats_->record_event({LifeEvent::Founder, pop->telemetry(true)});
            logger_->info("Agent spawned successfully at position (" + std::to_string(w) + ", " + std::to_string(h) +
                          ").");

        } else {
            // Handle dead agents -- a strang case in wich agent is dead before spawning
            logger_->error("Attempted to spawn a dead agent.");
        }
    }

    return true;
}

void PopsManager::remember_for_recovery(const Pop& pop) {
    const auto phy = pop.get_phy();
    const auto key = pop.birth_genome_fingerprint() + ":" + std::to_string(phy.mitochondrions) + ":" +
                     std::to_string(phy.chloroplasts) + ":" + std::to_string(phy.sensitiveness) + ":" +
                     std::to_string(phy.adipose_stock_max);
    // One latest donor per birth-genome/PHY profile prevents identical deaths from filling the archive.
    std::erase_if(recovery_bank_, [&](const RecoverySeed& seed) { return seed.profile_key == key; });
    recovery_bank_.push_back({pop.heritable_genome(), phy, pop.get_pop_id(), key});
    if (recovery_bank_.size() > recovery_bank_capacity_)
        recovery_bank_.pop_front();
}

bool PopsManager::recover_from_extinction() {
    if (!pops_.empty())
        return true;

    clear_reproduction_backlog();
    const auto capacity =
        static_cast<std::size_t>(world_->get_width()) * static_cast<std::size_t>(world_->get_height());
    const auto target = std::min<std::size_t>({MinPopulationAllowed, capacity, config_->get_max_population_allowed()});
    if (target == 0)
        return false;

    auto random_util = std::make_shared<RandomUtility>();
    std::vector<std::size_t> donors;
    for (std::size_t i = 0; i < recovery_bank_.size(); ++i)
        donors.push_back(i);
    for (std::size_t i = 0; i < target; ++i) {
        // Balanced random sampling: use every archived profile once before repeating any donor.
        if (!donors.empty() && i % donors.size() == 0)
            for (std::size_t n = donors.size(); n > 1; --n)
                std::swap(donors[n - 1], donors[static_cast<std::size_t>(random_util->rnd_int(0, static_cast<int>(n - 1)))]);
        const auto* seed = donors.empty() ? nullptr : &recovery_bank_[donors[i % donors.size()]];
        auto pop = seed ? std::make_shared<Pop>(world_, logger_, config_, seed->genome, seed->phy)
                        : std::make_shared<Pop>(world_, logger_, config_);
        pop->set_birth_provenance(seed ? seed->parent_id : "", true);

        bool spawned = false;
        for (unsigned attempt = 0; attempt < 1000 && !spawned; ++attempt) {
            spawned = pop->try_spawn(
                {random_util->rnd_int(0, world_->get_width() - 1), random_util->rnd_int(0, world_->get_height() - 1)});
        }
        for (int y = 0; y < world_->get_height() && !spawned; ++y)
            for (int x = 0; x < world_->get_width() && !spawned; ++x)
                spawned = pop->try_spawn({x, y});
        if (!spawned)
            return false;

        pops_.push_back(pop);
        switch (rand() % 3) {
            case 0:
                sense_bucket_.push_back(pop);
                break;
            case 1:
                think_bucket_.push_back(pop);
                break;
            default:
                act_bucket_.push_back(pop);
                break;
        }
        ++total_births_;
        if (stats_) {
            auto event = pop->telemetry(true);
            event["recovery_seed"] = true;
            event["recovery_from_archive"] = seed != nullptr;
            event["recovery_parent_external"] = seed && !seed->parent_in_run;
            stats_->record_event({LifeEvent::Birth, std::move(event)});
        }
    }
    forced_respawn_active_ = true;
    logger_->warning("Endless run recovered from extinction with " + std::to_string(target) + " forced births.");
    return true;
}

void PopsManager::update_cycle() {
    ++cycle_counter_;
    // Each parallel phase joins before the next; only per-Pop state is written.
    phase_arena_.execute([&] {
        tbb::parallel_for(std::size_t{0}, sense_bucket_.size(), [&](std::size_t i) {
            if (sense_bucket_[i]->is_alive())
                sense_bucket_[i]->sense();
        });
        tbb::parallel_for(std::size_t{0}, think_bucket_.size(), [&](std::size_t i) {
            if (think_bucket_[i]->is_alive())
                think_bucket_[i]->think();
        });
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

    cleanup_external_deaths();

    forced_respawn_active_ = false; // Only extinction recovery sets this flag; low counts do not force births.

    collect_reproducer_candidates();
    process_reproduction_backlog();

    rotate_buckets();

    if (config_->get_newgen_enabled()) {
        const unsigned interval = std::max<unsigned>(1, config_->get_newgen_interval_ticks());
        if (cycle_counter_ % interval == 0) {
            trigger_new_generation();
        }
    }
}

void PopsManager::cleanup_external_deaths() {
    // Also runs after the world update so checkpoint capture never sees a dead Pop.
    auto is_dead = [](const std::shared_ptr<Pop>& p) { return !p->is_alive(); };

    for (auto& pop : pops_) {
        if (!pop->is_alive()) {
            remember_for_recovery(*pop);
            if (stats_)
                stats_->record_event({LifeEvent::Death, pop->telemetry(false)});
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

}

void PopsManager::collect_reproducer_candidates() {
    uint64_t dropped_this_tick = 0;
    const bool full = pops_.size() >= config_->get_max_population_allowed();
    if (full)
        ++repro_cap_blocked_ticks_;
    std::size_t start = pops_.empty() ? 0 : repro_next_index_ % pops_.size();
    for (std::size_t i = 0; i < pops_.size(); ++i)
        if (pops_[i] && pops_[i]->get_pop_id() == repro_next_id_) {
            start = i;
            break;
        }
    for (std::size_t offset = 0; offset < pops_.size(); ++offset) {
        const auto index = (start + offset) % pops_.size();
        auto& pop = pops_[index];
        if (!pop || !pop->is_alive()) {
            continue;
        }

        if (!pop->wants_to_reproduce()) {
            continue;
        }

        ++repro_eligible_agent_ticks_;
        if (full) {
            ++repro_cap_blocked_agent_ticks_;
            pop->record_reproduction_admission(true, repro_backlog_ids_.contains(pop->get_pop_id()), false);
            continue;
        }
        const uint64_t dropped_before = repro_dropped_total_;
        const bool queued = repro_backlog_ids_.contains(pop->get_pop_id());
        const bool admitted = enqueue_reproducer_candidate(pop);
        pop->record_reproduction_admission(false, queued, admitted);
        if (admitted) {
            repro_next_index_ = (index + 1) % pops_.size();
            repro_next_id_ = pops_[repro_next_index_]->get_pop_id();
        }
        if (repro_dropped_total_ != dropped_before) {
            ++dropped_this_tick;
        }
    }

    if (dropped_this_tick > 0) {
        logger_->warning("Reproduction backlog full (" + std::to_string(config_->get_max_repro_backlog()) +
                         "), dropped " + std::to_string(dropped_this_tick) + " candidates this tick.");
    }
}

void PopsManager::process_reproduction_backlog() {
    repro_processed_this_tick_ = 0;

    const unsigned budget = std::max(1u, config_->get_max_reproducers_per_tick());

    unsigned inspected = 0;
    while (!repro_backlog_.empty() && inspected < budget && pops_.size() < config_->get_max_population_allowed()) {
        ReproBacklogEntry entry = repro_backlog_.front();
        repro_backlog_.pop_front();
        repro_backlog_ids_.erase(entry.pop_id);
        ++inspected;

        auto parent = entry.pop.lock();
        if (!parent || !parent->is_alive()) {
            continue;
        }
        if (!parent->wants_to_reproduce()) {
            continue;
        }

        try_reproduce(parent);
        ++repro_processed_this_tick_;
    }
}

bool PopsManager::enqueue_reproducer_candidate(const std::shared_ptr<Pop>& pop) {
    if (!pop || !pop->is_alive()) {
        return false;
    }

    const std::string& pop_id = pop->get_pop_id();
    if (repro_backlog_ids_.find(pop_id) != repro_backlog_ids_.end()) {
        return false;
    }

    const std::size_t max_backlog = static_cast<std::size_t>(std::max(1u, config_->get_max_repro_backlog()));
    if (repro_backlog_.size() >= max_backlog) {
        ++repro_dropped_total_;
        return false;
    }

    repro_backlog_.push_back(ReproBacklogEntry{pop, pop_id});
    repro_backlog_ids_.insert(pop_id);
    repro_backlog_peak_ = std::max<uint64_t>(repro_backlog_peak_, repro_backlog_.size());
    return true;
}

void PopsManager::clear_reproduction_backlog() {
    repro_backlog_.clear();
    repro_backlog_ids_.clear();
    repro_processed_this_tick_ = 0;
}

void PopsManager::try_reproduce(std::shared_ptr<Pop>& parent) {
    if (!parent || !parent->wants_to_reproduce())
        return;
    const float reproduction_energy_loss = static_cast<float>(config_->get_phy_energy_per_respiration());
    if (parent->get_energy() <= reproduction_energy_loss)
        return;
    ++repro_attempts_;
    // The configured mutation rate applies to both genome and PHY at every population size.
    auto child =
        std::make_shared<Pop>(world_, logger_, config_, parent->make_offspring_genome(), parent->make_offspring_phy());
    child->set_birth_provenance(parent->get_pop_id(), false);

    const PositionT pp = parent->get_position();
    // Shuffle each ring to avoid a persistent compass bias; try outer rings only after inner ones fail.
    const unsigned max_radius = std::min(config_->get_offspring_dispersal_radius(),
                                         static_cast<unsigned>(std::max(world_->get_width(), world_->get_height()) - 1));
    for (unsigned radius = 1; radius <= max_radius; ++radius) {
        std::vector<PositionT> candidates;
        candidates.reserve(radius * 8);
        for (int dy = -static_cast<int>(radius); dy <= static_cast<int>(radius); ++dy)
            for (int dx = -static_cast<int>(radius); dx <= static_cast<int>(radius); ++dx)
                if (std::max(std::abs(dx), std::abs(dy)) == static_cast<int>(radius)) {
                    const PositionT candidate{pp.x + dx, pp.y + dy};
                    if (candidate.x >= 0 && candidate.x < world_->get_width() && candidate.y >= 0 &&
                        candidate.y < world_->get_height())
                        candidates.push_back(candidate);
                }
        for (std::size_t i = candidates.size(); i > 1; --i)
            std::swap(candidates[i - 1], candidates[static_cast<std::size_t>(rand()) % i]);
        for (const PositionT candidate : candidates) {
            if (!child->try_spawn(candidate))
                continue;
            unsigned g = 0, w = 0, ca = 0, co2 = 0, o2 = 0, lipids = 0;
            float energy = 0.0f;
            parent->add_reproduction_energy_loss(reproduction_energy_loss);
            parent->donate_resources(g, w, ca, co2, o2, lipids, energy);
            child->set_resources(g, w, ca, co2, o2, lipids, energy);

            pops_.push_back(child);
            const int phase = rand() % 3;
            if (phase == 0)
                sense_bucket_.push_back(child);
            else if (phase == 1)
                think_bucket_.push_back(child);
            else
                act_bucket_.push_back(child);

            logger_->debug("Incrementing offspring count for parent at (" + std::to_string(pp.x) + "," +
                           std::to_string(pp.y) + ").");
            parent->increment_offspring_count();
            ++total_births_;
            ++repro_births_;
            if (radius > 1)
                ++repro_dispersed_births_;
            if (stats_)
                stats_->record_event({LifeEvent::Birth, child->telemetry(true)});

            logger_->debug("Offspring spawned at (" + std::to_string(candidate.x) + "," + std::to_string(candidate.y) +
                           ") from parent at (" + std::to_string(pp.x) + "," + std::to_string(pp.y) + ").");
            return;
        }
    }
    ++repro_no_space_;
    // No free cell within the configured radius; parent retains resources.
}

void PopsManager::rotate_buckets() {
    // Move each group to its next phase: sense -> think -> act -> sense.
    std::swap(think_bucket_, act_bucket_);
    std::swap(sense_bucket_, think_bucket_);
}

int PopsManager::get_alive_count() const {
    return static_cast<int>(pops_.size());
}

double PopsManager::score_pop_for_newgen(const std::shared_ptr<Pop>& pop) const {
    if (!pop) {
        return -1.0;
    }
    // Default criterion: offspring count (easy to replace with reward-based score).
    return Scorer::selection(pop->get_offspring_count(), pop->get_energy());
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
    const int min_survivors =
        std::min(static_cast<int>(alive.size()), static_cast<int>(std::max(1u, config_->get_newgen_min_survivors())));
    int survivors_n = static_cast<int>(std::round(static_cast<double>(alive.size()) * ratio));
    survivors_n = std::clamp(survivors_n, min_survivors, static_cast<int>(alive.size()));

    // Cull only rejected agents. Surviving identities, traits, resources and learned weights remain unchanged.
    for (std::size_t i = static_cast<std::size_t>(survivors_n); i < alive.size(); ++i) {
        alive[i]->die();
        if (stats_)
            stats_->record_event({LifeEvent::Cull, alive[i]->telemetry(false)});
        world_->remove_entity(alive[i]);
    }

    const int culled = static_cast<int>(alive.size()) - survivors_n;
    dead_count_ += culled;
    total_deaths_ += culled;
    const auto is_dead = [](const std::shared_ptr<Pop>& pop) { return !pop->is_alive(); };
    std::erase_if(pops_, is_dead);
    std::erase_if(sense_bucket_, is_dead);
    std::erase_if(think_bucket_, is_dead);
    std::erase_if(act_bucket_, is_dead);
    clear_reproduction_backlog();

    // Choose distinct cells across the whole map. Prefer a two-cell clearance so the
    // intervention breaks clusters when space permits; fall back to any free cell.
    const int width = world_->get_width(), height = world_->get_height();
    std::unordered_set<const IEntity*> survivors;
    for (const auto& pop : pops_)
        survivors.insert(pop.get());
    std::vector<PositionT> candidates;
    candidates.reserve(static_cast<std::size_t>(width) * height);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const PositionT position{x, y};
            const auto cell = world_->get_cell(position);
            const auto occupant = cell->get_occupant().lock();
            if (cell->get_calcium() != MaxCaco3 && (!occupant || survivors.contains(occupant.get())))
                candidates.push_back(position);
        }
    }
    if (candidates.size() < pops_.size()) {
        logger_->warning("NewGen skipped relocation: too few free cells for surviving Pop");
        ++generation_count_;
        return;
    }
    std::mt19937 rng(std::random_device{}());
    std::shuffle(candidates.begin(), candidates.end(), rng);
    const auto index = [width](PositionT p) { return static_cast<std::size_t>(p.y) * width + p.x; };
    std::vector<unsigned char> excluded(static_cast<std::size_t>(width) * height, 0);
    std::vector<unsigned char> selected(excluded.size(), 0);
    std::vector<PositionT> destinations;
    destinations.reserve(pops_.size());
    for (const auto position : candidates) {
        if (destinations.size() == pops_.size())
            break;
        if (excluded[index(position)])
            continue;
        destinations.push_back(position);
        selected[index(position)] = 1;
        for (int dy = -2; dy <= 2; ++dy)
            for (int dx = -2; dx <= 2; ++dx) {
                const PositionT nearby{position.x + dx, position.y + dy};
                if (nearby.x >= 0 && nearby.y >= 0 && nearby.x < width && nearby.y < height)
                    excluded[index(nearby)] = 1;
            }
    }
    for (const auto position : candidates) {
        if (destinations.size() == pops_.size())
            break;
        if (!selected[index(position)]) {
            destinations.push_back(position);
            selected[index(position)] = 1;
        }
    }
    // No simulation phase runs concurrently with NewGen. Vacate before placing so
    // two survivors can exchange cells without creating a false occupancy failure.
    for (const auto& pop : pops_)
        if (world_->get_cell(pop->get_position())->get_occupant().lock().get() != pop.get())
            throw std::runtime_error("NewGen found a survivor without its occupied cell");
    for (const auto& pop : pops_)
        if (!world_->remove_entity(pop))
            throw std::runtime_error("NewGen lost a survivor occupancy before respawn");
    for (std::size_t i = 0; i < pops_.size(); ++i) {
        if (!pops_[i]->try_spawn(destinations[i]))
            throw std::runtime_error("NewGen could not respawn a survivor in a selected free cell");
        pops_[i]->reset_after_newgen_relocation();
    }

    ++generation_count_;

    logger_->info("NewGen applied. generation=" + std::to_string(generation_count_) +
                  " survivors=" + std::to_string(pops_.size()) + " dead_total=" + std::to_string(dead_count_));
}

void PopsManager::reset_population_state() {
    repro_next_id_.clear();
    repro_next_index_ = 0;
    for (auto& pop : pops_) {
        if (pop) {
            world_->remove_entity(pop);
        }
    }
    pops_.clear();
    recovery_bank_.clear();
    sense_bucket_.clear();
    think_bucket_.clear();
    act_bucket_.clear();
    clear_reproduction_backlog();
    dead_count_ = 0;
    total_births_ = 0;
    total_deaths_ = 0;
    forced_respawn_active_ = false;
    generation_count_ = 0;
    cycle_counter_ = 0;
    repro_dropped_total_ = 0;
    repro_eligible_agent_ticks_ = repro_cap_blocked_agent_ticks_ = repro_cap_blocked_ticks_ = 0;
    repro_attempts_ = repro_no_space_ = repro_births_ = repro_dispersed_births_ = 0;
    repro_backlog_peak_ = 0;
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
        s.lifetime_distance = pop->get_lifetime_distance();
        s.energy = pop->get_energy();
        s.pos = pop->get_position();
        s.mitochondrions = pop->get_phy().mitochondrions;
        s.chloroplasts = pop->get_phy().chloroplasts;
        s.sensitiveness = pop->get_phy().sensitiveness;
        s.adipose_stock_max = pop->get_phy().adipose_stock_max;
        s.body_temperature = pop->get_body_temperature();
        s.opt_temperature = pop->get_opt_temperature();
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
        s.total_movement_energy_loss = pop->get_total_movement_energy_loss();
        s.total_metabolism_energy_loss = pop->get_total_metabolism_energy_loss();
        s.total_reproduction_energy_loss = pop->get_total_reproduction_energy_loss();
        s.total_respiration_energy_gain = pop->get_total_respiration_energy_gain();
        s.total_thermoregolation_energy_loss = pop->get_total_thermoregolation_energy_loss();
        snaps.push_back(s);
    }
    return snaps;
}
nlohmann::json PopsManager::telemetry(bool detailed) const {
    auto result = nlohmann::json::array();
    for (const auto& pop : pops_)
        if (pop->is_alive())
            result.push_back(pop->telemetry(detailed));
    return result;
}
nlohmann::json PopsManager::inspect(const std::string& id) const {
    for (const auto& pop : pops_)
        if (pop->is_alive() && pop->get_pop_id() == id) {
            auto result = pop->telemetry(true);
            result["brain"] = pop->brain_snapshot();
            return result;
        }
    throw std::runtime_error("Agent is no longer alive");
}

void PopsManager::reset_learning_credit() {
    for (auto& p : pops_) p->reset_learning_credit();
}

std::string PopsManager::pop_at(PositionT position) const {
    for (const auto& p : pops_) if (p->is_alive() && p->get_position().x == position.x && p->get_position().y == position.y) return p->get_pop_id();
    return {};
}
nlohmann::json PopsManager::sandbox_spawn(PositionT position) {
    if (pops_.size() >= config_->get_max_population_allowed()) throw std::runtime_error("Population cap reached");
    if (!world_->is_free(position)) throw std::runtime_error("Cell unavailable");
    auto p = std::make_shared<Pop>(world_, logger_, config_);
    p->set_birth_provenance("", true);
    if (!p->try_spawn(position)) throw std::runtime_error("Cell unavailable");
    pops_.push_back(p);
    sense_bucket_.push_back(p);
    ++total_births_;
    auto event = p->telemetry(true);
    event["sandbox_spawn"] = true;
    if (stats_) stats_->record_event({LifeEvent::Birth, event});
    return event;
}
bool PopsManager::sandbox_remove(PositionT position) {
    auto it = std::find_if(pops_.begin(), pops_.end(), [&](const auto& p) {
        const auto q = p->get_position();
        return p->is_alive() && q.x == position.x && q.y == position.y;
    });
    if (it == pops_.end()) return false;
    auto p = *it;
    p->die();
    remember_for_recovery(*p);
    if (stats_) {
        auto event = p->telemetry(false);
        event["sandbox_remove"] = true;
        stats_->record_event({LifeEvent::Death, std::move(event)});
    }
    world_->remove_entity(p);
    ++dead_count_;
    ++total_deaths_;
    auto same = [&](const auto& q) { return q == p; };
    std::erase_if(sense_bucket_, same);
    std::erase_if(think_bucket_, same);
    std::erase_if(act_bucket_, same);
    pops_.erase(it);
    return true;
}
void PopsManager::sandbox_weight(const nlohmann::json& args) {
    for (auto& p : pops_) if (p->is_alive() && p->get_pop_id() == args.at("pop_id").get<std::string>()) {
        p->edit_weight(args.at("from_layer"), args.at("to_layer"), args.at("col"), args.at("row"), args.at("value"));
        return;
    }
    throw std::runtime_error("Agent no longer alive");
}

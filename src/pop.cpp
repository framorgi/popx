#include "pop.h"

#include "brain.h"
#include "color.h"
#include "neuron.h"
#include "random_utility.h"
#include "render_state.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>

namespace {
std::atomic<uint64_t> s_pop_counter{0};
}
uint64_t Pop::next_id() {
    return s_pop_counter.load();
}
void Pop::reserve_ids(uint64_t next) {
    s_pop_counter.store(std::max(next, s_pop_counter.load()));
}
constexpr float action_attempt_cost = 0.01f;
constexpr float basal_metabolism_cost = 0.02f;
constexpr float thermoregulation_energy_cost = 0.05f;
constexpr float thermoregulation_heat_gain = 0.5f;

Pop::Pop(std::weak_ptr<IWorld> world, std::shared_ptr<ILogger> logger, std::shared_ptr<IConfig> config)
    : genome_(Genome(RandomUtility().rnd_int(static_cast<int>(config->get_genome_min_length()),
                                             static_cast<int>(config->get_genome_max_length())),
                     Genome::encoding_from_config(*config))),
      world_(std::move(world)), logger_(std::move(logger)), config_(std::move(config)),
      brain_(std::make_unique<Brain>(config_)), pos_{0, 0}, age_(0), energy_(100.0f), last_direction_{0, 0},
      random_util_(std::make_shared<RandomUtility>()) {
    pop_id_ = "pop_" + std::to_string(s_pop_counter++);
    init();
}

Pop::Pop(std::weak_ptr<IWorld> world, std::shared_ptr<ILogger> logger, std::shared_ptr<IConfig> config, Genome genome)
    : genome_(std::move(genome)), world_(std::move(world)), logger_(std::move(logger)), config_(std::move(config)),
      brain_(std::make_unique<Brain>(config_)), pos_{0, 0}, age_(0), energy_(100.0f), last_direction_{0, 0},
      random_util_(std::make_shared<RandomUtility>()) {
    pop_id_ = "pop_" + std::to_string(s_pop_counter++);
    init();
}

Pop::Pop(std::weak_ptr<IWorld> world, std::shared_ptr<ILogger> logger, std::shared_ptr<IConfig> config, Genome genome,
         PhyT phy)
    : genome_(std::move(genome)), phy_(phy), world_(std::move(world)), logger_(std::move(logger)),
      config_(std::move(config)), brain_(std::make_unique<Brain>(config_)), pos_{0, 0}, age_(0), energy_(100.0f),
      last_direction_{0, 0}, random_util_(std::make_shared<RandomUtility>()) {
    pop_id_ = "pop_" + std::to_string(s_pop_counter++);
    inherit_phy_ = true;
    init();
}
Pop::~Pop() = default;

void Pop::init() {
    const bool inherit_phy = inherit_phy_;
    alive_ = true;
    current_state_ = State::SENSE;
    next_phase_ = State::SENSE;
    first_run_ = true;
    last_action_ = -1;
    // Uniform choice among the eight neighbouring directions; never start with (0, 0).
    constexpr PositionT initial_directions[] = {{0, -1}, {1, -1}, {1, 0}, {1, 1}, {0, 1}, {-1, 1}, {-1, 0}, {-1, -1}};
    last_direction_ = initial_directions[random_util_->rnd_int(0, 7)];
    lazyness_ = random_util_->rnd_float(0.0f, 1.0f);
    glucose_ = 100;
    water_ = 100;
    o2_ = 20;
    co2_ = 0;
    lipids_ = 20;
    metabolism_heat_ = 0.0f;
    thermoregulation_heat_ = 0.0f;
    temperature_ = config_->get_phy_init_temperature();
    last_reward_ = 0.0f;
    last_move_failed_ = false;
    scorer_.reset();
    total_movement_energy_loss_ = 0.0f;
    total_other_action_energy_loss_ = 0.0f;
    total_metabolism_energy_loss_ = 0.0f;
    total_reproduction_energy_loss_ = 0.0f;
    total_respiration_energy_gain_ = 0.0f;
    total_thermoregolation_energy_loss_ = 0.0f;
    respiration_credit_ = 0.0;
    total_respiration_heat_ = 0.0;
    total_respiration_count_ = 0;
    total_photosynthesis_count_ = 0;
    total_energy_cap_loss_ = 0.0;
    total_energy_roundoff_ = 0.0;
    lifetime_distance_ = 0.0;
    if (!inherit_phy) {
        phy_.mitochondrions = static_cast<unsigned>(
            random_util_->rnd_int(1, static_cast<int>(config_->get_phy_init_mitochondrions_max())));
        phy_.chloroplasts =
            static_cast<unsigned>(random_util_->rnd_int(0, static_cast<int>(config_->get_phy_init_chloroplasts_max())));
        phy_.sensitiveness = static_cast<unsigned>(
            random_util_->rnd_int(1, static_cast<int>(config_->get_phy_init_sensitiveness_max())));
        phy_.adipose_stock_max = config_->get_phy_adipose_stock_max();
    }
    probe_dist_ = static_cast<int>(std::clamp(phy_.sensitiveness, 1u, 12u));
    lipids_ = std::min(lipids_, phy_.adipose_stock_max);
    sensor_values_.assign(brain_->get_size_s(), 0.0f);
    brain_->wire(genome_);
    action_requests_.fill(0);
    acquired_.fill(0);
    released_.fill(0);
    movement_attempts_ = movement_successes_ = 0;
    movement_failures_.fill(0);
}

void Pop::die() {
    if (!alive_)
        return;

    auto world = world_.lock();
    if (world) {
        auto cell = world->get_cell(pos_);
        {
            const auto accepted = cell->give_glucose(glucose_);
            glucose_ -= accepted;
            released_[0] += accepted;
        }
        {
            const auto accepted = cell->give_water(water_);
            water_ -= accepted;
            released_[1] += accepted;
        }
        // Share death overflow locally, respecting finite capacity and world boundaries.
        // Rotate the starting neighbour so indivisible remainders have no fixed direction.
        std::array<std::shared_ptr<ICell>, 8> neighbours;
        std::size_t count = 0;
        if (water_ > 0) {
            for (int dy = -1; dy <= 1; ++dy) {
                for (int dx = -1; dx <= 1; ++dx) {
                    const PositionT pos{pos_.x + dx, pos_.y + dy};
                    if ((dx == 0 && dy == 0) || pos.x < 0 || pos.y < 0 || pos.x >= world->get_width() ||
                        pos.y >= world->get_height())
                        continue;
                    auto neighbour = world->get_cell(pos);
                    if (neighbour->get_water() < MaxH2o)
                        neighbours[count++] = std::move(neighbour);
                }
            }
        }
        if (count > 1)
            std::rotate(neighbours.begin(), neighbours.begin() + random_util_->rnd_int(0, static_cast<int>(count) - 1),
                        neighbours.begin() + count);
        while (water_ > 0 && count > 0) {
            const unsigned share = std::max(1u, water_ / static_cast<unsigned>(count));
            unsigned distributed = 0;
            std::size_t available = 0;
            for (std::size_t i = 0; i < count; ++i) {
                const auto accepted = neighbours[i]->give_water(std::min(share, water_));
                water_ -= accepted;
                released_[1] += accepted;
                distributed += accepted;
                if (neighbours[i]->get_water() < MaxH2o)
                    neighbours[available++] = neighbours[i];
            }
            count = available;
            if (distributed == 0)
                break;
        }
        // Any remainder stays in the dead agent for the explicit overflow sink ledger.
        {
            const auto accepted = cell->give_o2(o2_);
            o2_ -= accepted;
            released_[4] += accepted;
        }
        {
            const auto accepted = cell->give_co2(co2_);
            co2_ -= accepted;
            released_[3] += accepted;
        }
        {
            const auto accepted = cell->give_calcium(calcium_);
            calcium_ -= accepted;
            released_[2] += accepted;
        }
        {
            const auto accepted = cell->give_lipids(lipids_);
            lipids_ -= accepted;
            released_[5] += accepted;
        }
    }
    alive_ = false;
}

bool Pop::try_spawn(PositionT p) {
    auto world = world_.lock();
    if (!world) {
        return false; // World no longer exists
    }
    const PositionT previous = pos_;
    pos_ = p;
    if (world->add_entity(shared_from_this()))
        return true;
    pos_ = previous; // A failed placement must not lose the last valid position.
    return false;
}

float Pop::calculate_reward() const {
    return Scorer::reward({age_, offspring_count_, phy_.mitochondrions, energy_, previous_energy_, temperature_},
                          *config_);
}

void Pop::learn(float reward) {
    // N.B. The first-run guard is necessary to prevent the Pop from applying a learning update before
    //  run a brain sweep, which could lead to an activations_ access in hebbian_update() before it's properly
    //  initialized by feed_forward().
    if (first_run_) {
        return; // Skip learning on the first run to allow initial sensing and acting
    }
    logger_->debug("Applying learning update for Pop at position (" + std::to_string(pos_.x) + ", " +
                   std::to_string(pos_.y) + ") with reward " + std::to_string(reward) + ".");
    brain_->hebbian_update(reward);
}

void Pop::update() {
    age_++;
    update_physiology();
    if (energy_ <= 0 || age_ > MaxAge || temperature_ <= 10.0 || temperature_ >= 75.0) {
        if (energy_ <= 0) {
            death_cause_ = DeathCause::EnergyDepletion;
        } else if (age_ > MaxAge) {
            death_cause_ = DeathCause::OldAge;
        } else {
            death_cause_ = DeathCause::TemperatureOutOfRange;
        }
        die();
    } else {
        const float signal = scorer_.observe(
            {age_, offspring_count_, phy_.mitochondrions, energy_, previous_energy_, temperature_}, *config_);
        last_reward_ = scorer_.raw_reward();
        learn(signal);
    }
}

void Pop::despawn() {
    alive_ = false;
    auto world = world_.lock();
    if (world) {
        world->remove_entity(shared_from_this());
    }
}

// be sure return sensor values in range [0,1]
void Pop::sense() {
    if (next_phase_ != State::SENSE)
        return;
    current_state_ = State::SENSE;

    // lock the world weak pointer to access the world safely
    auto world = world_.lock();
    if (!world)
        return;

    const int W = world->get_width();
    const int H = world->get_height();

    const unsigned sensor_number = brain_->get_size_s();

    sensor_values_.assign(sensor_number, 0.0f);
    const auto connected = brain_->get_connected_sensors();

    // ---- helpers -------------------------------------------------------
    // Average a metric over probe_dist_ cells stepping (dx,dy) from pos_.
    auto scan = [&](int dx, int dy, auto metric) -> float {
        float sum = 0.0f;
        int cnt = 0;
        int cx = pos_.x;
        int cy = pos_.y;
        for (int s = 1; s <= probe_dist_; ++s) {
            cx += dx;
            cy += dy;
            if (cx < 0 || cx >= W || cy < 0 || cy >= H)
                break;
            auto cell = world->get_cell(PositionT{cx, cy});
            if (!cell)
                break;
            sum += metric(cell);
            ++cnt;
        }
        return cnt > 0 ? sum / static_cast<float>(cnt) : 0.0f;
    };

    auto norm_temp = [](const std::shared_ptr<ICell>& c) -> float {
        return std::clamp(static_cast<float>(c->get_temperature()) / 50.0f, 0.0f, 1.0f);
    };

    auto occupancy = [](const std::shared_ptr<ICell>& c) -> float { return c->get_occupant().expired() ? 0.0f : 1.0f; };

    auto fero_at = [&](PositionT p, FeromoneT t) -> float {
        return static_cast<float>(world->get_feromone_magnitude(p, t));
    };

    auto fero_scan = [&](int dx, int dy, FeromoneT t) -> float {
        return scan(dx, dy, [&](const std::shared_ptr<ICell>& c) -> float {
            auto map = c->get_feromone_map();
            auto it = map.find(t);
            return it != map.end() ? static_cast<float>(it->second) / static_cast<float>(MaxFeromones) : 0.0f;
        });
    };

    // gradient: clamp((dir - here) * 0.5 + 0.5, 0, 1) → [0,1] where 1=rising
    auto gradient = [](float here, float dir) -> float { return std::clamp((dir - here) * 0.5f + 0.5f, 0.0f, 1.0f); };

    std::array<float, 4> signal_density{};
    bool signal_density_ready = false;
    auto surrounding_signal_density = [&]() {
        if (signal_density_ready)
            return;
        signal_density_ready = true;
        unsigned cells = 0;
        for (int dy = -probe_dist_; dy <= probe_dist_; ++dy) {
            for (int dx = -probe_dist_; dx <= probe_dist_; ++dx) {
                if ((dx == 0 && dy == 0) || pos_.x + dx < 0 || pos_.x + dx >= W ||
                    pos_.y + dy < 0 || pos_.y + dy >= H)
                    continue;
                const auto cell = world->get_cell({pos_.x + dx, pos_.y + dy});
                if (!cell)
                    continue;
                const auto signals = cell->get_feromone_map();
                for (unsigned type = 0; type < signal_density.size(); ++type) {
                    const auto found = signals.find(static_cast<FeromoneT>(type));
                    if (found != signals.end())
                        signal_density[type] += found->second / static_cast<float>(MaxFeromones);
                }
                ++cells;
            }
        }
        if (cells)
            for (auto& density : signal_density)
                density /= static_cast<float>(cells);
    };

    // Distance to boundaries, normalized to [0,1] with 0.5=center and 1=at boundary.
    const float bx = (W > 1) ? static_cast<float>(std::min(pos_.x, W - 1 - pos_.x)) / (W * 0.5f) : 0.0f;
    const float by = (H > 1) ? static_cast<float>(std::min(pos_.y, H - 1 - pos_.y)) / (H * 0.5f) : 0.0f;

    // ----start  per-sensor evaluation -----------------------------------------

    for (unsigned i = 0; i < sensor_number; ++i) {
        if (i < connected.size() && !connected[i])
            continue;
        float val = 0.0f;
        switch (static_cast<Sensor>(i)) {
            case Sensor::LOC_X:
                val = (W > 1) ? static_cast<float>(pos_.x) / (W - 1) : 0.0f;
                break;
            case Sensor::LOC_Y:
                val = (H > 1) ? static_cast<float>(pos_.y) / (H - 1) : 0.0f;
                break;
            case Sensor::BOUNDARY_DIST_X:
                val = bx;
                break;
            case Sensor::BOUNDARY_DIST_Y:
                val = by;
                break;
            case Sensor::BOUNDARY_DIST:
                val = std::min(bx, by);
                break;
            case Sensor::GENETIC_SIM_FWD: {
                // One adjacent cell in the heading direction, independent of movement stride.
                const int dx = (last_direction_.x > 0) - (last_direction_.x < 0);
                const int dy = (last_direction_.y > 0) - (last_direction_.y < 0);
                if (dx || dy) {
                    const auto cell = world->get_cell({pos_.x + dx, pos_.y + dy});
                    if (cell) {
                        const auto neighbour = std::dynamic_pointer_cast<Pop>(cell->get_occupant().lock());
                        if (neighbour && neighbour.get() != this && neighbour->alive_)
                            val = static_cast<float>(genome_.similarity(neighbour->genome_));
                    }
                }
                break;
            }
            case Sensor::LAST_MOVE_DIR_X:
                val = last_direction_.x * 0.5f + 0.5f;
                break;
            case Sensor::LAST_MOVE_DIR_Y:
                val = last_direction_.y * 0.5f + 0.5f;
                break;
            case Sensor::POPULATION_DENSITY_N:
                val = scan(0, -1, occupancy);
                break;
            case Sensor::POPULATION_DENSITY_W:
                val = scan(-1, 0, occupancy);
                break;
            case Sensor::POPULATION_DENSITY_E:
                val = scan(1, 0, occupancy);
                break;
            case Sensor::POPULATION_DENSITY_S:
                val = scan(0, 1, occupancy);
                break;
            case Sensor::TEMP_AVG_N:
                val = scan(0, -1, norm_temp);
                break;
            case Sensor::TEMP_AVG_W:
                val = scan(-1, 0, norm_temp);
                break;
            case Sensor::TEMP_AVG_E:
                val = scan(1, 0, norm_temp);
                break;
            case Sensor::TEMP_AVG_S:
                val = scan(0, 1, norm_temp);
                break;
            case Sensor::TEMP_DRV_N: {
                auto here = world->get_cell(pos_);
                val = here ? gradient(norm_temp(here), scan(0, -1, norm_temp)) : 0.0f;
                break;
            }
            case Sensor::TEMP_DRV_W: {
                auto here = world->get_cell(pos_);
                val = here ? gradient(norm_temp(here), scan(-1, 0, norm_temp)) : 0.0f;
                break;
            }
            case Sensor::TEMP_DRV_E: {
                auto here = world->get_cell(pos_);
                val = here ? gradient(norm_temp(here), scan(1, 0, norm_temp)) : 0.0f;
                break;
            }
            case Sensor::TEMP_DRV_S: {
                auto here = world->get_cell(pos_);
                val = here ? gradient(norm_temp(here), scan(0, 1, norm_temp)) : 0.0f;
                break;
            }
            case Sensor::SENSE_SIGNAL_FOOD:
                val = fero_at(pos_, FeromoneT::FOOD_FEROMONE);
                break;
            case Sensor::SENSE_SIGNAL_DANGER:
                val = fero_at(pos_, FeromoneT::DANGER_FEROMONE);
                break;
            case Sensor::SENSE_SIGNAL_MATE:
                val = fero_at(pos_, FeromoneT::MATE_FEROMONE);
                break;
            case Sensor::SENSE_SIGNAL_HOME:
                val = fero_at(pos_, FeromoneT::HOME_FEROMONE);
                break;
            case Sensor::SENSE_SIGNAL_DENSITY_FOOD:
            case Sensor::SENSE_SIGNAL_DENSITY_DANGER:
            case Sensor::SENSE_SIGNAL_DENSITY_MATE:
            case Sensor::SENSE_SIGNAL_DENSITY_HOME:
                surrounding_signal_density();
                val = signal_density[i - static_cast<unsigned>(Sensor::SENSE_SIGNAL_DENSITY_FOOD)];
                break;
            case Sensor::SENSE_DANGER_DRV_N:
            case Sensor::SENSE_DANGER_DRV_W:
            case Sensor::SENSE_DANGER_DRV_E:
            case Sensor::SENSE_DANGER_DRV_S:
            case Sensor::SENSE_MATE_DRV_N:
            case Sensor::SENSE_MATE_DRV_W:
            case Sensor::SENSE_MATE_DRV_E:
            case Sensor::SENSE_MATE_DRV_S:
            case Sensor::SENSE_HOME_DRV_N:
            case Sensor::SENSE_HOME_DRV_W:
            case Sensor::SENSE_HOME_DRV_E:
            case Sensor::SENSE_HOME_DRV_S: {
                const unsigned offset = i - static_cast<unsigned>(Sensor::SENSE_DANGER_DRV_N);
                constexpr FeromoneT types[] = {DANGER_FEROMONE, MATE_FEROMONE, HOME_FEROMONE};
                constexpr int dx[] = {0, -1, 1, 0};
                constexpr int dy[] = {-1, 0, 0, 1};
                val = gradient(fero_at(pos_, types[offset / 4]),
                               fero_scan(dx[offset % 4], dy[offset % 4], types[offset / 4]));
                break;
            }
            case Sensor::GLUCOSE_DENSITY_N:
                val = scan(0, -1, [](const std::shared_ptr<ICell>& c) {
                    return static_cast<float>(c->get_glucose()) / MaxC6h12o6;
                });
                break;
            case Sensor::GLUCOSE_DENSITY_W:
                val = scan(-1, 0, [](const std::shared_ptr<ICell>& c) {
                    return static_cast<float>(c->get_glucose()) / MaxC6h12o6;
                });
                break;
            case Sensor::GLUCOSE_DENSITY_E:
                val = scan(1, 0, [](const std::shared_ptr<ICell>& c) {
                    return static_cast<float>(c->get_glucose()) / MaxC6h12o6;
                });
                break;
            case Sensor::GLUCOSE_DENSITY_S:
                val = scan(0, 1, [](const std::shared_ptr<ICell>& c) {
                    return static_cast<float>(c->get_glucose()) / MaxC6h12o6;
                });
                break;
            case Sensor::OSC1:
                val = std::sin(static_cast<float>(age_) * 2.0f * 3.14159265f / osc_period_) * 0.5f + 0.5f;
                break;
            case Sensor::AGE: {
                const float spg = static_cast<float>(config_->get_steps_per_generation());
                val = (spg > 0) ? std::clamp(static_cast<float>(age_) / spg, 0.0f, 1.0f) : 0.0f;
                break;
            }
            case Sensor::TEMP: {
                auto here = world->get_cell(pos_);
                val = here ? norm_temp(here) : 0.0f;
                break;
            }
            case Sensor::RANDOM:
                val = static_cast<float>(random_util_->rnd_int(0, 10000)) / 10000.0f;
                break;
            case Sensor::LAST_MOVE_FAILED:
                // Old 49-input checkpoints retain the historical zero-valued slot.
                val = sensor_number >= static_cast<unsigned>(Sensor::SENSOR_COUNT) && last_move_failed_ ? 1.0f : 0.0f;
                break;
            case Sensor::MOVE_OPEN_N:
            case Sensor::MOVE_OPEN_NE:
            case Sensor::MOVE_OPEN_E:
            case Sensor::MOVE_OPEN_SE:
            case Sensor::MOVE_OPEN_S:
            case Sensor::MOVE_OPEN_SW:
            case Sensor::MOVE_OPEN_W:
            case Sensor::MOVE_OPEN_NW: {
                constexpr int dx[] = {0, 1, 1, 1, 0, -1, -1, -1};
                constexpr int dy[] = {-1, -1, 0, 1, 1, 1, 0, -1};
                const unsigned direction = i - static_cast<unsigned>(Sensor::MOVE_OPEN_N);
                const PositionT target{pos_.x + static_cast<int>(dx[direction] * responsiveness_),
                                       pos_.y + static_cast<int>(dy[direction] * responsiveness_)};
                // The read-only predicate mirrors try_move and GridWorld::move_entity.
                val = (target.x != pos_.x || target.y != pos_.y) && target.x >= 0 && target.y >= 0 &&
                              target.x < W && target.y < H && world->is_free(target) &&
                              world->get_elevation(target) - world->get_elevation(pos_) <
                                  MaxClimbableSlope * (1.0f - lazyness_)
                          ? 1.0f
                          : 0.0f;
                break;
            }
            case Sensor::SENSOR_COUNT:
                break;
        }
        sensor_values_[i] = val;
    }
    next_phase_ = State::THINK;
}

void Pop::think() {
    if (next_phase_ != State::THINK)
        return;
    current_state_ = State::THINK;
    // Consume the sensors from the preceding sense phase.
    last_action_ = brain_->feed_forward(sensor_values_);
    next_phase_ = State::ACT;
}

void Pop::serialize_brain(unsigned generation) const {
    if (!config_->get_brain_serialization_enabled()) {
        return;
    }
    brain_->serialize(pop_id_, generation);
}

void Pop::act() {
    if (next_phase_ != State::ACT)
        return;
    current_state_ = State::ACT;
    next_phase_ = State::SENSE;

    if (last_action_ < 0 || last_action_ > static_cast<int>(Action::KILL_FORWARD)) {
        return;
    }

    brain_->record_action(last_action_);
    first_run_ = false;
    ++action_requests_[last_action_];
    PositionT p = pos_;
    auto world = world_.lock();
    const auto action = static_cast<Action>(last_action_);
    // Charge the attempt, including blocked moves and ineffective transfers.
    // BURN_CALORIES already pays its larger thermoregulation cost; waiting is free.
    if (action != Action::DO_NOTHING && action != Action::BURN_CALORIES) {
        energy_cost_ += action_attempt_cost;
        if (last_action_ <= static_cast<int>(Action::MOVE_SOUTH))
            total_movement_energy_loss_ += action_attempt_cost;
        else
            total_other_action_energy_loss_ += action_attempt_cost;
    }

    switch (action) {
        // ── movement ────────────────────────────────────────────────────────
        case Action::MOVE_FORWARD:
            p.x += last_direction_.x * (responsiveness_);
            p.y += last_direction_.y * (responsiveness_);
            try_move(p);
            break;
        case Action::MOVE_LEFT:
            p.x -= last_direction_.y * responsiveness_;
            p.y += last_direction_.x * responsiveness_;
            try_move(p);
            break;
        case Action::MOVE_RIGHT:
            p.x += last_direction_.y * responsiveness_;
            p.y -= last_direction_.x * responsiveness_;
            try_move(p);
            break;
        case Action::MOVE_RANDOM:
            p.x += random_util_->rnd_int(-1, 1) * responsiveness_;
            p.y += random_util_->rnd_int(-1, 1) * responsiveness_;
            try_move(p);
            break;
        case Action::MOVE_EAST:
            p.x += 1 * responsiveness_;
            try_move(p);
            break;
        case Action::MOVE_WEST:
            p.x -= 1 * responsiveness_;
            try_move(p);
            break;
        case Action::MOVE_NORTH:
            p.y -= 1 * responsiveness_;
            try_move(p);
            break;
        case Action::MOVE_SOUTH:
            p.y += 1 * responsiveness_;
            try_move(p);
            break;
        // ── feromone signalling ─────────────────────────────────────────────
        case Action::EMIT_SIGNAL_FOOD:
            emit_feromone(FeromoneT::FOOD_FEROMONE, random_util_->rnd_int(70, 100));
            break;
        case Action::EMIT_SIGNAL_DANGER:
            emit_feromone(FeromoneT::DANGER_FEROMONE, random_util_->rnd_int(70, 100));
            break;
        case Action::EMIT_SIGNAL_MATE:
            emit_feromone(FeromoneT::MATE_FEROMONE, random_util_->rnd_int(70, 100));
            break;
        case Action::EMIT_SIGNAL_HOME:
            emit_feromone(FeromoneT::HOME_FEROMONE, random_util_->rnd_int(70, 100));
            break;
        // ── resource acquisition ────────────────────────────────────────────
        case Action::GET_GLUCOSE:
            if (world) {
                {
                    const auto accepted = world->get_cell(pos_)->take_glucose(10);
                    glucose_ += accepted;
                    acquired_[0] += accepted;
                }
            }
            break;
        case Action::LEAVE_GLUCOSE:
            if (world) {
                {
                    const auto accepted = world->get_cell(pos_)->give_glucose(std::min(10u, glucose_));
                    glucose_ -= accepted;
                    released_[0] += accepted;
                }
            }
            break;
        case Action::GET_H2O:
            if (world) {
                {
                    const auto accepted = world->get_cell(pos_)->take_water(10);
                    water_ += accepted;
                    acquired_[1] += accepted;
                }
            }
            break;

        case Action::LEAVE_H2O:
            if (world) {
                {
                    const auto accepted = world->get_cell(pos_)->give_water(std::min(10u, water_));
                    water_ -= accepted;
                    released_[1] += accepted;
                }
            }
            break;
        case Action::GET_CALCIUM:
            if (world) {
                {
                    const auto accepted = world->get_cell(pos_)->take_calcium(10);
                    calcium_ += accepted;
                    acquired_[2] += accepted;
                }
            }
            break;
        case Action::LEAVE_CALCIUM:
            if (world) {
                {
                    const auto accepted = world->get_cell(pos_)->give_calcium(std::min(10u, calcium_));
                    calcium_ -= accepted;
                    released_[2] += accepted;
                }
            }
            break;
        case Action::BURN_CALORIES:
            run_thermoregulation();
            break;
        // ── internal modulation ──────────────────────────────────────────────
        case Action::SET_OSCILLATOR_PERIOD: {
            const float spg = static_cast<float>(config_->get_steps_per_generation());
            osc_period_ = std::clamp(osc_period_ * 1.1f, 10.0f, spg > 0 ? spg : 10.0f);
            break;
        }
        case Action::SET_RESPONSIVENESS:
            responsiveness_ = std::clamp(responsiveness_ + 0.001f, 1.0f, 2.0f);
            probe_dist_ = std::min(probe_dist_ + 1,
                                   static_cast<int>(std::min(config_->get_phy_init_sensitiveness_max(), 12u)));
            break;
        // Waiting has no action cost. Physiology and ordinary life processes still run.
        case Action::DO_NOTHING:
        case Action::NUM_ACTIONS:
            break;
        case Action::KILL_FORWARD: {
            if (!world)
                break;
            PositionT fwd{pos_.x + last_direction_.x, pos_.y + last_direction_.y};
            auto cell = world->get_cell(fwd);
            if (!cell)
                break;
            auto occ = cell->get_occupant().lock();
            if (!occ)
                break;
            auto agent = std::dynamic_pointer_cast<Pop>(occ);
            if (agent)
                agent->die();
            break;
        }
    }
}

bool Pop::try_move(PositionT p) {
    ++movement_attempts_;
    last_move_failed_ = true;
    auto world = world_.lock();
    if (!world) {
        ++movement_failures_[5];
        logger_->error("Failed to move: World no longer exists.");
        return false; // World no longer exists
    }
    if (p.x == pos_.x && p.y == pos_.y) {
        ++movement_failures_[4];
        return false;
    }
    if (p.x < 0 || p.y < 0 || p.x >= world->get_width() || p.y >= world->get_height()) {
        ++movement_failures_[3];
        return false;
    }
    auto stiff = world->get_elevation(p) - world->get_elevation(pos_);
    if (stiff < MaxClimbableSlope * (1.0f - lazyness_)) {
        // cache last position for direction calculation
        PositionT old_pos = pos_;
        //  movement logic
        if (world->move_entity(shared_from_this(), p)) {
            pos_ = p;
            const double dx = static_cast<double>(pos_.x - old_pos.x);
            const double dy = static_cast<double>(pos_.y - old_pos.y);
            lifetime_distance_ += std::sqrt(dx * dx + dy * dy);
            update_last_direction(pos_, old_pos);
            ++movement_successes_;
            last_move_failed_ = false;
            return true;
        }
        const auto cell = world->get_cell(p);
        if (cell && !cell->get_occupant().expired())
            ++movement_failures_[0];
        else if (cell && cell->get_calcium() == MaxCaco3)
            ++movement_failures_[1];
        else
            ++movement_failures_[5];
        logger_->debug("Move to position (" + std::to_string(p.x) + ", " + std::to_string(p.y) + ") failed.");
    } else {
        ++movement_failures_[2];
    }
    return false;
}

PositionT Pop::get_position() const {
    return pos_;
}

bool Pop::is_alive() {
    return alive_;
}

Color Pop::get_genetic_color() const {
    return genome_.get_genetic_color();
}

RenderState Pop::get_render_state() const {
    Color c = genome_.get_genetic_color();
    float size = 1.0f;
    if (alive_) {
        if (current_state_ == State::SENSE || current_state_ == State::THINK) {
            // c = Color{0, 255, 0};
            size = 0.9f;
        }
    } else {
        c = Color{0, 0, 0};
    }
    RenderState state;
    state.position = Vec2{static_cast<float>(pos_.x), static_cast<float>(pos_.y)};
    state.color = c;   //  Black if dead
    state.size = size; //  size
    state.payload = PopVisualData{energy_ / 100.0f, age_ / 100.0f, phy_.chloroplasts > phy_.mitochondrions};
    state.shape = RenderShape::Circle; //  shape
    return state;
}

bool Pop::wants_to_reproduce() const {
    return alive_ && energy_ > 50.0f && age_ > 1000 && glucose_ > 20;
}

Genome Pop::heritable_genome() const {
    if (config_->get_hebbian_inheritance()) {
        if (auto lg = brain_->get_learned_genome()) {
            if (!lg->get_genes().empty()) {
                return *lg;
            }
        }
    }
    return genome_;
}

Genome Pop::make_offspring_genome() const {
    return heritable_genome().mutated(config_->get_point_mutation_rate(), Genome::encoding_from_config(*config_),
                                      config_->get_weight_mutation_rate(), config_->get_weight_mutation_step());
}

PhyT Pop::make_offspring_phy() const {
    PhyT offspring = phy_;
    const double rate = config_->get_point_mutation_rate();
    RandomUtility rand;
    auto mutate_unsigned = [&](unsigned val, unsigned min_val, unsigned max_val) -> unsigned {
        if (rand.rnd_double(0.0, 1.0) < rate) {
            int delta = rand.rnd_int(0, 1) == 0 ? -1 : 1;
            int result = static_cast<int>(val) + delta;
            return static_cast<unsigned>(std::clamp(result, static_cast<int>(min_val), static_cast<int>(max_val)));
        }
        return val;
    };
    offspring.mitochondrions = mutate_unsigned(offspring.mitochondrions, 1, config_->get_phy_init_mitochondrions_max());
    offspring.chloroplasts = mutate_unsigned(offspring.chloroplasts, 0, config_->get_phy_init_chloroplasts_max());
    offspring.sensitiveness = mutate_unsigned(offspring.sensitiveness, 1, config_->get_phy_init_sensitiveness_max());
    offspring.adipose_stock_max = mutate_unsigned(offspring.adipose_stock_max, 1, config_->get_phy_adipose_stock_max());
    return offspring;
}

void Pop::donate_resources(unsigned& out_glucose, unsigned& out_water, unsigned& out_calcium, unsigned& out_co2,
                           unsigned& out_o2, unsigned& out_lipids, float& out_energy) {
    out_glucose = glucose_ / 8;
    out_water = water_ / 8;
    out_calcium = calcium_ / 8;
    out_co2 = co2_ / 8;
    out_o2 = o2_ / 8;
    out_lipids = lipids_ / 8;
    out_energy = energy_ / 8.0f;
    o2_ -= out_o2;
    lipids_ -= out_lipids;
    change_energy(-out_energy);
    glucose_ -= out_glucose;
    water_ -= out_water;
    calcium_ -= out_calcium;
    co2_ -= out_co2;
}

void Pop::set_resources(unsigned glucose, unsigned water, unsigned calcium, unsigned co2, unsigned o2, unsigned lipids,
                        float energy) {
    glucose_ = glucose;
    water_ = water;
    calcium_ = calcium;
    co2_ = co2;
    o2_ = o2;
    lipids_ = lipids;
    energy_ = energy;
}

void Pop::update_last_direction(PositionT new_pos, PositionT old_pos) {
    last_direction_.x = new_pos.x - old_pos.x;
    last_direction_.y = new_pos.y - old_pos.y;
}

void Pop::emit_feromone(FeromoneT type, int intensity) {
    auto world = world_.lock();
    if (!world) {
        return; // World no longer exists
    }

    world->set_feromone(pos_, type, intensity);
}
[[nodiscard]] float Pop::get_feromone_strength(FeromoneT type, PositionT pos) const {
    auto world = world_.lock();
    if (!world) {
        return 0; // World no longer exists
    }
    float strength = world->get_feromone_strength(type, pos);
    return strength;
}

void Pop::update_physiology() {
    metabolism_heat_ = 0.0f;
    previous_energy_ = energy_;

    auto world = world_.lock();
    if (!world)
        return;

    // Auto-take resources from current cell
    {
        const auto accepted = world->get_cell(pos_)->take_glucose(5);
        glucose_ += accepted;
        acquired_[0] += accepted;
    }
    {
        const auto accepted = world->get_cell(pos_)->take_water(5);
        water_ += accepted;
        acquired_[1] += accepted;
    }
    {
        const auto accepted = world->get_cell(pos_)->take_calcium(2);
        calcium_ += accepted;
        acquired_[2] += accepted;
    }
    {
        const auto accepted = world->get_cell(pos_)->take_co2(5);
        co2_ += accepted;
        acquired_[3] += accepted;
    }
    if (lipids_ < phy_.adipose_stock_max) {
        const auto accepted = world->get_cell(pos_)->take_lipids(1);
        lipids_ += accepted;
        acquired_[5] += accepted;
    }
    {
        const auto accepted = world->get_cell(pos_)->take_o2(5);
        o2_ += accepted;
        acquired_[4] += accepted;
    }

    // Biological reactions
    run_chloroplasts();
    run_mitochondrions();
    update_temperature();
    thermoregulation_heat_ = 0.0f;

    // Metabolism cost
    change_energy(-basal_metabolism_cost);
    total_metabolism_energy_loss_ += basal_metabolism_cost;
    change_energy(-energy_cost_); // Additional cost from actions
                                  // reset energy cost for this cycle
    energy_cost_ = 0;
    // cap energy to MaxEnergy
    if (energy_ > MaxEnergy) {
        total_energy_cap_loss_ += static_cast<double>(energy_ - MaxEnergy);
        energy_ = MaxEnergy;
    }
}

void Pop::run_chloroplasts() {
    if (phy_.chloroplasts < config_->get_phy_min_chloroplasts() || co2_ == 0 || water_ == 0)
        return;

    auto world = world_.lock();
    if (!world)
        return;

    // Light condition: cell elevation must exceed threshold
    const double elev = world->get_elevation(pos_);
    if (elev < config_->get_phy_photo_min_elevation())
        return;

    // Probability scales linearly with chloroplast count
    const double prob = std::min(
        static_cast<double>(phy_.chloroplasts) / static_cast<double>(config_->get_phy_max_chloroplasts_ref()), 1.0);
    if (random_util_->rnd_double(0.0, 1.0) >= prob)
        return;

    // CO2 + H2O -> C6H12O6 + O2
    co2_--;
    water_--;
    glucose_++;
    ++total_photosynthesis_count_;
    // Keep any oxygen the cell cannot accept, instead of silently losing it.
    o2_ += 1u - world->get_cell(pos_)->give_o2(1);
}

void Pop::run_mitochondrions() {
    const double target = std::clamp(config_->get_phy_respiration_energy_target(), 0.0, static_cast<double>(MaxEnergy));
    const double band = config_->get_phy_respiration_demand_band();
    const double width = config_->get_phy_efficiency_half_range_c();
    const double optimum = config_->get_phy_opt_temperature();
    if (glucose_ == 0 || o2_ == 0 || phy_.mitochondrions == 0 || energy_ >= target || !std::isfinite(energy_) ||
        !std::isfinite(temperature_) || !std::isfinite(target) || !std::isfinite(band) || band <= 0.0 ||
        !std::isfinite(width) || width <= 0.0 || !std::isfinite(optimum)) {
        respiration_credit_ = 0.0;
        return;
    }

    // Mitochondria set capacity, not mandatory fuel consumption. The smooth thermal
    // response has no cold-start dead zone. Above the optimum a smooth inhibition
    // reaches zero at optimum + width, preventing fuel-driven overheating there.
    const double thermal_distance = (temperature_ - optimum) / width;
    const double thermal_yield = std::exp2(-thermal_distance * thermal_distance);
    const double hot = std::clamp(thermal_distance, 0.0, 1.0);
    const double hot_inhibition = 1.0 - hot * hot * (3.0 - 2.0 * hot);
    const double thermal_activity = thermal_yield * hot_inhibition;
    if (thermal_activity == 0.0) {
        respiration_credit_ = 0.0;
        return;
    }
    const double demand = std::clamp((target - energy_) / band, 0.0, 1.0);
    const double capacity = static_cast<double>(phy_.mitochondrions);
    respiration_credit_ = std::min(capacity, respiration_credit_ + capacity * demand * thermal_activity);
    const unsigned reactions =
        static_cast<unsigned>(std::min(std::floor(respiration_credit_), static_cast<double>(std::min(glucose_, o2_))));
    // Discard unfulfilled whole capacity when substrate-limited: no catch-up burst.
    respiration_credit_ -= std::floor(respiration_credit_);

    unsigned co2_produced = 0;
    for (unsigned i = 0; i < reactions && energy_ < target; ++i) {
        // Only completed reactions transfer matter and generate useful energy/heat.
        glucose_--;
        o2_--;
        const float energy_gain = static_cast<float>(config_->get_phy_energy_per_respiration() * thermal_yield);
        change_energy(energy_gain);
        total_respiration_energy_gain_ += energy_gain;
        const float heat = static_cast<float>(config_->get_phy_heat_per_respiration());
        metabolism_heat_ += heat;
        total_respiration_heat_ += heat;
        // Abstract 1:1 inverse of photosynthesis: glucose + O2 -> CO2 + water.
        co2_++;
        water_++;
        co2_produced++;
        ++total_respiration_count_;
    }
    if (energy_ >= target || glucose_ == 0 || o2_ == 0)
        respiration_credit_ = 0.0;
    // Release the completed reactions' products; retain anything the cell cannot accept.
    if (co2_produced > 0) {
        auto world = world_.lock();
        if (world) {
            auto cell = world->get_cell(pos_);
            const auto accepted = cell->give_co2(co2_produced);
            co2_ -= accepted;
            released_[3] += accepted;
            const auto water_accepted = cell->give_water(co2_produced);
            water_ -= water_accepted;
            released_[1] += water_accepted;
        }
    }
}

void Pop::update_temperature() {
    auto world = world_.lock();
    if (!world)
        return;
    const double env_temp = world->get_temperature(pos_);
    const double alpha = compute_alpha();
    const double heat_input = static_cast<double>(metabolism_heat_ + thermoregulation_heat_);
    temperature_ += alpha * (env_temp - temperature_) + heat_input;
}

void Pop::run_thermoregulation() {
    thermoregulation_heat_ += thermoregulation_heat_gain;
    energy_cost_ += thermoregulation_energy_cost;
    total_thermoregolation_energy_loss_ += thermoregulation_energy_cost;
}

double Pop::compute_alpha() const {
    const double alpha_min = config_->get_phy_alpha_min();
    const double alpha_max = config_->get_phy_alpha_max();
    const double max_lipids = config_->get_phy_max_lipids_ref();
    const double lipid_frac = std::min(static_cast<double>(lipids_) / max_lipids, 1.0);
    return std::clamp(alpha_max - (alpha_max - alpha_min) * lipid_frac, alpha_min, alpha_max);
}

void Pop::increment_offspring_count() {
    offspring_count_++;
}

void Pop::add_reproduction_energy_loss(float energy_loss) {
    if (energy_loss > 0.0f) {
        const float actual_loss = std::min(energy_loss, std::max(energy_, 0.0f));
        change_energy(-actual_loss);
        total_reproduction_energy_loss_ += actual_loss;
    }
}
void Pop::change_energy(float delta) {
    const double before = energy_;
    energy_ += delta;
    total_energy_roundoff_ += static_cast<double>(energy_) - before - static_cast<double>(delta);
}

nlohmann::json Pop::telemetry(bool detailed) const {
    static const char* causes[] = {"external", "energy", "age", "temperature"};
    nlohmann::json result = {{"pop_id", pop_id_},
                             {"parent_id", parent_id_},
                             {"forced", birth_was_forced_},
                             {"alive", alive_},
                             {"age", age_},
                             {"position", {pos_.x, pos_.y}},
                             {"death_cause", causes[static_cast<int>(death_cause_)]},
                             {"resources", {glucose_, water_, calcium_, co2_, o2_, lipids_}},
                             {"actions", action_requests_},
                             {"acquired", acquired_},
                             {"released", released_},
                             {"traits",
                              {{"mitochondria", phy_.mitochondrions},
                               {"chloroplasts", phy_.chloroplasts},
                               {"sensitiveness", phy_.sensitiveness},
                               {"adipose_capacity", phy_.adipose_stock_max}}},
                             {"totals",
                              {{"energy", energy_},
                               {"distance", lifetime_distance_},
                               {"offspring", offspring_count_},
                               {"repro_eligible_ticks", repro_eligible_ticks_},
                               {"repro_cap_ticks", repro_cap_ticks_},
                               {"repro_queue_exclusions", repro_queue_exclusions_},
                               {"repro_admissions", repro_admissions_},
                               {"repro_wait_ticks", repro_wait_ticks_},
                               {"repro_wait_max", repro_wait_max_},
                               {"movement_attempts", movement_attempts_},
                               {"movement_successes", movement_successes_},
                               {"movement_blocked_occupied", movement_failures_[0]},
                               {"movement_blocked_calcium", movement_failures_[1]},
                               {"movement_blocked_slope", movement_failures_[2]},
                               {"movement_blocked_boundary", movement_failures_[3]},
                               {"movement_no_displacement", movement_failures_[4]},
                               {"movement_blocked_other", movement_failures_[5]},
                               {"respirations", total_respiration_count_},
                               {"photosynthesis", total_photosynthesis_count_},
                               {"respiration_gain", total_respiration_energy_gain_},
                               {"respiration_heat", total_respiration_heat_},
                               {"metabolism_cost", total_metabolism_energy_loss_},
                               {"movement_cost", total_movement_energy_loss_},
                               {"other_action_cost", total_other_action_energy_loss_},
                               {"pending_action_cost", energy_cost_},
                               {"reproduction_cost", total_reproduction_energy_loss_},
                               {"thermoregulation_cost", total_thermoregolation_energy_loss_},
                               {"energy_cap", total_energy_cap_loss_},
                               {"roundoff", total_energy_roundoff_},
                               {"body_temperature", temperature_},
                               {"abs_temp_delta", std::abs(temperature_ - config_->get_phy_opt_temperature())}}}};
    const auto learning = brain_->learning_stats();
    const auto& components = scorer_.last_components();
    result["learning"] = {{"reward", scorer_.raw_reward()},
                          {"baseline", scorer_.baseline()},
                          {"signal", scorer_.signal()},
                          {"mean_abs_delta", learning.mean_abs_delta},
                          {"mean_abs_plasticity", learning.mean_abs_plasticity},
                          {"mean_abs_decay", learning.mean_abs_decay},
                          {"mean_abs_trace", learning.mean_abs_trace},
                          {"mean_abs_weight", learning.mean_abs_weight},
                          {"zero_fraction", learning.zero_fraction},
                          {"age_component", components.age},
                          {"energy_component", components.energy},
                          {"delta_component", components.delta},
                          {"offspring_component", components.offspring},
                          {"temperature_component", components.temperature}};
    result["totals"]["learning_updates"] = learning.updates;
    result["totals"]["learning_abs_weight_change"] = learning.abs_weight_change_sum;
    result["totals"]["learning_abs_plasticity"] = learning.abs_plasticity_sum;
    if (detailed) {
        result["sensor_model"] = "chemical_density_v2";
        result["sensing_radius"] = probe_dist_;
        const auto color = genome_.get_genetic_color();
        result["genetics"] = {{"version", 1},
                              {"fingerprint", genome_.fingerprint()},
                              {"similarity_model", "endpoint_weight_v1"},
                              {"color_model", "projection_v1"},
                              {"color", {color.r, color.g, color.b}}};
        result["genes"] = nlohmann::json::array();
        for (const auto& gene : genome_.get_genes())
            result["genes"].push_back({gene.get_source_type(), gene.get_source_num(), gene.get_sink_type(),
                                       gene.get_sink_num(), gene.get_weight()});
        result["last_action"] = last_action_;
        result["sensors"] = sensor_values_;
        result["reward"] = last_reward_;
        result["direction"] = {last_direction_.x, last_direction_.y};
        result["lazyness"] = lazyness_;
        result["phase"] = static_cast<int>(next_phase_);
    }
    return result;
}
nlohmann::json Pop::brain_snapshot() const {
    return brain_->snapshot();
}

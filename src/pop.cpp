#include "pop.h"

#include "brain.h"
#include "color.h"
#include "neuron.h"
#include "random_utility.h"
#include "render_state.h"

#include <algorithm>
#include <atomic>
#include <cmath>

namespace {
std::atomic<unsigned> s_pop_counter{0};
}
constexpr float move_cost = 0.01f;
constexpr float basal_metabolism_cost = 0.02f;
constexpr float thermoregulation_energy_cost = 0.05f;
constexpr float thermoregulation_heat_gain = 0.5f;

Pop::Pop(std::weak_ptr<IWorld> world, std::shared_ptr<ILogger> logger, std::shared_ptr<IConfig> config)
    : genome_(Genome(RandomUtility().rnd_int(static_cast<int>(config->get_genome_min_length()),
                                             static_cast<int>(config->get_genome_max_length())))),
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
Pop::~Pop() {
    if (config_->get_brain_serialization_enabled()) {
        brain_->remove_serialization(pop_id_);
    }
}

void Pop::init() {
    const bool inherit_phy = inherit_phy_;
    alive_ = true;
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
    total_movement_energy_loss_ = 0.0f;
    total_metabolism_energy_loss_ = 0.0f;
    total_reproduction_energy_loss_ = 0.0f;
    total_respiration_energy_gain_ = 0.0f;
    total_thermoregolation_energy_loss_ = 0.0f;
    if (!inherit_phy) {
        phy_.mitochondrions = static_cast<unsigned>(
            random_util_->rnd_int(1, static_cast<int>(config_->get_phy_init_mitochondrions_max())));
        phy_.chloroplasts =
            static_cast<unsigned>(random_util_->rnd_int(0, static_cast<int>(config_->get_phy_init_chloroplasts_max())));
        phy_.sensitiveness = static_cast<unsigned>(
            random_util_->rnd_int(1, static_cast<int>(config_->get_phy_init_sensitiveness_max())));
        phy_.adipose_stock_max = config_->get_phy_adipose_stock_max();
    }
    sensor_values_.assign(brain_->get_size_s(), 0.0f);
    brain_->wire(genome_);
    serialize_brain(0);
}

void Pop::die() {
    // TODO: Implement death logic (cleanup, final actions, mark as dead)

    auto world = world_.lock();
    if (world) {
        auto cell = world->get_cell(pos_);
        cell->give_glucose(glucose_);
        cell->give_water(water_);
        cell->give_o2(o2_);
        cell->give_co2(co2_);
        cell->give_calcium(calcium_);
        cell->give_lipids(lipids_);
    }
    alive_ = false;
    // remove serialized brain file for this pop
    if (config_->get_brain_serialization_enabled() && !brain_->remove_serialization(pop_id_)) {
        logger_->error("Failed to remove serialized brain file for Pop with ID: " + pop_id_);
    }
}

bool Pop::try_spawn(PositionT p) {
    pos_ = p;
    auto world = world_.lock();
    if (!world) {
        return false; // World no longer exists
    }
    return world->add_entity(shared_from_this());
}

float Pop::calculate_reward() const {
    logger_->debug("Calculating reward for Pop at position (" + std::to_string(pos_.x) + ", " + std::to_string(pos_.y) +
                   ") with age " + std::to_string(age_) + ", energy " + std::to_string(energy_) +
                   ", and offspring count " + std::to_string(offspring_count_) + ".");
    // Long-term survival signal in [0,1]: rewards living longer without saturating too early.
    const float age_signal = std::clamp(static_cast<float>(age_) / MaxAge, 0.0f, 1.0f);

    // Energy baseline signal in [-1,1]:
    // values below ~25% energy become negative, above become progressively positive.
    const float energy_score = std::clamp(energy_ / MaxEnergy, 0.0f, 1.0f);
    const float energy_signal = std::clamp((energy_score - 0.25f) / 0.75f, -1.0f, 1.0f);

    // Short-term trend signal in [-1,1]: compares this tick energy delta to the pop's own
    // theoretical per-tick respiration capacity (based on its current mitochondria count).
    const float per_respiration = static_cast<float>(config_->get_phy_energy_per_respiration());
    const float max_energy_delta =
        std::max(static_cast<float>(std::max(phy_.mitochondrions, 1u)) * per_respiration, 1e-6f);
    const float energy_delta = energy_ - previous_energy_;
    const float delta_signal = std::clamp(energy_delta / max_energy_delta, -1.0f, 1.0f);

    // Thermal homeostasis signal in [-1,1]: positive near optimal body temperature,
    // negative when far from it.
    const float opt_temp = static_cast<float>(config_->get_phy_opt_temperature());
    const float temp_distance = std::abs(static_cast<float>(temperature_) - opt_temp);
    const float temp_score = std::clamp(1.0f - (temp_distance / 32.5f), 0.0f, 1.0f);
    const float temp_signal = 2.0f * temp_score - 1.0f;

    // Reproduction bonus in [0,1]: kept as a bounded bonus so reward is not dominated by offspring count.
    const float offspring_bonus = std::clamp(static_cast<float>(offspring_count_) / 25.0f, 0.0f, 1.0f);

    // Weight policy:
    // - keep at least a minimal survival/energy pressure,
    // - cap offspring dominance,
    // - assign remaining mass to temperature management.
    const float w_age = std::max(AgeRewardWeight, 0.10f);
    const float w_energy = std::max(EnergyRewardWeight, 0.30f);
    const float w_delta = EnergyDeltaRewardWeight;
    const float w_offspring = std::min(OffspringRewardWeight, 0.05f);
    const float w_temp = std::clamp(1.0f - (w_age + w_energy + w_delta + w_offspring), 0.0f, 1.0f);

    // Final reward is a weighted sum of interpretable signals and stays in [-1,1].
    const float reward_raw = w_age * age_signal + w_energy * energy_signal + w_delta * delta_signal +
                             w_offspring * offspring_bonus + w_temp * temp_signal;

    if (!std::isfinite(reward_raw)) {
        logger_->warning("Non-finite reward_raw detected for Pop " + pop_id_ + ". Forcing reward to -1.");
        return -1.0f;
    }

    if (age_ % 1000 == 0 && std::abs(delta_signal) > 0.98f) {
        logger_->warning("Reward delta component near saturation for Pop " + pop_id_ +
                         " (delta=" + std::to_string(energy_delta) + ", max_delta=" + std::to_string(max_energy_delta) +
                         ", ratio=" + std::to_string(delta_signal) + ").");
    }

    return std::clamp(reward_raw, -1.0f, 1.0f);
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
        const float reward = calculate_reward();
        last_reward_ = reward;
        learn(reward);
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
            case Sensor::GENETIC_SIM_FWD:
                val = 0.5f;
                break; // TODO: needs genome API on IAgent
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
            case Sensor::SENSE_SIGNAL_DRV_N:
                val = gradient(fero_at(pos_, FeromoneT::FOOD_FEROMONE), fero_scan(0, -1, FeromoneT::FOOD_FEROMONE));
                break;
            case Sensor::SENSE_SIGNAL_DRV_W:
                val = gradient(fero_at(pos_, FeromoneT::FOOD_FEROMONE), fero_scan(-1, 0, FeromoneT::FOOD_FEROMONE));
                break;
            case Sensor::SENSE_SIGNAL_DRV_E:
                val = gradient(fero_at(pos_, FeromoneT::FOOD_FEROMONE), fero_scan(1, 0, FeromoneT::FOOD_FEROMONE));
                break;
            case Sensor::SENSE_SIGNAL_DRV_S:
                val = gradient(fero_at(pos_, FeromoneT::FOOD_FEROMONE), fero_scan(0, 1, FeromoneT::FOOD_FEROMONE));
                break;
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
            case Sensor::NUM_SENSES:
                break;
        }
        sensor_values_[i] = val;
    }
}

void Pop::think() {
    current_state_ = State::THINK;
    // sensor_values_ filled by sense() in the current cycle.
    last_action_ = brain_->feed_forward(sensor_values_);
    first_run_ = false; // Clear the first-run guard after the initial think() to allow learning in subsequent cycles
}

void Pop::serialize_brain(unsigned generation) const {
    if (!config_->get_brain_serialization_enabled()) {
        return;
    }
    brain_->serialize(pop_id_, generation);
}

void Pop::act() {
    current_state_ = State::ACT;

    if (last_action_ < 0 || last_action_ > static_cast<int>(Action::KILL_FORWARD)) {
        return;
    }

    PositionT p = pos_;
    auto world = world_.lock();

    switch (static_cast<Action>(last_action_)) {
        // ── movement ────────────────────────────────────────────────────────
        case Action::MOVE_FORWARD:
            p.x += last_direction_.x * (responsiveness_);
            p.y += last_direction_.y * (responsiveness_);
            try_move(p);
            energy_cost_ += move_cost;
            total_movement_energy_loss_ += move_cost;
            break;
        case Action::MOVE_LEFT:
            p.x -= last_direction_.y * responsiveness_;
            p.y += last_direction_.x * responsiveness_;
            try_move(p);
            energy_cost_ += move_cost;
            total_movement_energy_loss_ += move_cost;
            break;
        case Action::MOVE_RIGHT:
            p.x += last_direction_.y * responsiveness_;
            p.y -= last_direction_.x * responsiveness_;
            try_move(p);
            energy_cost_ += move_cost;
            total_movement_energy_loss_ += move_cost;
            break;
        case Action::MOVE_RANDOM:
            p.x += random_util_->rnd_int(-1, 1) * responsiveness_;
            p.y += random_util_->rnd_int(-1, 1) * responsiveness_;
            try_move(p);
            energy_cost_ += move_cost;
            total_movement_energy_loss_ += move_cost;
            break;
        case Action::MOVE_EAST:
            p.x += 1 * responsiveness_;
            try_move(p);
            energy_cost_ += move_cost;
            total_movement_energy_loss_ += move_cost;
            break;
        case Action::MOVE_WEST:
            p.x -= 1 * responsiveness_;
            try_move(p);
            energy_cost_ += move_cost;
            total_movement_energy_loss_ += move_cost;
            break;
        case Action::MOVE_NORTH:
            p.y -= 1 * responsiveness_;
            try_move(p);
            energy_cost_ += move_cost;
            total_movement_energy_loss_ += move_cost;
            break;
        case Action::MOVE_SOUTH:
            p.y += 1 * responsiveness_;
            try_move(p);
            energy_cost_ += move_cost;
            total_movement_energy_loss_ += move_cost;
            break;
        // ── feromone signalling ─────────────────────────────────────────────
        case Action::EMIT_SIGNAL_FOOD:
            emit_feromone(FeromoneT::FOOD_FEROMONE, RandomUtility().rnd_int(70, 100));
            break;
        case Action::EMIT_SIGNAL_DANGER:
            emit_feromone(FeromoneT::DANGER_FEROMONE, RandomUtility().rnd_int(70, 100));
            break;
        case Action::EMIT_SIGNAL_MATE:
            emit_feromone(FeromoneT::MATE_FEROMONE, RandomUtility().rnd_int(70, 100));
            break;
        case Action::EMIT_SIGNAL_HOME:
            emit_feromone(FeromoneT::HOME_FEROMONE, RandomUtility().rnd_int(70, 100));
            break;
        // ── resource acquisition ────────────────────────────────────────────
        case Action::GET_GLUCOSE:
            if (world) {
                glucose_ += world->get_cell(pos_)->take_glucose(10);
            }
            break;
        case Action::LEAVE_GLUCOSE:
            if (world) {
                world->get_cell(pos_)->give_glucose(10);
            }
            break;
        case Action::GET_H2O:
            if (world) {
                water_ += world->get_cell(pos_)->take_water(10);
            }
            break;

        case Action::LEAVE_H2O:
            if (world) {
                world->get_cell(pos_)->give_water(10);
            }
            break;
        case Action::GET_CALCIUM:
            if (world) {
                calcium_ += world->get_cell(pos_)->take_calcium(10);
            }
            break;
        case Action::LEAVE_CALCIUM:
            if (world) {
                world->get_cell(pos_)->give_calcium(10);
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
            break;
        // ── sentinels ────────────────────────────────────────────────────────────
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
            auto agent = std::dynamic_pointer_cast<IAgent>(occ);
            if (agent)
                agent->die();
            break;
        }
    }
}

bool Pop::try_move(PositionT p) {
    auto world = world_.lock();
    if (!world) {
        logger_->error("Failed to move: World no longer exists.");
        return false; // World no longer exists
    }
    auto stiff = world->get_elevation(p) - world->get_elevation(pos_);
    if (stiff < MaxClimbableSlope * (1.0f - lazyness_)) {
        // cache last position for direction calculation
        PositionT old_pos = pos_;
        //  movement logic
        if (world->move_entity(shared_from_this(), p)) {
            pos_ = p;
            update_last_direction(pos_, old_pos);
            return true;
        }
        logger_->debug("Move to position (" + std::to_string(p.x) + ", " + std::to_string(p.y) + ") failed.");
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

Genome Pop::make_offspring_genome() const {
    if (config_->get_hebbian_inheritance()) {
        if (auto lg = brain_->get_learned_genome()) {
            if (!lg->get_genes().empty()) {
                return lg->mutated(config_->get_point_mutation_rate());
            }
        }
    }
    return genome_.mutated(config_->get_point_mutation_rate());
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

void Pop::donate_resources(unsigned& out_glucose, unsigned& out_water, unsigned& out_calcium, unsigned& out_co2) {
    out_glucose = glucose_ / 8;
    out_water = water_ / 8;
    out_calcium = calcium_ / 8;
    out_co2 = co2_ / 8;
    glucose_ -= out_glucose;
    water_ -= out_water;
    calcium_ -= out_calcium;
    co2_ -= out_co2;
}

void Pop::set_resources(unsigned glucose, unsigned water, unsigned calcium, unsigned co2) {
    glucose_ = glucose;
    water_ = water;
    calcium_ = calcium;
    co2_ = co2;
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
    glucose_ += world->get_cell(pos_)->take_glucose(5);
    water_ += world->get_cell(pos_)->take_water(5);
    calcium_ += world->get_cell(pos_)->take_calcium(2);
    co2_ += world->get_cell(pos_)->take_co2(5);
    lipids_ += world->get_cell(pos_)->take_lipids(1);
    o2_ += world->get_cell(pos_)->take_o2(5);

    // Biological reactions
    run_chloroplasts();
    run_mitochondrions();
    update_temperature();
    thermoregulation_heat_ = 0.0f;

    // Metabolism cost
    energy_ -= basal_metabolism_cost;
    total_metabolism_energy_loss_ += basal_metabolism_cost;
    energy_ -= energy_cost_; // Additional cost from actions
                             // reset energy cost for this cycle
    energy_cost_ = 0;
    // cap energy to MaxEnergy
    if (energy_ > MaxEnergy) {
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
    world->get_cell(pos_)->give_o2(1);
}

void Pop::run_mitochondrions() {
    unsigned co2_produced = 0;
    const float opt_temp = static_cast<float>(config_->get_phy_opt_temperature());
    constexpr float kEfficiencyHalfRangeC = 10.0f;
    const float temp_delta = std::abs(static_cast<float>(temperature_) - opt_temp);
    const float efficiency = std::clamp(1.0f - (temp_delta / kEfficiencyHalfRangeC), 0.0f, 1.0f);
    for (unsigned i = 0; i < phy_.mitochondrions; ++i) {
        if (glucose_ == 0 || o2_ == 0)
            break;
        // C6H12O6 + O2 -> energy + CO2 + heat
        glucose_--;
        o2_--;
        const float energy_gain = static_cast<float>(config_->get_phy_energy_per_respiration()) * efficiency;
        energy_ += energy_gain;
        total_respiration_energy_gain_ += energy_gain;
        metabolism_heat_ += static_cast<float>(config_->get_phy_heat_per_respiration());
        co2_++;
        co2_produced++;
    }
    // Release produced CO2 into the cell
    if (co2_produced > 0) {
        auto world = world_.lock();
        if (world)
            world->get_cell(pos_)->give_co2(co2_produced);
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
        total_reproduction_energy_loss_ += energy_loss;
    }
}
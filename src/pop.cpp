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

Pop::Pop(std::weak_ptr<IWorld> world, std::shared_ptr<ILogger> logger, std::shared_ptr<IConfig> config)
    : genome_(Genome(RandomUtility().rnd_int(static_cast<int>(config->get_genome_min_length()),
                                             static_cast<int>(config->get_genome_max_length())))),
      world_(std::move(world)), logger_(std::move(logger)), config_(std::move(config)),
      brain_(std::make_unique<Brain>(config_)), pos_{0, 0}, age_(0), energy_(100.0f), last_direction_{0, 0},
      random_util_(std::make_shared<RandomUtility>()) {
    pop_id_ = "pop_" + std::to_string(s_pop_counter++);
    init();
}
void Pop::init() {
    alive_ = true;
    glucose_ = 100;
    water_ = 100;
    sensor_values_.assign(brain_->get_size_s(), 0.0f);
    brain_->wire(genome_);
    brain_->serialize(pop_id_, 0);
}

void Pop::die() {
    // TODO: Implement death logic (cleanup, final actions, mark as dead)
    alive_ = false;
}

bool Pop::try_spawn(PositionT p) {
    pos_ = p;
    auto world = world_.lock();
    if (!world) {
        return false; // World no longer exists
    }
    return world->add_entity(shared_from_this());
}

void Pop::update() {
    age_++;
    // energy_ -= 0.01f; // Example energy consumption
    if (energy_ <= 0) {
        die();
    }
}

void Pop::despawn() {
    alive_ = false;
    auto world = world_.lock();
    if (world) {
        world->remove_entity(shared_from_this());
    }
}

void Pop::sense() {
    current_state_ = State::SENSE;

    auto world = world_.lock();
    if (!world)
        return;

    const int W = world->get_width();
    const int H = world->get_height();
    const unsigned ns = brain_->get_size_s();
    sensor_values_.assign(ns, 0.0f);
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

    // ---- per-sensor evaluation -----------------------------------------
    const float bx = (W > 1) ? static_cast<float>(std::min(pos_.x, W - 1 - pos_.x)) / (W * 0.5f) : 0.0f;
    const float by = (H > 1) ? static_cast<float>(std::min(pos_.y, H - 1 - pos_.y)) / (H * 0.5f) : 0.0f;

    for (unsigned i = 0; i < ns; ++i) {
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
}

void Pop::serialize_brain(unsigned generation) const {
    brain_->serialize(pop_id_, generation);
}

void Pop::act() {
    current_state_ = State::ACT;

    if (last_action_ < 0 || last_action_ > static_cast<int>(Action::KILL_FORWARD))
        return;

    PositionT p = pos_;
    auto world = world_.lock();

    switch (static_cast<Action>(last_action_)) {
        // ── movement ────────────────────────────────────────────────────────
        case Action::MOVE_FORWARD:
            p.x += last_direction_.x;
            p.y += last_direction_.y;
            try_move(p);
            break;
        case Action::MOVE_LEFT:
            p.x -= last_direction_.y;
            p.y += last_direction_.x;
            try_move(p);
            break;
        case Action::MOVE_RIGHT:
            p.x += last_direction_.y;
            p.y -= last_direction_.x;
            try_move(p);
            break;
        case Action::MOVE_RANDOM:
            p.x += random_util_->rnd_int(-1, 1);
            p.y += random_util_->rnd_int(-1, 1);
            try_move(p);
            break;
        case Action::MOVE_EAST:
            p.x += 1;
            try_move(p);
            break;
        case Action::MOVE_WEST:
            p.x -= 1;
            try_move(p);
            break;
        case Action::MOVE_NORTH:
            p.y -= 1;
            try_move(p);
            break;
        case Action::MOVE_SOUTH:
            p.y += 1;
            try_move(p);
            break;
        // ── feromone signalling ─────────────────────────────────────────────
        case Action::EMIT_SIGNAL_FOOD:
            emit_feromone(FeromoneT::FOOD_FEROMONE, 50);
            break;
        case Action::EMIT_SIGNAL_DANGER:
            emit_feromone(FeromoneT::DANGER_FEROMONE, 50);
            break;
        case Action::EMIT_SIGNAL_MATE:
            emit_feromone(FeromoneT::MATE_FEROMONE, 50);
            break;
        case Action::EMIT_SIGNAL_HOME:
            emit_feromone(FeromoneT::HOME_FEROMONE, 50);
            break;
        // ── resource acquisition ────────────────────────────────────────────
        case Action::GET_GLUCOSE:
            if (world)
                glucose_ += world->get_cell(pos_)->take_glucose(2);
            break;
        case Action::GET_H2O:
            if (world)
                water_ += world->get_cell(pos_)->take_water(2);
            break;
        case Action::GET_CALCIUM:
            if (world)
                calcium_ += world->get_cell(pos_)->take_calcium(2);
            break;
        // ── internal modulation ──────────────────────────────────────────────
        case Action::SET_OSCILLATOR_PERIOD: {
            const float spg = static_cast<float>(config_->get_steps_per_generation());
            osc_period_ = std::clamp(osc_period_ * 1.1f, 10.0f, spg > 0 ? spg : 10.0f);
            break;
        }
        case Action::SET_RESPONSIVENESS:
            responsiveness_ = std::clamp(responsiveness_ + 0.1f, 0.25f, 4.0f);
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
    if (stiff < MaxClimbableSlope) {
        // cache last position for direction calculation
        PositionT old_pos = pos_;
        //  movement logic
        if (world->move_entity(shared_from_this(), p)) {
            pos_ = p;
            update_last_direction(pos_, old_pos);
            return true;
        }
        logger_->warning("Move to position (" + std::to_string(p.x) + ", " + std::to_string(p.y) + ") failed.");
    }
    return false;
}

PositionT Pop::get_position() const {
    return pos_;
}

bool Pop::is_alive() {
    return alive_;
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
    state.color = c;                                                //  Black if dead
    state.size = size;                                              //  size
    state.payload = PopVisualData{energy_ / 100.0f, age_ / 100.0f}; // Example payload
    state.shape = RenderShape::Circle;                              //  shape
    return state;
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
[[nodiscard]] int Pop::get_feromone_strength(FeromoneT type, PositionT pos) const {
    auto world = world_.lock();
    if (!world) {
        return 0; // World no longer exists
    }
    int strength = world->get_feromone_strength(type, pos);
    return strength;
}

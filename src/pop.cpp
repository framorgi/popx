#include "pop.h"

#include "color.h"
#include "neuron.h"
#include "random_utility.h"
#include "render_state.h"

Pop::Pop(std::weak_ptr<IWorld> world, std::shared_ptr<ILogger> logger)
    : genome_(Genome(4)), world_(std::move(world)), logger_(std::move(logger)), pos_{0, 0}, age_(0), energy_(100.0f),
      last_direction_{0, 0}, random_util_(std::make_shared<RandomUtility>()) {
    init();
}
void Pop::init() {
    // TODO: Implement initialization logic for Pop (set initial stats, behaviors, etc.)
    alive_ = true;
}

void Pop::die() {
    // TODO: Implement death logic (cleanup, final actions, mark as dead)
    alive_ = false;
}

bool Pop::try_spawn(Position p) {
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
    // set  current state to SENSE
    current_state_ = State::SENSE;

    Sensor sensor = Sensor::RANDOM; // Example: using RANDOM sensor
    double val = 0.0;
    switch (sensor) {
        case Sensor::LOC_X:
            val = static_cast<double>(pos_.x) / map_size;
            break;
        case Sensor::LOC_Y:
            val = static_cast<double>(pos_.y) / map_size;
            break;
        case Sensor::BOUNDARY_DIST_X:
        case Sensor::BOUNDARY_DIST:
        case Sensor::BOUNDARY_DIST_Y:
        case Sensor::GENETIC_SIM_FWD:
        case Sensor::LAST_MOVE_DIR_X:
        case Sensor::LAST_MOVE_DIR_Y:
        case Sensor::POPULATION_DENSITY_N:
        case Sensor::POPULATION_DENSITY_W:
        case Sensor::POPULATION_DENSITY_E:
        case Sensor::POPULATION_DENSITY_S:
        case Sensor::TEMP_AVG_N:
        case Sensor::TEMP_AVG_W:
        case Sensor::TEMP_AVG_E:
        case Sensor::TEMP_AVG_S:
        case Sensor::TEMP_DRV_N:
        case Sensor::TEMP_DRV_W:
        case Sensor::TEMP_DRV_E:
        case Sensor::TEMP_DRV_S:
        case Sensor::SENSE_SIGNAL:
        case Sensor::SENSE_SIGNAL_DRV_N:
        case Sensor::SENSE_SIGNAL_DRV_W:
        case Sensor::SENSE_SIGNAL_DRV_E:
        case Sensor::SENSE_SIGNAL_DRV_S:
        case Sensor::GLUCOSE_DENSITY_N:
        case Sensor::GLUCOSE_DENSITY_W:
        case Sensor::GLUCOSE_DENSITY_E:
        case Sensor::GLUCOSE_DENSITY_S:
        case Sensor::OSC1:
        case Sensor::AGE:
        case Sensor::TEMP:
        case Sensor::RANDOM:
        case Sensor::NUM_SENSES:
            break;
    }
    // Use world_.lock() to access the world safely
}

void Pop::think() {
    // set  current state to THINK
    current_state_ = State::THINK;
}

void Pop::act() {
    // set  current state to ACT
    current_state_ = State::ACT;

    Position p = pos_;
    auto random_action = static_cast<Action>(random_util_->rnd_int(0, 12));
    switch (random_action) {
        case Action::MOVE_FORWARD: // W continue last direction
        {
            p.x += last_direction_.x;
            p.y += last_direction_.y;
            try_move(p);
            break;
        }

        case Action::MOVE_BACKWARD: // W
        {
            p.x += -last_direction_.x;
            p.y += -last_direction_.y;
            try_move(p);
            break;
        } break;
        case Action::MOVE_LEFT: // W
        {
            p.x += -last_direction_.y;
            p.y += last_direction_.x;
            try_move(p);
            break;
        } break;
        case Action::MOVE_RIGHT: // W

        {
            p.x += last_direction_.y;
            p.y += -last_direction_.x;
            try_move(p);
            break;
        }

        case Action::MOVE_RANDOM: // W
        {
            p.y += random_util_->rnd_int(-1, 1);
            p.x += random_util_->rnd_int(-1, 1);
            try_move(p);
            break;
        }
        case Action::MOVE_EAST: // W
        {
            p.x += 1;
            try_move(p);
            break;
        }
        case Action::MOVE_WEST: // W
        {
            p.x += -1;
            try_move(p);
            break;
        }
        case Action::MOVE_NORTH: // W
        {
            p.y += -1;
            try_move(p);
            break;
        }
        case Action::MOVE_SOUTH: // W
        {
            p.y += 1;
            try_move(p);

            break;
        }
        case Action::SET_OSCILLATOR_PERIOD: // I
            // TODO: Implement set oscillator period
            break;
        case Action::SET_LONGPROBE_DIST: // I
            // TODO: Implement set long probe distance
            break;
        case Action::SET_RESPONSIVENESS: // I
            // TODO: Implement set responsiveness
            break;
        case Action::EMIT_SIGNAL: // W
            // TODO: Implement emit signal
            emit_feromone(FeromoneT::DANGER_FEROMONE, 100);
            break;
        case Action::BURN_CALORIES: // W
            // TODO: Implement burn calories
            break;
        case Action::NUM_ACTIONS: // <<----------------- END OF ACTIVE ACTIONS MARKER
            // Invalid action
            break;
        case Action::KILL_FORWARD: // W
            // TODO: Implement kill forward
            break;
    }
}

bool Pop::try_move(Position p) {
    auto world = world_.lock();
    if (!world) {
        logger_->error("Failed to move: World no longer exists.");
        return false; // World no longer exists
    }
    // cache last position for direction calculation
    Position old_pos = pos_;
    //  movement logic
    if (world->move_entity(shared_from_this(), p)) {
        pos_ = p;
        update_last_direction(pos_, old_pos);
        return true;
    }
    logger_->warning("Move to position (" + std::to_string(p.x) + ", " + std::to_string(p.y) + ") failed.");
    return false;
}

Position Pop::get_position() const {
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

void Pop::update_last_direction(Position new_pos, Position old_pos) {
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
#include "pop.h"

#include "neuron.h"
#include "random_utility.h"
#include "render_state.h"

Pop::Pop(std::weak_ptr<IWorld> world, std::shared_ptr<ILogger> logger)
    : genome_(Genome(4)), world_(std::move(world)), logger_(std::move(logger)), pos_{0, 0} {
    init();
}
void Pop::init() {
    // TODO: Implement initialization logic for Pop (set initial stats, behaviors, etc.)
    alive_ = true;
    age_ = 0;
    energy_ = 100.0f; // Example initial energy
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

    auto world = world_.lock();
    if (!world) {
        return; // World no longer exists
    }
    RandomUtility rand_util;
    return world->set_feromone(pos_, Feromone_t::DANGER_FEROMONE, max_feromones);
    // TODO: Implement entity update logic (called each tick)
}

void Pop::despawn() {
    alive_ = false;
    auto world = world_.lock();
    if (world) {
        world->remove_entity(shared_from_this());
    }
}

void Pop::sense() {
    // TODO: Implement sensing logic (perceive nearby entities, environment, etc.)
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
    // TODO: Implement decision-making logic (AI, behavior selection, goal planning)
}

void Pop::act() {
    // TODO: Implement action execution (move, interact, consume resources, etc.)
}

bool Pop::try_move(Position p) {
    // TODO: Implement movement logic (validate move, update position, notify world)
    auto world = world_.lock();
    if (!world) {
        logger_->error("Failed to move: World no longer exists.");
        return false; // World no longer exists
    }
    // Add actual movement logic here
    if (world->move_entity(shared_from_this(), p)) {
        pos_ = p;

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
    RenderState state;
    state.position = Vec2{static_cast<float>(pos_.x), static_cast<float>(pos_.y)};
    state.color = alive_ ? genome_.get_genetic_color() : Color{0, 0, 0}; //  Black if dead
    state.size = 1;                                                      //  size
    state.payload = PopVisualData{energy_ / 100.0f, age_ / 100.0f};      // Example payload
    state.shape = RenderShape::Circle;                                   //  shape
    return state;
}

void Pop::update_last_direction(Position new_pos, Position old_pos) {
    last_direction_.x = new_pos.x - old_pos.x;
    last_direction_.y = new_pos.y - old_pos.y;
}
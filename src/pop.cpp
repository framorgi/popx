#include "pop.h"

Pop::Pop(std::weak_ptr<IWorld> world) : world_(world) {
    alive_ = true;
    pos_.x = 0;
    pos_.y = 0;
}
void Pop::init() {
    // TODO: Implement initialization logic for Pop (set initial stats, behaviors, etc.)
}

void Pop::die() {
    // TODO: Implement death logic (cleanup, final actions, mark as dead)
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
        return false; // World no longer exists
    }
    // Add actual movement logic here
    return false;
}

Position Pop::get_position() const {
    return pos_;
}

bool Pop::is_alive() {
    return alive_;
}

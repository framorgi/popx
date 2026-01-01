#include "grid_world.h"

#include "cell.h"

GridWorld::GridWorld(int w, int h, std::shared_ptr<ILogger> logger) : width_(w), height_(h), logger_(logger) {
    cells_.resize(width_ * height_);
    for (auto& cell : cells_) {
        cell = std::make_shared<Cell>();
    }
    logger_->debug("Grid world created with size " + std::to_string(width_) + "x" + std::to_string(height_));
}

void GridWorld::init() {
    // TODO: Reconsider this implementation - setting to nullptr wastes constructor allocation
    for (auto& cell : cells_) {
        // cell = nullptr;
    }
}

bool GridWorld::is_free(Position p) const {
    if (!in_bounds(p.x, p.y)) {
        logger_->warning("Position (" + std::to_string(p.x) + "," + std::to_string(p.y) + ") is out of bounds.");
        return false;
    }

    auto weak_occupant = cells_[index(p.x, p.y)]->get_occupant();
    auto locked = weak_occupant.lock();

    // Debug: check use_count and owner_before

    bool is_free = (locked == nullptr);

    return is_free;
}

bool GridWorld::move_entity(std::shared_ptr<IEntity> entity, Position new_pos) {
    if (!is_free(new_pos)) {
        logger_->warning("Failed to move entity to occupied position (" + std::to_string(new_pos.x) + "," +
                         std::to_string(new_pos.y) + ")");
        return false;
    }

    logger_->debug("Moving entity to position (" + std::to_string(new_pos.x) + "," + std::to_string(new_pos.y) + ")");
    Position old_pos = entity->get_position();
    cells_[index(old_pos.x, old_pos.y)]->set_occupant(std::weak_ptr<IEntity>());
    cells_[index(new_pos.x, new_pos.y)]->set_occupant(entity);

    return true;
}

bool GridWorld::remove_entity(std::shared_ptr<IEntity> e) {
    Position pos = e->get_position();
    if (!in_bounds(pos.x, pos.y)) {
        return false;
    }

    auto weak_occupant = cells_[index(pos.x, pos.y)]->get_occupant();
    auto locked_occupant = weak_occupant.lock();

    if (!locked_occupant || locked_occupant.get() != e.get()) {
        return false;
    }

    cells_[index(pos.x, pos.y)]->set_occupant(std::weak_ptr<IEntity>());
    return true;
}
bool GridWorld::add_entity(std::shared_ptr<IEntity> e) {
    Position pos = e->get_position();
    logger_->debug("Adding entity to grid world at position (" + std::to_string(pos.x) + "," + std::to_string(pos.y) +
                   ")");
    if (!is_free(pos)) {
        logger_->warning("Cannot add entity - position already occupied!");
        return false;
    }
    cells_[index(pos.x, pos.y)]->set_occupant(e);
    logger_->info("Entity successfully added to position (" + std::to_string(pos.x) + "," + std::to_string(pos.y) +
                  ")");
    return true;
}

bool GridWorld::in_bounds(int x, int y) const {
    return x >= 0 && y >= 0 && x < width_ && y < height_;
}

int GridWorld::index(int x, int y) const {
    return y * width_ + x;
}

bool GridWorld::update_cycle() {
    // TODO: Implement world update logic (resource regeneration, environmental changes, etc.)
    return true;
}

#include "grid_world.h"

#include "cell.h"
#include "common.h"
#include "i_random.h"
#include "random_utility.h"

GridWorld::GridWorld(int w, int h, std::shared_ptr<ILogger> logger) : width_(w), height_(h), logger_(logger) {
    cells_.resize(width_ * height_);
    for (auto& cell : cells_) {
        cell = std::make_shared<Cell>();
    }
    logger_->debug("Grid world created with size " + std::to_string(width_) + "x" + std::to_string(height_));
}

void GridWorld::init() {
    RandomUtility random_util;

    /// Generate a random RBF set for temperature
    RBFSet temp_rbf_set = random_util.rnd_rbf_set(10, 5.0, 100.0, 0.0, static_cast<double>(width_), 0.0,
                                                  static_cast<double>(height_), 6.0, 20.0, 6.0, 20.0);

    /// Generate a random RBF set for elevation
    RBFSet elevation_rbf_set = random_util.rnd_symmetric_rbf_set(20, 5.0, 100.0, 0.0, static_cast<double>(width_), 0.0,
                                                                 static_cast<double>(height_), 0.0, 20.0);

    /// Generate a random RBF set for glucose distribution
    RBFSet glucose_rbf_set = random_util.rnd_rbf_set(35, MaxC6h12o6 / 8, MaxC6h12o6, 0, static_cast<double>(width_),
                                                     0.0, static_cast<double>(height_), 5.0, 20.0, 5.0, 10.0);
    ///
    for (int y = 0; y < height_; ++y) {
        for (int x = 0; x < width_; ++x) {
            cells_[index(x, y)]->set_occupant(std::weak_ptr<IEntity>());
            double temp_value =
                random_util.evaluate_rbf_set(temp_rbf_set, static_cast<double>(x), static_cast<double>(y));
            cells_[index(x, y)]->set_temperature((temp_value));
            double elevation_value =
                random_util.evaluate_rbf_set(elevation_rbf_set, static_cast<double>(x), static_cast<double>(y));
            cells_[index(x, y)]->set_elevation((elevation_value));

            auto glucose_value = static_cast<unsigned>(
                random_util.evaluate_rbf_set(glucose_rbf_set, static_cast<double>(x), static_cast<double>(y)));
            cells_[index(x, y)]->set_glucose((glucose_value));
        }
    }
    logger_->info("Grid world initialized.");
}

bool GridWorld::is_free(PositionT p) const {
    if (!in_bounds(p.x, p.y)) {
        logger_->debug("Position (" + std::to_string(p.x) + "," + std::to_string(p.y) + ") is out of bounds.");
        return false;
    }

    auto weak_occupant = cells_[index(p.x, p.y)]->get_occupant();
    auto locked = weak_occupant.lock();

    // Debug: check use_count and owner_before

    bool is_free = (locked == nullptr);

    return is_free;
}

bool GridWorld::move_entity(std::shared_ptr<IEntity> entity, PositionT new_pos) {
    if (!is_free(new_pos)) {
        logger_->debug("Failed to move entity to occupied position (" + std::to_string(new_pos.x) + "," +
                       std::to_string(new_pos.y) + ")");
        return false;
    }

    logger_->debug("Moving entity to position (" + std::to_string(new_pos.x) + "," + std::to_string(new_pos.y) + ")");
    PositionT old_pos = entity->get_position();
    cells_[index(old_pos.x, old_pos.y)]->set_occupant(std::weak_ptr<IEntity>());
    cells_[index(new_pos.x, new_pos.y)]->set_occupant(entity);

    return true;
}

bool GridWorld::remove_entity(std::shared_ptr<IEntity> e) {
    PositionT pos = e->get_position();
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
    PositionT pos = e->get_position();
    logger_->debug("Adding entity to grid world at position (" + std::to_string(pos.x) + "," + std::to_string(pos.y) +
                   ")");
    if (!is_free(pos)) {
        logger_->debug("Cannot add entity - position already occupied!");
        return false;
    }
    cells_[index(pos.x, pos.y)]->set_occupant(e);
    logger_->debug("Entity successfully added to position (" + std::to_string(pos.x) + "," + std::to_string(pos.y) +
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
    for (auto& cell : cells_) {
        cell->update();
    }
    return true;
}

double GridWorld::get_feromone_magnitude(PositionT p, FeromoneT type) const {
    if (!in_bounds(p.x, p.y)) {
        return 0.0;
    }
    auto cell = cells_[index(p.x, p.y)];
    FeromoneMapT fm = cell->get_feromone_map();

    switch (type) {
        case FeromoneT::DANGER_FEROMONE:
            return static_cast<double>(fm.at(FeromoneT::DANGER_FEROMONE)) / MaxFeromones;
        case FeromoneT::FOOD_FEROMONE:
            return static_cast<double>(fm.at(FeromoneT::FOOD_FEROMONE)) / MaxFeromones;
        case FeromoneT::MATE_FEROMONE:
            return static_cast<double>(fm.at(FeromoneT::MATE_FEROMONE)) / MaxFeromones;

        case FeromoneT::HOME_FEROMONE:
            return static_cast<double>(fm.at(FeromoneT::HOME_FEROMONE)) / MaxFeromones;
        default:
            return 0.0; // Other feromone types not implemented in visual data
    }
}

void GridWorld::set_feromone(PositionT p, FeromoneT type, float value) {
    if (!in_bounds(p.x, p.y)) {
        logger_->debug("Cannot set feromone - position out of bounds!");
        return;
    }
    auto cell = cells_[index(p.x, p.y)];
    cell->set_feromone(type, value);
}

[[nodiscard]] float GridWorld::get_feromone_strength(FeromoneT type, PositionT pos) const {
    if (!in_bounds(pos.x, pos.y)) {
        return 0.0f; // Out of bounds
    }
    auto cell = cells_[index(pos.x, pos.y)];
    FeromoneMapT fm = cell->get_feromone_map();

    switch (type) {
        case FeromoneT::DANGER_FEROMONE:
            return fm.at(FeromoneT::DANGER_FEROMONE);
        case FeromoneT::FOOD_FEROMONE:
            return fm.at(FeromoneT::FOOD_FEROMONE);
        case FeromoneT::MATE_FEROMONE:
            return fm.at(FeromoneT::MATE_FEROMONE);
        case FeromoneT::HOME_FEROMONE:
            return fm.at(FeromoneT::HOME_FEROMONE);
        default:
            return 0; // Other feromone types not implemented
    }
}

[[nodiscard]] double GridWorld::get_temperature(PositionT pos) const {
    if (!in_bounds(pos.x, pos.y)) {
        return 0.0; // Out of bounds
    }
    auto cell = cells_[index(pos.x, pos.y)];
    return cell->get_temperature();
}

[[nodiscard]] double GridWorld::get_elevation(PositionT pos) const {
    if (!in_bounds(pos.x, pos.y)) {
        return 0; // Out of bounds
    }
    auto cell = cells_[index(pos.x, pos.y)];
    return static_cast<int>(cell->get_elevation());
}

OrganicsT GridWorld::get_total_organics() const {
    OrganicsT total{};
    for (const auto& cell : cells_) {
        const auto o = cell->get_organics();
        total.c6h12o6 += o.c6h12o6;
        total.lipids += o.lipids;
        total.o2 += o.o2;
        total.co2 += o.co2;
        total.h2o += o.h2o;
        total.n2 += o.n2;
        total.caco3 += o.caco3;
    }
    return total;
}
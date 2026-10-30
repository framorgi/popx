#include "grid_world.h"

#include "cell.h"
#include "common.h"
#include "random_utility.h"

GridWorld::GridWorld(int w, int h, std::shared_ptr<ILogger> logger, SourceSettings settings, double climate_speed,
                     ReaperSettings reaper_settings)
    : width_(w), height_(h), logger_(logger), source_settings_(settings), reaper_settings_(reaper_settings),
      climate_rotation_speed_(climate_speed) {
    cells_.resize(width_ * height_);
    for (auto& cell : cells_) {
        cell = std::make_shared<Cell>();
    }
    logger_->debug("Grid world created with size " + std::to_string(width_) + "x" + std::to_string(height_));
}

void GridWorld::init() {
    glucose_regenerated_ = oxygen_regenerated_ = reaper_kills_ = 0;
    environment_tick_counter_ = rotation_tick_counter_ = 0;
    sources_.clear();
    reapers_.clear();
    RandomUtility random_util;

    /// Generate a random RBF set for temperature
    temp_rbf_set_ = random_util.rnd_rbf_set(10, 5.0, 100.0, 0.0, static_cast<double>(width_), 0.0,
                                            static_cast<double>(height_), 6.0, 20.0, 6.0, 20.0);

    /// Generate a random RBF set for elevation
    elevation_rbf_set_ = random_util.rnd_symmetric_rbf_set(20, 5.0, 100.0, 0.0, static_cast<double>(width_), 0.0,
                                                           static_cast<double>(height_), 0.0, 20.0);

    /// Generate a random RBF set for glucose distribution
    glucose_rbf_set_ = random_util.rnd_rbf_set(35, MaxC6h12o6 / 8, MaxC6h12o6, 0, static_cast<double>(width_), 0.0,
                                               static_cast<double>(height_), 5.0, 20.0, 5.0, 10.0);

    /// Generate a random RBF set for water distribution
    water_rbf_set_ = random_util.rnd_rbf_set(35, MaxH2o / 8, MaxH2o, 0, static_cast<double>(width_), 0.0,
                                             static_cast<double>(height_), 5.0, 20.0, 5.0, 10.0);
    ///
    ///
    for (int y = 0; y < height_; ++y) {
        for (int x = 0; x < width_; ++x) {
            cells_[index(x, y)]->set_occupant(std::weak_ptr<IEntity>());
            double temp_value =
                random_util.evaluate_rbf_set(temp_rbf_set_, static_cast<double>(x), static_cast<double>(y));
            cells_[index(x, y)]->set_temperature((temp_value));
            double elevation_value =
                random_util.evaluate_rbf_set(elevation_rbf_set_, static_cast<double>(x), static_cast<double>(y));
            cells_[index(x, y)]->set_elevation((elevation_value));

            auto glucose_value = static_cast<unsigned>(
                random_util.evaluate_rbf_set(glucose_rbf_set_, static_cast<double>(x), static_cast<double>(y)));
            cells_[index(x, y)]->set_glucose((glucose_value));

            auto water_value = static_cast<unsigned>(
                random_util.evaluate_rbf_set(water_rbf_set_, static_cast<double>(x), static_cast<double>(y)));
            cells_[index(x, y)]->set_water((water_value));
        }
    }
    reset_sources();
    reset_reapers();
    logger_->info("Grid world initialized.");
}

void GridWorld::reset_reapers(bool spawn) {
    for (auto& reaper : reapers_) reaper->despawn();
    reapers_.clear();
    std::mt19937 rng(std::random_device{}());
    std::uniform_int_distribution<int> x_dist(0, width_ - 1), y_dist(0, height_ - 1);
    for (unsigned n = 0; n < reaper_settings_.count; ++n) {
        auto reaper = std::make_shared<Reaper>(*this, reaper_settings_, rng());
        if (!spawn) { reapers_.push_back(std::move(reaper)); continue; }
        bool placed = false;
        for (int trial = 0; trial < 1000 && !placed; ++trial)
            placed = reaper->try_spawn({x_dist(rng), y_dist(rng)});
        if (!placed) {
            const int start = std::uniform_int_distribution<int>(0, width_ * height_ - 1)(rng);
            for (int offset = 0; offset < width_ * height_ && !placed; ++offset) {
                const int position = (start + offset) % (width_ * height_);
                placed = reaper->try_spawn({position % width_, position / width_});
            }
        }
        if (!placed) throw std::runtime_error("No free cell available for Reaper");
        reapers_.push_back(std::move(reaper));
    }
}

int GridWorld::nearest_reaper_distance(PositionT position) const {
    int result = -1;
    for (const auto& reaper : reapers_) {
        if (!reaper->is_alive()) continue;
        const auto p = reaper->get_position();
        const int distance = std::max(std::abs(p.x - position.x), std::abs(p.y - position.y));
        if (result < 0 || distance < result) result = distance;
    }
    return result;
}

void GridWorld::reset_sources(bool spawn) {
    for (auto& source : sources_) source->despawn();
    sources_.clear();
    std::mt19937 rng(std::random_device{}());
    std::uniform_int_distribution<int> x_dist(0, width_ - 1), y_dist(0, height_ - 1);
    for (unsigned n = 0; n < source_settings_.count; ++n) {
        auto source = std::make_shared<Source>(*this, source_settings_, rng());
        if (!spawn) { sources_.push_back(std::move(source)); continue; }
        bool placed = false;
        for (int trial = 0; trial < 1000 && !placed; ++trial)
            placed = source->try_spawn({x_dist(rng), y_dist(rng)});
        if (!placed) {
            const int start = std::uniform_int_distribution<int>(0, width_ * height_ - 1)(rng);
            for (int offset = 0; offset < width_ * height_ && !placed; ++offset) {
                const int index = (start + offset) % (width_ * height_);
                placed = source->try_spawn({index % width_, index / width_});
            }
        }
        if (!placed) throw std::runtime_error("No free cell available for Source");
        sources_.push_back(std::move(source));
    }
}

bool GridWorld::sandbox_spawn_source(PositionT position) {
    if (sources_.size() >= 32 || !is_free(position)) return false;
    auto source = std::make_shared<Source>(*this, source_settings_);
    if (!source->try_spawn(position)) return false;
    sources_.push_back(std::move(source));
    source_settings_.count = sources_.size();
    return true;
}

bool GridWorld::sandbox_remove_source(PositionT position) {
    for (auto it = sources_.begin(); it != sources_.end(); ++it) {
        const auto p = (*it)->get_position();
        if (p.x == position.x && p.y == position.y) {
            (*it)->despawn();
            sources_.erase(it);
            source_settings_.count = sources_.size();
            return true;
        }
    }
    return false;
}

bool GridWorld::is_free(PositionT p) const {
    if (!in_bounds(p.x, p.y)) {
        logger_->debug("Position (" + std::to_string(p.x) + "," + std::to_string(p.y) + ") is out of bounds.");
        return false;
    }

    auto weak_occupant = cells_[index(p.x, p.y)]->get_occupant();
    auto carbonate_barrier = cells_[index(p.x, p.y)]->get_organics().caco3;
    auto locked = weak_occupant.lock();

    // Debug: check use_count and owner_before

    bool is_free = (locked == nullptr) && (carbonate_barrier != MaxCaco3);

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
    // Weathering redistributes carbonate without creating matter or new saturated barriers.
    // Recipients stay below capacity; therefore this scan cannot cascade through new barriers.
    if ((environment_tick_counter_ + 1) % CalciumWeatheringInterval == 0) {
        for (int y = 0; y < height_; ++y) {
            for (int x = 0; x < width_; ++x) {
                auto& source = cells_[index(x, y)];
                if (source->get_calcium() != MaxCaco3)
                    continue;
                for (int dy = -1; dy <= 1; ++dy) {
                    for (int dx = -1; dx <= 1; ++dx) {
                        if ((!dx && !dy) || !in_bounds(x + dx, y + dy))
                            continue;
                        auto& target = cells_[index(x + dx, y + dy)];
                        if (target->get_calcium() < MaxCaco3 - 1)
                            source->take_calcium(target->give_calcium(1));
                    }
                }
            }
        }
    }
    for (auto& cell : cells_) {
        const auto oxygen = cell->get_organics().o2;
        cell->update();
        oxygen_regenerated_ += cell->get_organics().o2 - oxygen;
    }
    for (const auto& source : sources_) {
        source->update();
        glucose_regenerated_ += source->last_deposit();
    }
    for (const auto& reaper : reapers_) {
        reaper->update();
        reaper_kills_ += reaper->last_kills();
    }
    if (environment_tick_counter_ % TemperatureRefreshInterval == 0) {
        RandomUtility random_util;
        const double center_x = static_cast<double>(width_) * 0.5;
        const double center_y = static_cast<double>(height_) * 0.5;
        temp_rbf_set_ =
            random_util.rotate_rbf_set_scaled(temp_rbf_set_, rotation_tick_counter_, height_ / 6, center_x, center_y,
                                              climate_rotation_speed_);
        for (int y = 0; y < height_; ++y) {
            for (int x = 0; x < width_; ++x) {
                double temp_value =
                    random_util.evaluate_rbf_set(temp_rbf_set_, static_cast<double>(x), static_cast<double>(y));
                cells_[index(x, y)]->set_temperature((temp_value));
            }
        }
        rotation_tick_counter_++;
    }
    environment_tick_counter_++;
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

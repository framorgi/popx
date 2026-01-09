#include "cell.h"

#include "common.h"
#include "random_utility.h"
#include "render_state.h"

Cell::Cell(std::weak_ptr<IEntity> occupant)
    : occupant_(std::move(occupant)), temperature_(20.0f), humidity_(50.0f), elevation_(0.0f)

{
    // Initialize organics to zero
    organics_ = Organics{0, 0, 0, 0, 0, 0, 0};
    RandomUtility rand_util;
    // Initialize feromones to zero
    feromones_[Feromone_t::FOOD_FEROMONE] = 0;
    feromones_[Feromone_t::DANGER_FEROMONE] = 0;
    feromones_[Feromone_t::MATE_FEROMONE] = 0;
    feromones_[Feromone_t::HOME_FEROMONE] = 0;
}

void Cell::set_occupant(std::weak_ptr<IEntity> occupant) {
    occupant_ = std::move(occupant);
}

[[nodiscard]] std::weak_ptr<IEntity> Cell::get_occupant() const {
    return occupant_;
}

void Cell::set_temperature(double temperature) {
    temperature_ = static_cast<float>(temperature);
}
[[nodiscard]] double Cell::get_temperature() const {
    return static_cast<double>(temperature_);
}

void Cell::set_elevation(double elevation) {
    elevation_ = static_cast<float>(elevation);
}
[[nodiscard]] double Cell::get_elevation() const {
    return static_cast<double>(elevation_);
}

void Cell::set_humidity(double humidity) {
    humidity_ = static_cast<float>(humidity);
}
[[nodiscard]] double Cell::get_humidity() const {
    return static_cast<double>(humidity_);
}

void Cell::set_feromone(Feromone_t type, int value) {
    feromones_[type] += value;

    if (feromones_[type] < 0) {
        feromones_[type] = 0;
    }
    if (feromones_[type] > (max_feromones)) {
        feromones_[type] = (max_feromones);
    }
}

[[nodiscard]] FeromoneMap Cell::get_feromone_map() const {
    return feromones_;
}

RenderState Cell::get_render_state() const {
    RenderState state;
    state.position = Vec2{0, 0};
    state.color = Color{0, 0, 0}; //  Black
    state.size = 5;               //  size
    state.payload = CellVisualData{temperature_,
                                   elevation_,
                                   humidity_,
                                   organics_.h2o > water_threshold, // water presence based on threshold
                                   static_cast<double>(feromones_.at(Feromone_t::DANGER_FEROMONE)) / max_feromones,
                                   static_cast<double>(feromones_.at(Feromone_t::FOOD_FEROMONE)) / max_feromones};
    state.shape = RenderShape::Circle; //  shape

    return state;
}

void Cell::decay_feromones() {
    const double decay_rate = 0.97; // Decay rate per update
    for (auto& [type, value] : feromones_) {
        value = static_cast<int>(value * decay_rate);
        if (value < 1) {
            value = 0; // Threshold to zero
        }
    }
}

void Cell::update() {
    decay_feromones();
}
#include "cell.h"

#include "common.h"
#include "random_utility.h"
#include "render_state.h"

#include <algorithm>

Cell::Cell(std::weak_ptr<IEntity> occupant)
    : occupant_(std::move(occupant)), temperature_(20.0f), humidity_(50.0f), elevation_(0.0f)

{
    // Initialize organics with random values
    RandomUtility rand_util;
    organics_ = OrganicsT{
        static_cast<unsigned>(rand_util.rnd_int(0, static_cast<int>(MaxC6h12o6))),
        static_cast<unsigned>(rand_util.rnd_int(0, static_cast<int>(MaxLipids))),
        0,
        0,
        static_cast<unsigned>(rand_util.rnd_int(0, static_cast<int>(MaxH2o))),
        0,
        static_cast<unsigned>(rand_util.rnd_int(0, static_cast<int>(MaxCaco3))),
    };
    // Initialize feromones to zero
    feromones_[FeromoneT::FOOD_FEROMONE] = 0;
    feromones_[FeromoneT::DANGER_FEROMONE] = 0;
    feromones_[FeromoneT::MATE_FEROMONE] = 0;
    feromones_[FeromoneT::HOME_FEROMONE] = 0;
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

void Cell::set_feromone(FeromoneT type, int value) {
    feromones_[type] += value;

    if (feromones_[type] < 0) {
        feromones_[type] = 0;
    }
    if (feromones_[type] > (MaxFeromones)) {
        feromones_[type] = (MaxFeromones);
    }
}

[[nodiscard]] FeromoneMapT Cell::get_feromone_map() const {
    return feromones_;
}

unsigned Cell::get_glucose() const {
    return organics_.c6h12o6;
}
unsigned Cell::get_water() const {
    return organics_.h2o;
}
unsigned Cell::get_calcium() const {
    return organics_.caco3;
}
unsigned Cell::get_carbon() const {
    return organics_.lipids;
}

unsigned Cell::take_glucose(unsigned amount) {
    const unsigned taken = std::min(amount, organics_.c6h12o6);
    organics_.c6h12o6 -= taken;
    return taken;
}

unsigned Cell::take_water(unsigned amount) {
    const unsigned taken = std::min(amount, organics_.h2o);
    organics_.h2o -= taken;
    return taken;
}

unsigned Cell::take_calcium(unsigned amount) {
    const unsigned taken = std::min(amount, organics_.caco3);
    organics_.caco3 -= taken;
    return taken;
}

RenderState Cell::get_render_state() const {
    RenderState state;
    state.position = Vec2{0, 0};
    state.color = Color{0, 0, 0}; //  Black
    state.size = 5;               //  size
    state.payload = CellVisualData{temperature_,
                                   elevation_,
                                   humidity_,
                                   organics_.h2o > WaterThreshold, // water presence based on threshold
                                   static_cast<double>(feromones_.at(FeromoneT::DANGER_FEROMONE)) / MaxFeromones,
                                   static_cast<double>(feromones_.at(FeromoneT::FOOD_FEROMONE)) / MaxFeromones};
    state.shape = RenderShape::Circle; //  shape

    return state;
}

void Cell::decay_feromones() {
    for (auto& [type, value] : feromones_) {
        value = static_cast<int>(value * FeromoneDecayRate);
        if (value < 1) {
            value = 0; // Threshold to zero
        }
    }
}

void Cell::update() {
    decay_feromones();
}
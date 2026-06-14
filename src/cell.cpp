#include "cell.h"

#include "common.h"
#include "random_utility.h"
#include "render_state.h"

#include <algorithm>

Cell::Cell(std::weak_ptr<IEntity> occupant)
    : occupant_(std::move(occupant)), temperature_(20.0f), humidity_(50.0f), elevation_(0.0f), regen_tick_(0)

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

    // Randomize regen phase so cells do not all regenerate glucose at the same tick.
    regen_tick_ = static_cast<unsigned>(rand_util.rnd_int(0, static_cast<int>(GlucoseRegenInterval) - 1));
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

void Cell::set_glucose(unsigned int glucose) {
    organics_.c6h12o6 = std::min(glucose, MaxC6h12o6);
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

void Cell::set_feromone(FeromoneT type, float value) {
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

unsigned int Cell::get_glucose() const {
    return organics_.c6h12o6;
}
unsigned int Cell::get_water() const {
    return organics_.h2o;
}
unsigned int Cell::get_calcium() const {
    return organics_.caco3;
}
unsigned int Cell::get_carbon() const {
    return organics_.lipids;
}

unsigned int Cell::take_glucose(unsigned int amount) {
    const unsigned int taken = std::min(amount, organics_.c6h12o6);
    organics_.c6h12o6 -= taken;
    if (organics_.c6h12o6 > MaxC6h12o6) {
        organics_.c6h12o6 = MaxC6h12o6; // Ensure we don't go negative
    }
    return taken;
}

unsigned int Cell::give_glucose(unsigned int amount) {
    const unsigned int given = std::min(amount, organics_.c6h12o6);
    organics_.c6h12o6 += given;
    if (organics_.c6h12o6 > MaxC6h12o6) {
        organics_.c6h12o6 = MaxC6h12o6; // Ensure we don't exceed max capacity
    }
    return given;
}

unsigned int Cell::take_water(unsigned int amount) {
    const unsigned int taken = std::min(amount, organics_.h2o);
    organics_.h2o -= taken;
    return taken;
}

unsigned int Cell::give_water(unsigned int amount) {
    organics_.h2o = std::min(organics_.h2o + amount, MaxH2o);
    return amount;
}

unsigned int Cell::take_calcium(unsigned int amount) {
    const unsigned int taken = std::min(amount, organics_.caco3);
    organics_.caco3 -= taken;
    return taken;
}

unsigned int Cell::give_calcium(unsigned int amount) {
    organics_.caco3 = std::min(organics_.caco3 + amount, MaxCaco3);
    return amount;
}

unsigned int Cell::take_o2(unsigned int amount) {
    const unsigned int taken = std::min(amount, organics_.o2);
    organics_.o2 -= taken;
    return taken;
}

unsigned int Cell::give_o2(unsigned int amount) {
    organics_.o2 = std::min(organics_.o2 + amount, MaxO2);
    return amount;
}

unsigned int Cell::take_co2(unsigned int amount) {
    const unsigned int taken = std::min(amount, organics_.co2);
    organics_.co2 -= taken;
    return taken;
}

unsigned int Cell::give_co2(unsigned int amount) {
    organics_.co2 = std::min(organics_.co2 + amount, MaxCo2);
    return amount;
}

unsigned int Cell::give_lipids(unsigned int amount) {
    organics_.lipids = std::min(organics_.lipids + amount, MaxLipids);
    return amount;
}

unsigned int Cell::take_lipids(unsigned int amount) {
    const unsigned int taken = std::min(amount, organics_.lipids);
    organics_.lipids -= taken;
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
                                   static_cast<double>(organics_.c6h12o6) / MaxC6h12o6,
                                   static_cast<double>(feromones_.at(FeromoneT::DANGER_FEROMONE)) / MaxFeromones,
                                   static_cast<double>(feromones_.at(FeromoneT::FOOD_FEROMONE)) / MaxFeromones,
                                   static_cast<double>(feromones_.at(FeromoneT::MATE_FEROMONE)) / MaxFeromones,
                                   static_cast<double>(feromones_.at(FeromoneT::HOME_FEROMONE)) / MaxFeromones};
    state.shape = RenderShape::Circle; //  shape

    return state;
}

void Cell::decay_feromones() {
    for (auto& [type, value] : feromones_) {
        value = static_cast<float>(value * FeromoneDecayRate);
        if (value < 0.01f) {
            value = 0.0f; // Threshold to zero
        }
    }
}

void Cell::regen_glucose() {
    if (++regen_tick_ >= GlucoseRegenInterval) {
        regen_tick_ = 0;
        organics_.c6h12o6 = std::min(organics_.c6h12o6 + GlucoseRegenAmount, MaxC6h12o6);
    }
}

void Cell::update() {
    decay_feromones();
    regen_glucose();
}
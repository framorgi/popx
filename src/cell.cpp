#include "cell.h"

Cell::Cell(std::weak_ptr<IEntity> occupant)
    : occupant_(std::move(occupant)), temperature_(20.0f), humidity_(50.0f), elevation_(0.0f)

{}

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
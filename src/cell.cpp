#include "cell.h"

Cell::Cell(std::weak_ptr<IEntity> occupant) : occupant_(std::move(occupant)) {}

void Cell::set_occupant(std::weak_ptr<IEntity> occupant) {
    occupant_ = std::move(occupant);
}

[[nodiscard]] std::weak_ptr<IEntity> Cell::get_occupant() const {
    return occupant_;
}
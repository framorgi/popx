#pragma once

#include "i_entity.h"
#include "i_slice.h"

#include <memory>

class Cell : ICell {
  public:
    Cell(std::weak_ptr<IEntity> occupant = {}) : occupant_(std::move(occupant)) {}

    ~Cell() = default;
    void set_occupant(std::weak_ptr<IEntity> occupant) {
        occupant_ = std::move(occupant);
    }
    [[nodiscard]] std::weak_ptr<IEntity> get_occupant() const {
        return occupant_;
    }

  private:
    std::weak_ptr<IEntity> occupant_; // Weak pointer - observes but doesn't own the entity - An empty weak_ptr means no occupant

};

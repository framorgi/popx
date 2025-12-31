#pragma once

#include "i_entity.h"
#include "i_slice.h"

#include <memory>

class Slice : ISlice {
  public:
    Slice(std::weak_ptr<IEntity> occupant = std::weak_ptr<IEntity>()) : occupant_(occupant) {}

    ~Slice() = default;
    void set_occupant(std::weak_ptr<IEntity> occupant) {
        occupant_ = occupant;
    }
    [[nodiscard]] std::weak_ptr<IEntity> get_occupant() const {
        return occupant_;
    }

  private:
    std::weak_ptr<IEntity> occupant_; // Weak pointer - observes but doesn't own the entity
};

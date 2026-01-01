#pragma once

#include "i_cell.h"
#include "i_entity.h"

#include <memory>

class Cell : public ICell {
  public:
    Cell(std::weak_ptr<IEntity> occupant = {});

    ~Cell() = default;
    void set_occupant(std::weak_ptr<IEntity> occupant) override;

    [[nodiscard]] std::weak_ptr<IEntity> get_occupant() const override;

  private:
    std::weak_ptr<IEntity>
        occupant_; // Weak pointer - observes but doesn't own the entity - An empty weak_ptr means no occupant
};

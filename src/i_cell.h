#pragma once

#include <memory>

class IEntity; // Forward declaration - avoids including full header

class ICell {
  private:
    /* data */
  public:
    ~ICell() = default;
    virtual void set_occupant(std::weak_ptr<IEntity> occupant) = 0;

    [[nodiscard]] virtual std::weak_ptr<IEntity> get_occupant() const = 0;
};

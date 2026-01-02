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

    virtual void set_temperature(double temperature) = 0;
    [[nodiscard]] virtual double get_temperature() const = 0;

    virtual void set_elevation(double elevation) = 0;
    [[nodiscard]] virtual double get_elevation() const = 0;

    virtual void set_humidity(double humidity) = 0;
    [[nodiscard]] virtual double get_humidity() const = 0;
};

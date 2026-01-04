#pragma once

#include "i_graphic_engine.h"
#include "i_renderer.h"

#include <memory>

class IEntity; // Forward declaration - avoids including full header
class ICell {
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

    [[nodiscard]] virtual bool need_rendering() const = 0;
    virtual void reset_need_rendering() = 0;

  protected:
    ///--------------------------------------------------------------------------
    /// @brief    Flag indicating if the cell needs rendering
    ///--------------------------------------------------------------------------
    bool need_rendering_ = true;
};
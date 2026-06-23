#pragma once

#include "common.h"
#include "i_graphic_engine.h"
#include "i_renderer.h"
#include "render_state.h"

#include <memory>

class IEntity; // Forward declaration - avoids including full header
class ICell {
  public:
    ~ICell() = default;

    [[nodiscard]] virtual RenderState get_render_state() const = 0;

    virtual void set_occupant(std::weak_ptr<IEntity> occupant) = 0;

    [[nodiscard]] virtual std::weak_ptr<IEntity> get_occupant() const = 0;

    virtual void set_temperature(double temperature) = 0;
    [[nodiscard]] virtual double get_temperature() const = 0;

    virtual void set_elevation(double elevation) = 0;
    [[nodiscard]] virtual double get_elevation() const = 0;

    virtual void set_glucose(unsigned glucose) = 0;

    virtual void set_feromone(FeromoneT type, float value) = 0;
    [[nodiscard]] virtual FeromoneMapT get_feromone_map() const = 0;

    // --- Chemical resources ---------------------------------------------------
    [[nodiscard]] virtual unsigned get_glucose() const = 0;

    virtual void set_water(unsigned water) = 0;
    [[nodiscard]] virtual unsigned get_water() const = 0;
    [[nodiscard]] virtual unsigned get_calcium() const = 0;
    [[nodiscard]] virtual unsigned get_carbon() const = 0;
    /// Remove up to `amount` units; returns the quantity actually taken.
    virtual unsigned take_glucose(unsigned amount) = 0;
    virtual unsigned give_glucose(unsigned amount) = 0;
    virtual unsigned take_water(unsigned amount) = 0;
    virtual unsigned give_water(unsigned amount) = 0;
    virtual unsigned take_calcium(unsigned amount) = 0;
    virtual unsigned give_calcium(unsigned amount) = 0;
    virtual unsigned take_o2(unsigned amount) = 0;
    virtual unsigned give_o2(unsigned amount) = 0;
    virtual unsigned take_co2(unsigned amount) = 0;
    virtual unsigned give_co2(unsigned amount) = 0;
    virtual unsigned take_lipids(unsigned amount) = 0;
    virtual unsigned give_lipids(unsigned amount) = 0;

    /// @brief Return all organic compounds currently in this cell (read-only aggregate).
    [[nodiscard]] virtual OrganicsT get_organics() const = 0;

    virtual void update() = 0;
};
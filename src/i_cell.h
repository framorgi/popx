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

    virtual void set_humidity(double humidity) = 0;
    [[nodiscard]] virtual double get_humidity() const = 0;

    virtual void set_feromone(Feromone_t type, int value) = 0;
    [[nodiscard]] virtual FeromoneMap get_feromone_map() const = 0;

    virtual void update() = 0;
};
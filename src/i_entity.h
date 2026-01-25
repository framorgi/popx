#pragma once
#include "common.h"
#include "i_world.h"
#include "render_state.h"

#include <memory>

///----------------------------------------------------------------------
/// @brief    IEntity interface that defines the methods for entities in the simulation
/// An Entity can be any object that exists in the world, such as agents, obstacles or anything else
// that can be moved and occupy space in the world.
///----------------------------------------------------------------------
class IEntity {
  public:
    ~IEntity() = default;

    /// @brief Get the render state of the entity
    [[nodiscard]]
    virtual RenderState get_render_state() const = 0;

    /// @brief Initialize the entity
    /// @param world The world in which the entity exists
    /// @note This method is called once when the entity is created and set the Position  in the world
    virtual bool try_spawn(PositionT p) = 0;

    /// @brief entity can have an update method to update its state
    virtual void update() = 0;

    /// @brief Terminate the entity
    virtual void despawn() = 0;

    /// @brief Get the entity's PositionT
    [[nodiscard]] virtual PositionT get_position() const = 0;

    /// @brief Set the agent's PositionT
    virtual bool try_move(PositionT p) = 0;
};

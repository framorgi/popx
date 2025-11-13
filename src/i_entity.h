#pragma once
#include <memory>
#include "common.h"
#include "i_world.h"
 
class i_entity
{

public:

    ~i_entity() = default;

    /// @brief Initialize the entity
    /// @param world The world in which the entity exists
    /// @note This method is called once when the entity is created and set the position in the world
    virtual bool try_spawn(std::shared_ptr<i_world> world, Position p)=0;

    /// @brief entity can have an update method to update its state
    virtual void update(std::shared_ptr<i_world> world)=0;

    /// @brief Terminate the entity
    virtual void despawn(std::shared_ptr<i_world> world)=0;

    /// @brief Get the entity's position
    virtual Position get_position() const = 0;

    
    /// @brief Set the agent's position
    virtual bool try_move(std::shared_ptr<i_world> world, Position p)=0;


};

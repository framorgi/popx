#pragma once
#include "common.h"
#include <memory>

class i_entity;

class i_world
{
public:
    virtual ~i_world() = default;

    /// @brief Initialize the world
    virtual void init()=0;

    /// @brief Check if position is free
    virtual bool is_free(Position p) const = 0;

    /// @brief Add an entity to the world at entity position
    virtual bool add_entity(std::shared_ptr<i_entity> entity)=0;

    /// @brief Remove an entity from the world
    /// @param entity The entity to remove
    /// @return True if the entity was removed successfully, false otherwise
    virtual bool remove_entity(std::shared_ptr<i_entity> entity)=0; 

    /// @brief Move an entity to a new position
    /// @param entity The entity to move
    /// @param new_pos The new position for the entity
    /// @return True if the entity was moved successfully, false otherwise
    virtual bool move_entity(std::shared_ptr<i_entity> entity, Position new_pos)=0;

    virtual bool update_cycle()=0;


    virtual int get_width() const = 0;

    virtual int get_height() const = 0;
 
};
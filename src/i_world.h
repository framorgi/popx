#pragma once
#include "common.h"

#include <memory>

class IEntity;

class IWorld {
  public:
    virtual ~IWorld() = default;
    ///----------------------------------------------------------------------
    /// @brief Initialize the world
    ///----------------------------------------------------------------------
    virtual void init() = 0;
    ///----------------------------------------------------------------------
    /// @brief Check if a position in the world is free (no entity)
    /// @param p The position to check
    /// @return True if the position is free, false otherwise
    ///----------------------------------------------------------------------
    [[nodiscard]] virtual bool is_free(Position p) const = 0;

    /// @brief Add an entity to the world (World stores reference, does not take ownership)
    /// @param entity The entity to add
    /// @return True if the entity was added successfully, false otherwise
    /// @note The world stores only a reference to the entity. The entity must remain valid
    ///       and owned by the caller (typically AgentsManager) for the lifetime of its presence in the world.
    ///----------------------------------------------------------------------
    virtual bool add_entity(std::shared_ptr<IEntity> entity) = 0;
    ///----------------------------------------------------------------------
    /// @brief Remove an entity from the world (only removes reference, doesn't destroy entity)
    /// @param entity The entity to remove
    /// @return True if the entity was removed successfully, false otherwise
    /// @note This only removes the entity reference from the world. The entity continues to exist
    ///       if owned by another component (e.g., AgentsManager).
    ///----------------------------------------------------------------------
    virtual bool remove_entity(std::shared_ptr<IEntity> entity) = 0;
    ///----------------------------------------------------------------------
    /// @brief Move an entity to a new position
    /// @param entity The entity to move
    /// @param new_pos The new position for the entity
    /// @return True if the entity was moved successfully, false otherwise
    ///----------------------------------------------------------------------
    virtual bool move_entity(std::shared_ptr<IEntity> entity, Position new_pos) = 0;

    ///----------------------------------------------------------------------
    /// @brief Update the world for a simulation cycle
    /// @return True if the update was successful, false otherwise
    ///----------------------------------------------------------------------

    virtual bool update_cycle() = 0;

    ///----------------------------------------------------------------------
    /// @brief Get the width of the world
    /// @return The width of the world
    ///----------------------------------------------------------------------
    [[nodiscard]] virtual int get_width() const = 0;

    ///----------------------------------------------------------------------
    /// @brief Get the height of the world
    /// @return The height of the world
    ///----------------------------------------------------------------------
    [[nodiscard]] virtual int get_height() const = 0;
};
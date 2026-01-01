#pragma once
#include "i_cell.h"
#include "i_entity.h"
#include "i_logger.h"
#include "i_world.h"

#include <memory>
#include <vector>

///--------------------------------------------------------------------------
/// @brief    GridWorld class that implements the IWorld interface and its methods
///--------------------------------------------------------------------------

class GridWorld : public IWorld {
  public:
    ///----------------------------------------------------------------------
    /// @brief    Constructor of GridWorld
    /// @param    w Width of the grid world
    /// @param    h Height of the grid world
    /// @param    logger Shared pointer to the Logger instance
    ///----------------------------------------------------------------------
    GridWorld(int w, int h, std::shared_ptr<ILogger> logger = nullptr);

    ///----------------------------------------------------------------------
    /// @brief    Initialization of  the grid world
    ///----------------------------------------------------------------------

    void init() override;

    ///----------------------------------------------------------------------
    /// @brief    Check if a position in the world is free (no entity)
    /// @param    p The position to check
    /// @return   True if the position is free, false otherwise
    ///----------------------------------------------------------------------
    [[nodiscard]] bool is_free(Position p) const override;
    ///----------------------------------------------------------------------
    /// @brief    Move an entity to a new position
    /// @param    entity The entity to move
    /// @param    new_pos The new position for the entity
    /// @return   True if the entity was moved successfully, false otherwise
    ///----------------------------------------------------------------------
    bool move_entity(std::shared_ptr<IEntity> entity, Position new_pos) override;

    ///----------------------------------------------------------------------
    /// @brief    Remove an entity from the world
    /// @param    e The entity to remove
    /// @return   True if the entity was removed successfully, false otherwise
    ///----------------------------------------------------------------------
    bool remove_entity(std::shared_ptr<IEntity> e) override;
    ///----------------------------------------------------------------------
    /// @brief    Add an entity to the world
    /// @param    e The entity to add
    /// @return   True if the entity was added successfully, false otherwise
    ///----------------------------------------------------------------------
    bool add_entity(std::shared_ptr<IEntity> e) override;
    ///----------------------------------------------------------------------
    /// @brief    Update cycle for the world
    /// @return   True if the update cycle was successful, false otherwise
    ///----------------------------------------------------------------------
    bool update_cycle() override;
    ///----------------------------------------------------------------------
    /// @brief    Get the width of the world
    /// @return   Width of the world
    ///----------------------------------------------------------------------
    [[nodiscard]] int get_width() const override {
        return width_;
    }
    ///----------------------------------------------------------------------
    /// @brief    Get the height of the world
    /// @return   Height of the world
    ///----------------------------------------------------------------------
    [[nodiscard]] int get_height() const override {
        return height_;
    }
    ///----------------------------------------------------------------------
    /// @brief    Get the grid cells of the world
    /// @return     vector of shared pointers to ICell representing the grid
    ///----------------------------------------------------------------------
    [[nodiscard]] std::vector<std::shared_ptr<ICell>> get_slices() const {
        return cells_;
    }

  private:
    ///----------------------------------------------------------------------
    /// @brief    Check if coordinates are within the bounds of the grid
    /// @param    x The x-coordinate
    /// @param    y The y-coordinate
    /// @return   True if the coordinates are within bounds, false otherwise
    ///----------------------------------------------------------------------
    [[nodiscard]]
    bool in_bounds(int x, int y) const;

    ///----------------------------------------------------------------------
    /// @brief    Convert 2D coordinates to a 1D index
    /// @param    x The x-coordinate
    /// @param    y The y-coordinate
    /// @return   The corresponding 1D index
    ///----------------------------------------------------------------------
    [[nodiscard]]
    int index(int x, int y) const;

    ///----------------------------------------------------------------------
    /// @brief    Width and height of the grid world
    ///----------------------------------------------------------------------
    int width_, height_;

    ///----------------------------------------------------------------------
    /// @brief      vector representing the grid cells of the world
    ///----------------------------------------------------------------------
    std::vector<std::shared_ptr<ICell>> cells_;
    ///----------------------------------------------------------------------
    /// @brief    Shared pointer to the Logger instance
    ///----------------------------------------------------------------------
    std::shared_ptr<ILogger> logger_;
};
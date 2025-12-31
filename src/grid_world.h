#pragma once
#include "i_entity.h"
#include "i_logger.h"
#include "i_world.h"
#include "slice.h"

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
    bool remove_entity(std::shared_ptr<IEntity> e) override;
    bool add_entity(std::shared_ptr<IEntity> e) override;
    bool update_cycle() override;

    int get_width() const override {
        return width_;
    }
    int get_height() const override {
        return height_;
    }

    std::vector<std::shared_ptr<Slice>> get_slices() const {
        return slices_;
    }

  private:
    bool in_bounds(int x, int y) const;

    int index(int x, int y) const;

    int width_, height_;
    std::vector<std::shared_ptr<Slice>> slices_;
    std::shared_ptr<ILogger> logger_;
};
#pragma once
#include "grid_world.h"
#include "i_renderer.h"
#include "sfml_graphic_engine.h"

///--------------------------------------------------------------------------
/// @brief    Renderer class that implements the IRenderer interface and its methods and wraps around a graphic engine
///--------------------------------------------------------------------------
class Renderer : public IRenderer {
  public:
    ///--------------------------------------------------------------------------
    /// @brief    Constructor of Renderer
    /// @param    gfx Shared pointer to the SfmlGraphicEngine instance
    /// @param    world Shared pointer to the GridWorld instance
    ///--------------------------------------------------------------------------
    // TODO: check if we need to pass the implemented objects or the interfaces
    Renderer(std::shared_ptr<SfmlGraphicEngine> gfx, std::shared_ptr<GridWorld> world);
    ///--------------------------------------------------------------------------
    /// @brief    Initializes the renderer
    ///--------------------------------------------------------------------------
    void init() override;
    ///--------------------------------------------------------------------------
    /// @brief    Draws the current frame
    ///--------------------------------------------------------------------------
    void draw() override;
    ///--------------------------------------------------------------------------
    /// @brief    Saves the current frame to a file
    ///--------------------------------------------------------------------------
    void save_frame() override;

  private:
    ///--------------------------------------------------------------------------
    /// @brief    Shared pointer to the graphic engine  instance
    ///--------------------------------------------------------------------------
    std::shared_ptr<SfmlGraphicEngine> gfx_;

    ///--------------------------------------------------------------------------
    /// @brief    Shared pointer to the world instance
    ///--------------------------------------------------------------------------
    std::shared_ptr<GridWorld> world_;
    ///--------------------------------------------------------------------------
    /// @brief    Draws the simulation world
    ///--------------------------------------------------------------------------
    void draw_world();

    ///--------------------------------------------------------------------------
    /// @brief    Draws a single entity
    /// @param    entity Pointer to the entity to be drawn
    ///--------------------------------------------------------------------------
    void draw_entity(IEntity* entity);
};

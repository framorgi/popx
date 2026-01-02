#pragma once
#include <memory>

constexpr int cell_render_size = 10; // Size of each cell in pixels when rendering

///--------------------------------------------------------------------------
/// @brief    Interface for Renderer who is responsible for rendering the simulation.Renderer is a wrapper around a
/// graphic engine
///--------------------------------------------------------------------------

class IRenderer {
  public:
    ////-------------------------------------------------------------------------
    /// @brief    Virtual destructor
    ///--------------------------------------------------------------------------
    virtual ~IRenderer() = default;

    ///--------------------------------------------------------------------------
    /// @brief    Initializes the renderer
    ///--------------------------------------------------------------------------

    virtual void init() = 0;
    ///--------------------------------------------------------------------------
    /// @brief    Draws the current frame
    ///--------------------------------------------------------------------------

    virtual void draw() = 0;

    ///--------------------------------------------------------------------------
    /// @brief    Saves the current frame to a file
    ///--------------------------------------------------------------------------
    virtual void save_frame() = 0;
};

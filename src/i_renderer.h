#pragma once
#include <memory>

constexpr int cell_render_size = 6; // Size of each cell in pixels when rendering
constexpr int SEGMENTS = 16;        // Number of segments to approximate a circle for entity rendering
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

    ///--------------------------------------------------------------------------
    /// @brief    Checks if the rendering window is open
    /// @return   True if the window is open, false otherwise
    ///--------------------------------------------------------------------------
    [[nodiscard]] virtual bool window_open() const = 0;
    ///--------------------------------------------------------------------------
    /// @brief    Polls for window events
    ///--------------------------------------------------------------------------
    virtual void poll_event() = 0;
};

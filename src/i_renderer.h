#pragma once
#include "render_flags.h"

#include <memory>

constexpr int cell_render_size = 8; // Size of each cell in pixels when rendering
constexpr int SEGMENTS = 11; // was 16;         // Number of segments to approximate a circle for entity rendering
constexpr int FEROMONE_SEGMENTS = 5; // Number of segments to approximate a circle for feromone rendering
constexpr int GLUCOSE_SEGMENTS = 3;  // Number of segments to approximate a circle for glucose rendering
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

    ///--------------------------------------------------------------------------
    /// @brief    Apply render layer flags (which layers are visible)
    ///--------------------------------------------------------------------------
    virtual void set_render_flags(const RenderFlags& flags) = 0;

    ///--------------------------------------------------------------------------
    /// @brief    Initialise Dear ImGui (call once after the window is ready).
    ///           Delegated to the underlying IGraphicEngine.
    ///--------------------------------------------------------------------------
    virtual void imgui_init() = 0;

    ///--------------------------------------------------------------------------
    /// @brief    Present the finished frame to the screen.
    ///           Must be called AFTER gui->render() so ImGui draws appear on top.
    ///--------------------------------------------------------------------------
    virtual void present() = 0;
};

#pragma once

///--------------------------------------------------------------------------
/// @brief Pure interface for the GUI layer.
///        Responsible for building and rendering the ImGui overlay.
///        The GUI layer is completely separate from the simulation renderer.
///--------------------------------------------------------------------------
class IGui {
  public:
    virtual ~IGui() = default;

    /// @brief One-time initialisation (must be called after IGraphicEngine::imgui_init).
    virtual void init() = 0;

    /// @brief Build the ImGui frame for this tick.
    ///        Call BEFORE the simulation renderer draws, but AFTER imgui_new_frame.
    ///        @param dt elapsed seconds since last frame (used by imgui-sfml internally).
    virtual void update(float dt) = 0;

    /// @brief Submit the ImGui draw commands to the graphics engine.
    ///        Call AFTER the simulation renderer has drawn its content and
    ///        BEFORE the final window present.
    virtual void render() = 0;

    /// @brief Release ImGui resources.
    virtual void shutdown() = 0;

    /// @brief Notify the GUI that the simulation has ended
    ///        (all pops dead or stop requested). The GUI will then display
    ///        the end-of-simulation summary modal.
    virtual void notify_end_of_simulation() = 0;
};

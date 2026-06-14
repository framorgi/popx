#pragma once
#include "i_agents_manager.h"
#include "i_graphic_engine.h"
#include "i_gui.h"
#include "i_stats.h"
#include "pop_snapshot.h"
#include "render_flags.h"
#include "sim_control.h"

#include <memory>
#include <string>
#include <vector>

///--------------------------------------------------------------------------
/// @brief Concrete ImGui GUI layer.
///
///  Lifecycle expected by the app loop:
///    1. imgui_init() already called on IGraphicEngine
///    2. Each frame: update(dt)  → builds the ImGui frame
///    3. Each frame: render()    → submits ImGui draw commands
///    4. At shutdown: shutdown() → nothing extra needed (ImGui::SFML::Shutdown
///       is called by SfmlGraphicEngine destructor)
///--------------------------------------------------------------------------
class ImGuiLayer : public IGui {
  public:
    ///------------------------------------------------------------------------
    /// @param gfx         Graphics engine (used to call imgui_new_frame / imgui_render)
    /// @param stats       Stats collector (read-only access from GUI)
    /// @param agents      Agents manager (read-only: alive count + snapshots)
    /// @param sim_control Shared control struct written by GUI, read by app loop
    /// @param render_flags Shared flags written by GUI, read by renderer
    ///------------------------------------------------------------------------
    ImGuiLayer(IGraphicEngine& gfx, IStats& stats, IAgentsManager& agents, SimControl& sim_control,
               RenderFlags& render_flags);

    void init() override;
    void update(float dt) override;
    void render() override;
    void shutdown() override;
    void notify_end_of_simulation() override;

  private:
    IGraphicEngine& gfx_;
    IStats& stats_;
    IAgentsManager& agents_;
    SimControl& sim_control_;
    RenderFlags& render_flags_;

    // Sub-window visibility toggles
    bool show_pop_inspector_ = false;
    bool show_charts_ = false;

    // Pop Inspector state
    int pop_idx_ = 0;
    std::string selected_pop_id_;
    std::vector<PopSnapshot> snap_cache_;
    bool snap_dirty_ = true;

    // End-of-simulation state
    bool end_of_sim_ = false;
    bool stats_saved_ = false;
    std::string saved_path_;

    // Sub-window builders (called from update())
    void build_control_panel();
    void build_stats_overlay();
    void build_pop_inspector();
    void build_charts();
    void build_end_modal();
};

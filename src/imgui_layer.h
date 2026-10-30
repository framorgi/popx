#pragma once
#include "i_agents_manager.h"
#include "i_graphic_engine.h"
#include "i_gui.h"
#include "render_flags.h"
#include "sim_control.h"
#include "stats.h"

#include <functional>
#include <vector>

/// @brief Sidebar composed of independently registered widgets.
class ImGuiLayer final : public IGui {
  public:
    ImGuiLayer(IGraphicEngine& gfx, Stats& stats, IAgentsManager& agents, SimControl& controls, RenderFlags& flags);
    void init() override;
    void update(float dt) override;
    void render() override;
    void shutdown() override {}
    void notify_end_of_simulation() override {}
    void add_widget(std::string title, std::function<void()> draw);

  private:
    void draw_world();
    void draw_inspector();
    void draw_sandbox();
    void draw_brain();
    IGraphicEngine& gfx_;
    nlohmann::json draft_, last_runtime_, edge_, appearance_draft_, last_appearance_;
    std::string observed_id_, theme_;
    std::vector<std::vector<float>> average_, peaks_;
    uint64_t observed_tick_ = 0;
    bool monitor_open_ = false, aggregate_ = false;
    float min_weight_ = .01f;
    int selected_gene_ = -1;
    Stats& stats_;
    IAgentsManager& agents_;
    SimControl& controls_;
    RenderFlags& flags_;
    std::vector<std::pair<std::string, std::function<void()>>> widgets_;
};

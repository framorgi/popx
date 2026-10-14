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
    IGraphicEngine& gfx_;
    Stats& stats_;
    IAgentsManager& agents_;
    SimControl& controls_;
    RenderFlags& flags_;
    std::vector<std::pair<std::string, std::function<void()>>> widgets_;
};

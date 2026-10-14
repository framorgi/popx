#pragma once
#include "config.h"
#include "imgui_layer.h"
#include "pops_manager.h"
#include "renderer.h"
#include "sim_control.h"
#include "simulator.h"
#include "stats.h"

#include <map>

struct AppOptions {
    std::string config_path;
    unsigned udp_port = 8001, speed = 1;
    bool headless = false;
};
/// @brief Owns process/window lifetime independently of a simulation run.
class PopXApp {
  public:
    explicit PopXApp(AppOptions options);
    int run();

  private:
    void start();
    void show_gui();
    void hide_gui();
    void dispatch(nlohmann::json command);
    void finish_if_needed();
    nlohmann::json status() const;
    AppOptions options_;
    std::shared_ptr<ILogger> logger_;
    std::shared_ptr<Config> config_;
    std::shared_ptr<GridWorld> world_;
    std::shared_ptr<PopsManager> agents_;
    std::shared_ptr<Simulator> sim_;
    std::shared_ptr<Stats> stats_;
    std::shared_ptr<SfmlGraphicEngine> gfx_;
    std::unique_ptr<Renderer> renderer_;
    std::unique_ptr<ImGuiLayer> gui_;
    SimControl control_;
    RenderFlags flags_;
    bool quit_ = false, finished_recorded_ = false, reset_clock_ = false;
    std::map<std::string, nlohmann::json> replies_;
};

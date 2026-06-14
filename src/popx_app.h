#pragma once
#include "console_logger.h"
#include "i_agents_manager.h"
#include "i_app.h"
#include "i_gui.h"
#include "i_stats.h"
#include "i_world.h"
#include "render_flags.h"
#include "renderer.h"
#include "sim_control.h"
#include "simulator.h"

#include <filesystem>
#include <memory>
#include <string>

///--------------------------------------------------------------------------
/// @brief Application class that implements the IApp interface and its methods
///--------------------------------------------------------------------------
class PopXApp : public IApp {
  public:
    PopXApp(std::shared_ptr<ISimulator> sim, std::shared_ptr<IRenderer> renderer, std::shared_ptr<ILogger> logger,
            std::shared_ptr<IStats> stats, std::shared_ptr<IAgentsManager> agents, std::shared_ptr<IWorld> world);

    void init() override;
    void run() override;
    void stop() override;

    /// @brief Inject the GUI layer (call before init()).
    void set_gui(std::shared_ptr<IGui> gui) {
        gui_ = std::move(gui);
    }

    /// @brief Provide access to the control/flag structs so that the GUI layer
    ///        (constructed in main.cpp) can bind references to them.
    SimControl& get_sim_control() {
        return sim_control_;
    }
    RenderFlags& get_render_flags() {
        return render_flags_;
    }

  private:
    std::shared_ptr<ISimulator> sim_;
    std::shared_ptr<IRenderer> renderer_;
    std::shared_ptr<ILogger> logger_;
    std::shared_ptr<IStats> stats_;
    std::shared_ptr<IAgentsManager> agents_;
    std::shared_ptr<IWorld> world_;
    std::shared_ptr<IGui> gui_;

    SimControl sim_control_;
    RenderFlags render_flags_;

    bool running_token_ = false;
    bool end_of_sim_ = false;
    uint64_t tick_count_ = 0;

    /// @brief Derive a timestamped output path and call stats_->save_to_json().
    [[nodiscard]] bool save_stats();
};
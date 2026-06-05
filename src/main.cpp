

#include "common.h"
#include "config.h"
#include "console_logger.h"
#include "grid_world.h"
#include "pops_manager.h"
#include "popx_app.h"
#include "sfml_graphic_engine.h"

#include <filesystem>
#include <memory>
int main(int /*argc*/, char* argv[]) {
    /// Setup application components
    /// ------------------------------------------------------------------------

    // Resolve config path relative to the executable (works from any CWD).
    // argv[0]: .../build-debug/src/popx  →  ../../config/popx.ini
    const std::filesystem::path exe_dir = std::filesystem::weakly_canonical(argv[0]).parent_path();
    const std::string config_path = (exe_dir.parent_path().parent_path() / "config" / "popx.ini").string();

    // Create Config
    std::shared_ptr<Config> app_config = std::make_shared<Config>(config_path);

    // Create Logger
    std::shared_ptr<ConsoleLogger> app_logger = std::make_shared<ConsoleLogger>();
    app_logger->set_level(
        LogLevel::Info); // Set log level to Error (you can change this to Debug, Info, Warning as needed)
    // Create SimulationWorld
    std::shared_ptr<GridWorld> app_grid_simulation_world = std::make_shared<GridWorld>(MapSize, MapSize, app_logger);

    // Create Agents Manager. Constructor needs the SimulationWorld
    std::shared_ptr<PopsManager> app_agents_manager =
        std::make_shared<PopsManager>(app_grid_simulation_world, app_logger, app_config);

    // Create the main Simulator. Constructor needs the SimulationWorld and Agents Manager
    std::shared_ptr<Simulator> app_main_simulator =
        std::make_shared<Simulator>(app_grid_simulation_world, app_agents_manager, app_logger);

    // Create the SFML Graphic Engine
    std::shared_ptr<SfmlGraphicEngine> app_gfx = std::make_shared<SfmlGraphicEngine>(app_logger);

    // Create the Renderer. Constructor needs the Graphic Engine and the SimulationWorld
    std::shared_ptr<Renderer> app_renderer = std::make_shared<Renderer>(app_gfx, app_grid_simulation_world, app_config);

    // Create the main Application. Constructor needs the Simulator and Renderer
    PopXApp app(app_main_simulator, app_renderer, app_logger);

    /// Run the application
    /// ------------------------------------------------------------------------
    app.init();
    app.run();

    return 0;
}
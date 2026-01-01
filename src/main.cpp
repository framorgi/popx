

#include "grid_world.h"
#include "pops_manager.h"
#include "popx_app.h"
#include "sfml_graphic_engine.h"

#include <memory>


int main() {

    /// Setup application components
    /// ------------------------------------------------------------------------

    // Create Logger
    std::shared_ptr<ConsoleLogger> app_logger = std::make_shared<ConsoleLogger>();
    
    // Create SimulationWorld
    std::shared_ptr<GridWorld> app_grid_simulation_world = std::make_shared<GridWorld>(800, 800, app_logger);
    
    // Create Agents Manager. Constructor needs the SimulationWorld  
    std::shared_ptr<PopsManager> app_agents_manager = std::make_shared<PopsManager>(app_grid_simulation_world, app_logger);

    // Create the main Simulator. Constructor needs the SimulationWorld and Agents Manager
    std::shared_ptr<Simulator> app_main_simulator = std::make_shared<Simulator>(app_grid_simulation_world, app_agents_manager, app_logger);

    // Create the SFML Graphic Engine 
    std::shared_ptr<SfmlGraphicEngine> app_gfx = std::make_shared<SfmlGraphicEngine>();

    // Create the Renderer. Constructor needs the Graphic Engine and the SimulationWorld
    std::shared_ptr<Renderer> app_renderer = std::make_shared<Renderer>(app_gfx, app_grid_simulation_world);

    // Create the main Application. Constructor needs the Simulator and Renderer
    PopXApp app(app_main_simulator, app_renderer, app_logger);

    /// Run the application
    /// ------------------------------------------------------------------------
    app.init();
    app.run();

    return 0;
}
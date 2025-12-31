#pragma once
#include "grid_world.h"
#include "i_logger.h"
#include "i_simulator.h"
#include "pops_manager.h"

#include <memory>

///--------------------------------------------------------------------------
/// @brief    Simulator class that implements the ISimulator interface and its methods
///--------------------------------------------------------------------------
class Simulator : public ISimulator {
  public:
    ///--------------------------------------------------------------------------
    /// @brief    Constructor of Simulator
    /// @param    world Shared pointer to the GridWorld instance
    /// @param    agents_manager Shared pointer to the PopsManager instance
    /// @param    logger Shared pointer to the Logger instance
    ///--------------------------------------------------------------------------
    // TODO: check if we need to pass the implemented objects or the interfaces
    Simulator(std::shared_ptr<GridWorld> world, std::shared_ptr<PopsManager> agents_manager,
              std::shared_ptr<ILogger> logger);

    ///--------------------------------------------------------------------------
    /// @brief    Initializes the simulator
    ///--------------------------------------------------------------------------
    void init() override;
    ///--------------------------------------------------------------------------
    /// @brief    Updates the simulator state
    ///--------------------------------------------------------------------------
    void update() override;
    ///--------------------------------------------------------------------------
    /// @brief    Returns the shared pointer to the world
    ///--------------------------------------------------------------------------
    [[nodiscard]] std::shared_ptr<IWorld> get_world() const;

  private:
    bool update_agents();
    bool update_environment();

    std::shared_ptr<GridWorld> world_;
    std::shared_ptr<PopsManager> agents_manager_;
    std::shared_ptr<ILogger> logger_;
    // statistic handler
};
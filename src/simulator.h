#pragma once
#include "grid_world.h"
#include "i_agents_manager.h"
#include "i_logger.h"
#include "i_simulator.h"
#include "i_world.h"

#include <sys/types.h>

#include <memory>
#include <nlohmann/json.hpp>
#include <string>

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
    Simulator(std::shared_ptr<IWorld> world, std::shared_ptr<IAgentsManager> agents_manager,
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

    uint64_t tick() const {
        return tick_count_;
    }
    const std::string& state() const {
        return state_;
    }
    const std::string& reason() const {
        return reason_;
    }
    unsigned speed() const {
        return speed_;
    }
    void set_duration(double minutes) {
        duration_minutes_ = minutes;
    }
    void command(const std::string& name, const nlohmann::json& arguments);

  private:
    std::string state_ = "running", reason_;
    unsigned speed_ = 1;
    double duration_minutes_ = 0;

    bool update_agents();
    bool update_environment();

    std::shared_ptr<IWorld> world_;
    std::shared_ptr<IAgentsManager> agents_manager_;
    std::shared_ptr<ILogger> logger_;

    uint64_t tick_count_ = 0;
    // statistic handler
};
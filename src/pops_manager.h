#pragma once
#include "common.h"
#include "i_agents_manager.h"
#include "i_config.h"
#include "i_logger.h"
#include "pop.h"
#include "random_utility.h"

#include <memory>
#include <vector>
class PopsManager : public IAgentsManager {
  public:
    PopsManager(std::shared_ptr<IWorld> world, std::shared_ptr<ILogger> logger, std::shared_ptr<IConfig> config);
    /// @brief Create and register a new agent
    bool spawn_population() override;

    /// @brief Remove an agent
    void update_cycle() override;

  private:
    std::vector<std::shared_ptr<Pop>> pops_;
    std::shared_ptr<IWorld> world_;
    std::shared_ptr<ILogger> logger_;
    std::shared_ptr<IConfig> config_;
    ///--------------------------------------------------------------------------
    /// Buckets for managing agent states (sense, think, act)
    ///--------------------------------------------------------------------------
    std::vector<std::shared_ptr<Pop>> sense_bucket_;
    std::vector<std::shared_ptr<Pop>> think_bucket_;
    std::vector<std::shared_ptr<Pop>> act_bucket_;
    ///--------------------------------------------------------------------------
    /// Rotates the buckets to cycle agent states
    ///--------------------------------------------------------------------------
    void rotate_buckets();
    ///--------------------------------------------------------------------------
    /// @brief Attempts to spawn an offspring of parent into an adjacent free cell.
    ///--------------------------------------------------------------------------
    void try_reproduce(std::shared_ptr<Pop>& parent);
};

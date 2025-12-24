#pragma once
#include "common.h"
#include <memory>
#include "i_agents_manager.h"
#include "i_logger.h"
#include "pop.h"
#include <vector>
#include "random_utility.h"
class PopsManager : public IAgentsManager 
{

public:
    PopsManager(std::shared_ptr<IWorld> world,std::shared_ptr<ILogger> logger );
    /// @brief Create and register a new agent
    bool spawn_population() override;

    /// @brief Remove an agent
    void update_cycle() override;
private:
    std::vector<std::shared_ptr<Pop>> pops_;
    std::shared_ptr<IWorld> world_;
    std::shared_ptr<ILogger> logger_;
};

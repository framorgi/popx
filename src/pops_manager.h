#pragma once
#include "common.h"
#include <memory>
#include "i_agents_manager.h"
#include "i_logger.h"
#include "pop.h"
#include <vector>
#include "random_utility.h"
class pops_manager : public i_agents_manager 
{

public:
    pops_manager(std::shared_ptr<i_world> world,std::shared_ptr<i_logger> logger );

    /// @brief Create and register a new agent
    bool spawn_population() override;

    /// @brief Remove an agent
    void update_cycle() override;
private:
    std::vector<std::shared_ptr<pop>> pops_;
    std::shared_ptr<i_world> world_;
    std::shared_ptr<i_logger> logger_;
};

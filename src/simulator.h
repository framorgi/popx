#pragma once
#include <memory>

#include "i_simulator.h"
#include "grid_world.h"
#include "pops_manager.h"
#include "i_logger.h"


class simulator : public i_simulator {
public:

    simulator( std::shared_ptr<grid_world> world, std::shared_ptr<pops_manager> agents_manager, std::shared_ptr<i_logger> logger );

    void init() override;
    void update() override;
    std::shared_ptr<i_world> get_world() const ;
private:
    
   
 
    bool update_agents();
    bool update_environment();

std::shared_ptr<grid_world> world_;
std::shared_ptr<pops_manager> agents_manager_;
std::shared_ptr<i_logger> logger_;
    // statistic handler 
};
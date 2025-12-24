#pragma once
#include <memory>

#include "i_simulator.h"
#include "grid_world.h"
#include "pops_manager.h"
#include "i_logger.h"

class Simulator : public ISimulator {
public:

    Simulator( std::shared_ptr<GridWorld> world, std::shared_ptr<PopsManager> agents_manager, std::shared_ptr<ILogger> logger );

    void init() override;
    void update() override;
    [[nodiscard]] std::shared_ptr<IWorld> get_world() const ;
private:
    
   
 
    bool update_agents();
    bool update_environment();

std::shared_ptr<GridWorld> world_;
std::shared_ptr<PopsManager> agents_manager_;
std::shared_ptr<ILogger> logger_;
    // statistic handler 
};
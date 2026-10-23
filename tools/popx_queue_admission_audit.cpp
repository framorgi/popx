// Isolated admission-order fixture; never connects to the running simulator.
#include "pops_manager.h"
#include "console_logger.h"
#include "config.h"
#include "grid_world.h"
#include <set>
#include <iostream>
class SimulationAudit {
public:
    static void run(const char* path) {
        auto logger=std::make_shared<ConsoleLogger>();logger->set_level(LogLevel::Error);
        auto cfg=std::make_shared<Config>(path,true);
        auto world=std::make_shared<GridWorld>(24,24,logger);
        PopsManager m(world,logger,cfg);
        for(int i=0;i<159;++i) {
            auto p=std::make_shared<Pop>(world,logger,cfg,Genome(0,Genome::encoding_from_config(*cfg)));
            p->age_=2000;p->energy_=100;p->glucose_=100;m.pops_.push_back(p);
        }
        std::set<std::string> served;
        for(int step=0;step<1000;++step) {
            m.collect_reproducer_candidates();
            // Diagnostic only: consume the same budget as production, with persistent eligibility.
            // This isolates admission order; it does not simulate births or survival.
            for(int n=0;n<3 && !m.repro_backlog_.empty();++n) {
                auto entry=m.repro_backlog_.front();m.repro_backlog_.pop_front();
                m.repro_backlog_ids_.erase(entry.pop_id);served.insert(entry.pop_id);
            }
        }
        std::cout<<"eligible=159 queue_cap="<<cfg->get_max_repro_backlog()<<" rounds=1000 serviced_distinct="<<served.size()<<" never_serviced="<<159-served.size()<<"\n";
    }
};
int main(int argc, char** argv) {
    SimulationAudit::run(argc > 1 ? argv[1] : "config/popx.ini");
}

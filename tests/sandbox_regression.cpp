#include "brain.h"
#include "config.h"
#include "console_logger.h"
#include "grid_world.h"
#include "pops_manager.h"
#include "scorer.h"
#include "stats.h"
#include <cmath>
#include <iostream>
#include <stdexcept>
void require(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
int main() {
    auto config=std::make_shared<Config>("config/popx.ini",true);
    const ScoreObservation observation{4000,3,10,80,79,30};
    const auto initial=Scorer::reward(observation,*config);
    const auto weights=config->get_reward_weights();
    config->apply_runtime({{"reward_age_weight",2*weights[0]},{"reward_energy_weight",2*weights[1]},
                           {"reward_delta_weight",2*weights[2]},{"reward_offspring_weight",2*weights[3]},
                           {"reward_temperature_weight",2*weights[4]}});
    require(std::abs(Scorer::reward(observation,*config)-initial)<1e-6,"normalization changed relative weights");
    config->apply_runtime({{"reward_age_weight",0},{"reward_energy_weight",0},{"reward_delta_weight",0},{"reward_offspring_weight",0},{"reward_temperature_weight",0}});
    require(Scorer::reward(observation,*config)==0,"zero mixer must produce zero reward");
    auto before=config->values();
    try {config->apply_runtime({{"learning_rate",.8},{"map_size",30}});throw std::logic_error("accepted forbidden patch");}catch(const std::invalid_argument&){}
    require(config->values()==before,"invalid patch partially applied");
    Genome genome(0);genome.add_gene(Gene(0,0,7,4,8192));
    Brain brain(config);brain.wire(genome);brain.feed_forward({1});brain.record_action(4);
    const auto connections=brain.get_total_connections();
    brain.edit_weight(0,brain.get_num_hidden()+1,0,4,.125f);
    require(brain.get_total_connections()==connections,"edit changed topology");
    try {brain.edit_weight(0,brain.get_num_hidden()+1,0,3,.5f);throw std::logic_error("created new edge");}catch(const std::invalid_argument&){}
    config->apply_runtime({{"learning_weight_decay",0}});brain.hebbian_update(1);
    require(brain.learning_stats().mean_abs_plasticity==0,"edit retained old credit");
    auto log=std::make_shared<ConsoleLogger>();log->set_level(LogLevel::Error);
    auto world=std::make_shared<GridWorld>(24,24,log);world->init();
    auto stats=std::make_shared<Stats>();stats->begin_session({{"kind","sandbox_fixture"}});
    PopsManager manager(world,log,config);manager.set_stats(stats);manager.spawn_population();stats->sample(0,manager,*world);
    bool spawned=false;
    for(int y=0;y<24&&!spawned;y++)for(int x=0;x<24&&!spawned;x++)if(world->is_free({x,y})){manager.sandbox_spawn({x,y});spawned=true;}
    require(spawned,"fixture spawn unavailable");stats->sample(0,manager,*world);
    require(stats->latest()["counts"]["forced_births"]==1,"sandbox birth not distinct");
    for(auto value:stats->latest()["resource_ledger"]["residual"])require(value.get<double>()==0,"sandbox resource ledger drift");
    require(std::abs(stats->latest()["energy_ledger"]["residual"].get<double>())<1e-5,"sandbox energy ledger drift");
    std::cout<<"PASS: mixer normalization, atomic configuration, topology-preserving edits, cleared credit, external spawn ledgers\n";
}

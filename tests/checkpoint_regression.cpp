#include "checkpoint.h"
#include "console_logger.h"
#include "simulator.h"
#include <iostream>
int main(int argc,char**argv) {
    try {
        auto logger=std::make_shared<ConsoleLogger>();logger->set_level(LogLevel::Error);
        auto config=std::make_shared<Config>(argv[1],true);
        auto w=std::make_shared<GridWorld>(config->get_map_size(),config->get_map_size(),logger,
            SourceSettings{config->get_source_count(),config->get_source_move_interval_ticks(),
                           config->get_source_glucose_interval_ticks(),config->get_source_glucose_per_neighbor()});
        auto m=std::make_shared<PopsManager>(w,logger,config);
        Simulator sim(w,m,logger);sim.init();
        std::ifstream input(argv[1]);std::string ini((std::istreambuf_iterator<char>(input)),{});
        for(int tick=0;tick<30;++tick) {
            auto a=CheckpointCodec::capture(*w,*m,ini,config->values(),"fixture",tick,0);
            a["manager"]["next_id"]=a["pops"][0]["pop_id"];
            a["manager"]["next_index"]=7;
            auto w2=std::make_shared<GridWorld>(config->get_map_size(),config->get_map_size(),logger,
            SourceSettings{config->get_source_count(),config->get_source_move_interval_ticks(),
                           config->get_source_glucose_interval_ticks(),config->get_source_glucose_per_neighbor()});
            PopsManager m2(w2,logger,config);CheckpointCodec::restore(a,*w2,m2);
            auto b=CheckpointCodec::capture(*w2,m2,ini,config->values(),"fixture",tick,0);
            a.erase("next_id");b.erase("next_id");
            if(a!=b) {std::cerr<<nlohmann::json::diff(a,b).dump(2);return 1;}
            // Environment future evolution must also preserve RBF phase, regeneration and pheromones.
            w2->update_cycle();w->update_cycle();
            auto c=CheckpointCodec::capture(*w,*m,ini,config->values(),"fixture",tick,0);
            auto d=CheckpointCodec::capture(*w2,m2,ini,config->values(),"fixture",tick,0);
            if(c["cells"]!=d["cells"] || c["world"]!=d["world"]) throw std::runtime_error("environment continuation differs");
            sim.update();
        }
        auto doc=CheckpointCodec::capture(*w,*m,ini,config->values(),"fixture",30,0);
        auto legacy=doc;
        for(auto& pop:legacy["pops"]) pop.erase("total_other_action_energy_loss");
        auto legacy_world=std::make_shared<GridWorld>(config->get_map_size(),config->get_map_size(),logger,
            SourceSettings{config->get_source_count(),config->get_source_move_interval_ticks(),
                           config->get_source_glucose_interval_ticks(),config->get_source_glucose_per_neighbor()});
        PopsManager legacy_manager(legacy_world,logger,config);
        CheckpointCodec::restore(legacy,*legacy_world,legacy_manager);
        auto restored=CheckpointCodec::capture(*legacy_world,legacy_manager,ini,config->values(),"fixture",30,0);
        for(const auto& pop:restored["pops"])
            if(pop["total_other_action_energy_loss"]!=0) throw std::runtime_error("old checkpoint cost default");
        std::filesystem::path p=std::string(argv[1])+".popx";std::filesystem::remove(p);
        CheckpointCodec::write(p,doc);if(CheckpointCodec::read(p)!=doc)throw std::runtime_error("disk roundtrip");
        {std::fstream f(p,std::ios::in|std::ios::out|std::ios::binary);f.seekp(-1,std::ios::end);f.put('\0');}
        bool rejected=false;try{CheckpointCodec::read(p);}catch(...){rejected=true;}
        if(!rejected)throw std::runtime_error("corruption accepted");std::filesystem::remove(p);
        std::cout<<"checkpoint state, phases, neural weights/traces, environment continuation and corruption: OK\n";
    }catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}
}

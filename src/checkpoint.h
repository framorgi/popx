#pragma once
#include "brain.h"
#include "cell.h"
#include "config.h"
#include "pops_manager.h"
#include "grid_world.h"
#include <filesystem>
#include <fstream>
#include <sstream>
#include <set>
#include <fcntl.h>
#include <unistd.h>

/// @brief Versioned state checkpoints, captured only between complete simulator ticks.
class CheckpointCodec {
    using Json = nlohmann::json;
    static void require(bool ok, const char* message) {
        if (!ok) throw std::runtime_error(std::string("Checkpoint: ") + message);
    }
    static Json genes(const std::vector<Gene>& input) {
        auto j = Json::array();
        for (const auto& g : input) j.push_back({g.get_source_type(), g.get_source_num(), g.get_sink_type(), g.get_sink_num(), g.get_weight()});
        return j;
    }
    static std::vector<Gene> genes(const Json& j) {
        require(j.is_array() && j.size() <= 100000, "invalid genes");
        std::vector<Gene> out;
        for (const auto& g : j) {
            require(g.size() == 5 && g[0].get<int>() >= 0 && g[0].get<int>() <= 7 &&
                    g[2].get<int>() >= 0 && g[2].get<int>() <= 7 &&
                    g[1].get<int64_t>() >= 0 && g[1].get<int64_t>() <= UINT32_MAX &&
                    g[3].get<int64_t>() >= 0 && g[3].get<int64_t>() <= UINT32_MAX &&
                    g[4].get<int>() >= -32768 && g[4].get<int>() <= 32767, "invalid gene endpoint/weight");
            out.emplace_back(g[0], g[1], g[2], g[3], g[4]);
        }
        return out;
    }
    static Json rbf(const RBFSet& r) {
        Json j = {{"min",r.min_value},{"max",r.max_value},{"data",Json::array()}};
        for (const auto& f : r.data) j["data"].push_back({f.amplitude,f.mean_x,f.mean_y,f.stddev_x,f.stddev_y});
        return j;
    }
    static RBFSet rbf(const Json& j) {
        RBFSet r{j.at("max"),j.at("min"),{}};
        require(j.at("data").size() <= 1000, "too many RBFs");
        for (const auto& f : j.at("data")) {
            require(f.size()==5 && f[3].get<double>()>0 && f[4].get<double>()>0,"invalid RBF width");
            r.data.push_back({f[0],f[1],f[2],f[3],f[4]});
        }
        return r;
    }
    static Json brain(const Brain& b) {
        auto j = b.snapshot();
        j["eligibility"] = b.eligibility_;
        return j;
    }
    static void brain(Brain& b, const Json& j, bool partial) {
        require(j.at("sizeS")==b.size_s_ && j.at("sizeN")==b.size_n_ && j.at("sizeY")==b.size_y_ &&
                j.at("num_hidden_layers")==b.num_hidden_, "brain/config dimensions differ");
        b.allocate();
        std::set<unsigned> seen;
        for (const auto& m : j.at("connections")) {
            unsigned src=m.at("from_layer"),dst=m.at("to_layer");
            require(src<dst && dst<b.num_layers_,"invalid layer");
            unsigned idx=src*b.num_layers_+dst;
            require(seen.insert(idx).second,"duplicate matrix");
            std::vector<Eigen::Triplet<float>> entries;
            std::set<std::pair<int,int>> coordinates;
            for (const auto& e : m.at("entries")) {
                int row=e.at("row"),col=e.at("col");float value=e.at("value");
                require(row>=0 && unsigned(row)<b.layer_size(dst) && col>=0 && unsigned(col)<b.layer_size(src) &&
                        std::abs(value)<=400000 && coordinates.emplace(row,col).second,"invalid synapse");
                entries.emplace_back(row,col,value);
            }
            b.M_[idx].setFromTriplets(entries.begin(),entries.end());
            b.M_[idx].makeCompressed();
            b.eligibility_[idx].assign(entries.size(),0);
        }
        const auto& activations=j.at("activations");
        require(activations.empty() || activations.size()==b.num_layers_,"activation layers");
        for (unsigned i=0;i<activations.size();++i) {
            auto v=activations[i].get<std::vector<float>>();
            require(v.size()==b.layer_size(i),"activation size");
            b.activations_.push_back(Eigen::Map<Eigen::VectorXf>(v.data(),v.size()));
        }
        if (!partial) {
            auto traces=j.at("eligibility").get<std::vector<std::vector<float>>>();
            require(traces.size()==b.eligibility_.size(),"trace layers");
            for (unsigned i=0;i<traces.size();++i) {
                require(traces[i].size()==b.eligibility_[i].size(),"trace size");
                for(float value:traces[i]) require(std::abs(value)<=.25001f,"trace range");
            }
            b.eligibility_=std::move(traces);
        }
        b.silent_genes_=genes(j.at("silent_genes"));b.discarded_genes_=genes(j.at("discarded_genes"));
        b.tie_cursor_=j.value("tie_cursor",b.tie_cursor_)%std::max(1u,b.size_y_);
        b.learned_genome_.reset();b.learned_genome_dirty_=true;
    }
    static Json pop(const Pop& p) {
        Json j;
#define SAVE(field) j[#field] = p.field##_
        SAVE(first_run);
        SAVE(inherit_phy);
        SAVE(age);
        SAVE(energy);
        SAVE(alive);
        SAVE(current_state);
        SAVE(next_phase);
        SAVE(steps_in_current_state);
        SAVE(action_requests);
        SAVE(acquired);
        SAVE(released);
        SAVE(movement_attempts);
        SAVE(movement_successes);
        j["movement_failures"] = p.movement_failures_;
        SAVE(pop_id);
        SAVE(parent_id);
        SAVE(birth_was_forced);
        SAVE(last_action);
        SAVE(last_executed_action);
        SAVE(reaper_exposure_ticks);
        SAVE(reaper_escape_attempts);
        SAVE(reaper_escape_successes);
        SAVE(sensor_values);
        SAVE(probe_dist);
        SAVE(osc_period);
        SAVE(responsiveness);
        SAVE(lazyness);
        SAVE(glucose);
        SAVE(water);
        SAVE(calcium);
        SAVE(energy_cost);
        SAVE(previous_energy);
        SAVE(last_reward);
        SAVE(lifetime_distance);
        SAVE(last_move_failed);
        SAVE(offspring_count);
        SAVE(repro_eligible_ticks);
        SAVE(repro_cap_ticks);
        SAVE(repro_queue_exclusions);
        SAVE(repro_admissions);
        SAVE(repro_wait_ticks);
        SAVE(repro_wait_max);
        SAVE(death_cause);
        SAVE(temperature);
        SAVE(o2);
        SAVE(co2);
        SAVE(lipids);
        SAVE(metabolism_heat);
        SAVE(thermoregulation_heat);
        SAVE(total_movement_energy_loss);
        SAVE(total_other_action_energy_loss);
        SAVE(total_metabolism_energy_loss);
        SAVE(total_reproduction_energy_loss);
        SAVE(total_respiration_energy_gain);
        SAVE(total_thermoregolation_energy_loss);
        SAVE(respiration_credit);
        SAVE(total_respiration_heat);
        SAVE(total_respiration_count);
        SAVE(total_photosynthesis_count);
        SAVE(total_energy_cap_loss);
        SAVE(total_energy_roundoff);
#undef SAVE
        j["position"]={p.pos_.x,p.pos_.y};j["direction"]={p.last_direction_.x,p.last_direction_.y};
        j["traits"]={p.phy_.mitochondrions,p.phy_.chloroplasts,p.phy_.sensitiveness,p.phy_.adipose_stock_max};
        j["genes"]=genes(p.genome_.get_genes());j["brain"]=brain(dynamic_cast<const Brain&>(*p.brain_));
        const auto& s=p.scorer_;
        j["scorer"]={s.observations_,s.reward_,s.baseline_,s.signal_,s.components_.age,s.components_.energy,
                     s.components_.delta,s.components_.offspring,s.components_.temperature};
        std::ostringstream rng;rng<<p.random_util_->mt_;j["rng"]=rng.str();
        return j;
    }
    static void pop(Pop& p,const Json& j) {
#define LOAD(field) j.at(#field).get_to(p.field##_)
        LOAD(first_run);
        LOAD(inherit_phy);
        LOAD(age);
        LOAD(energy);
        LOAD(alive);
        LOAD(current_state);
        LOAD(next_phase);
        LOAD(steps_in_current_state);
        LOAD(action_requests);
        LOAD(acquired);
        LOAD(released);
        LOAD(movement_attempts);
        LOAD(movement_successes);
        if (j.contains("movement_failures")) j.at("movement_failures").get_to(p.movement_failures_);
        LOAD(pop_id);
        LOAD(parent_id);
        LOAD(birth_was_forced);
        LOAD(last_action);
        p.last_executed_action_ = j.value("last_executed_action", -1);
        p.reaper_exposure_ticks_ = j.value("reaper_exposure_ticks", uint64_t(0));
        p.reaper_escape_attempts_ = j.value("reaper_escape_attempts", uint64_t(0));
        p.reaper_escape_successes_ = j.value("reaper_escape_successes", uint64_t(0));
        LOAD(sensor_values);
        LOAD(probe_dist);
        LOAD(osc_period);
        LOAD(responsiveness);
        LOAD(lazyness);
        LOAD(glucose);
        LOAD(water);
        LOAD(calcium);
        LOAD(energy_cost);
        LOAD(previous_energy);
        LOAD(last_reward);
        LOAD(lifetime_distance);
        p.last_move_failed_ = j.value("last_move_failed", false);
        LOAD(offspring_count);
        LOAD(repro_eligible_ticks);
        LOAD(repro_cap_ticks);
        LOAD(repro_queue_exclusions);
        LOAD(repro_admissions);
        LOAD(repro_wait_ticks);
        LOAD(repro_wait_max);
        LOAD(death_cause);
        LOAD(temperature);
        LOAD(o2);
        LOAD(co2);
        LOAD(lipids);
        LOAD(metabolism_heat);
        LOAD(thermoregulation_heat);
        LOAD(total_movement_energy_loss);
        p.total_other_action_energy_loss_ = j.value("total_other_action_energy_loss", 0.0);
        LOAD(total_metabolism_energy_loss);
        LOAD(total_reproduction_energy_loss);
        LOAD(total_respiration_energy_gain);
        LOAD(total_thermoregolation_energy_loss);
        LOAD(respiration_credit);
        LOAD(total_respiration_heat);
        LOAD(total_respiration_count);
        LOAD(total_photosynthesis_count);
        LOAD(total_energy_cap_loss);
        LOAD(total_energy_roundoff);
#undef LOAD
        p.pos_={j.at("position").at(0),j.at("position").at(1)};
        p.last_direction_={j.at("direction").at(0),j.at("direction").at(1)};
        const auto& t=j.at("traits");require(t.size()==4,"traits");
        p.phy_={t[0],t[1],t[2],t[3]};
        auto& s=p.scorer_;const auto& score=j.at("scorer");require(score.size()==9,"scorer");
        s.observations_=score[0];s.reward_=score[1];s.baseline_=score[2];s.signal_=score[3];
        s.components_={score[4],score[5],score[6],score[7],score[8]};
        std::istringstream rng(j.at("rng").get<std::string>());rng>>p.random_util_->mt_;require(!rng.fail(),"RNG");
        brain(dynamic_cast<Brain&>(*p.brain_),j.at("brain"),false);
    }
    static void legacy_pop(Pop& p,const Json& j) {
        p.age_=j.at("age");p.energy_=j.at("totals").at("energy");p.temperature_=j.at("totals").at("body_temperature");
        p.pos_={j.at("position").at(0),j.at("position").at(1)};
        p.last_direction_={j.at("direction").at(0),j.at("direction").at(1)};
        p.pop_id_=j.at("pop_id");p.parent_id_=j.at("parent_id");p.birth_was_forced_=j.at("forced");
        const auto& r=j.at("resources");require(r.size()==6,"resources");
        p.glucose_=r[0];p.water_=r[1];p.calcium_=r[2];p.co2_=r[3];p.o2_=r[4];p.lipids_=r[5];
        const auto& t=j.at("traits");p.phy_={t.at("mitochondria"),t.at("chloroplasts"),t.at("sensitiveness"),t.at("adipose_capacity")};
        p.offspring_count_=j.at("totals").at("offspring");p.lifetime_distance_=j.at("totals").at("distance");
        p.energy_cost_=j.at("totals").at("pending_action_cost");p.previous_energy_=p.energy_;
        p.lazyness_=j.at("lazyness");p.last_action_=j.at("last_action");p.sensor_values_=j.at("sensors").get<std::vector<float>>();
        p.next_phase_=static_cast<State>(j.at("phase").get<int>());p.current_state_=p.next_phase_;p.first_run_=false;
        auto& s=p.scorer_;const auto& l=j.at("learning");s.observations_=1;s.reward_=l.at("reward");
        s.baseline_=l.at("baseline");s.signal_=l.at("signal");p.last_reward_=s.reward_;
        s.components_={l.at("age_component"),l.at("energy_component"),l.at("delta_component"),l.at("offspring_component"),l.at("temperature_component")};
        brain(dynamic_cast<Brain&>(*p.brain_),j.at("brain"),true);
    }
    static void finite(const Json& j) {
        if(j.is_number_float()) require(std::isfinite(j.get<double>()),"non-finite value");
        else if(j.is_structured()) for(const auto& v:j) finite(v);
    }
  public:
    static constexpr const char* model = "popx_state_v4";
    static Json signature() {
        // Keep the historical v4 signature compatible; the live cap is stored in the checkpoint INI.
        return {TemperatureRefreshInterval,MaxAge,MaxEnergy,240u,
            MinDecayingSynapseMagnitude,MaxH2o,MaxC6h12o6,MaxLipids,MaxO2,MaxCo2,MaxN2,MaxCaco3,
            O2RegenInterval,O2RegenAmount,
            CalciumWeatheringInterval,MaxFeromones,FeromoneDecayRate,MaxClimbableSlope};
    }
    static Json capture(const GridWorld& w,const PopsManager& m,const std::string& ini,
                        const Json& config,const std::string& run,uint64_t tick,unsigned speed) {
        Json j={{"version",1},{"model",model},{"constants",signature()},{"partial",false},{"ini",ini},{"config",config},
                {"source_run",run},{"source_tick",tick},{"speed",speed},{"next_id",Pop::next_id()},
                {"width",w.width_},{"height",w.height_},{"cells",Json::array()},{"pops",Json::array()}};
        if (ini.find("# Checkpoint resolved paths") == std::string::npos) {
            std::string portable = ini + "\n# Checkpoint resolved paths\n";
            for (const auto* key : {"nnets_dir", "log_dir", "image_dir"}) portable += std::string(key) + " = " + config.at(key).get<std::string>() + "\n";
            j["ini"] = portable;
        }
        j["world"]={{"environment_tick",w.environment_tick_counter_},{"rotation_tick",w.rotation_tick_counter_},
                    {"temperature",rbf(w.temp_rbf_set_)},{"elevation",rbf(w.elevation_rbf_set_)},
                    {"regenerated_glucose",w.glucose_regenerated_},{"regenerated_o2",w.oxygen_regenerated_},
                    {"reaper_kills",w.reaper_kills_},{"sources",Json::array()},{"reapers",Json::array()},
                    {"glucose",rbf(w.glucose_rbf_set_)},{"water",rbf(w.water_rbf_set_)}};
        for (const auto& source : w.sources_)
            j["world"]["sources"].push_back(source->snapshot());
        for (const auto& reaper : w.reapers_)
            j["world"]["reapers"].push_back(reaper->snapshot());
        for(const auto& item:w.cells_) {
            const auto& c=dynamic_cast<const Cell&>(*item);const auto& o=c.organics_;
            j["cells"].push_back({{"temperature",c.temperature_},{"elevation",c.elevation_},
                {"organics",{o.c6h12o6,o.lipids,o.o2,o.co2,o.h2o,o.n2,o.caco3}},
                {"pheromones",{c.feromones_.at(FOOD_FEROMONE),c.feromones_.at(DANGER_FEROMONE),
                    c.feromones_.at(MATE_FEROMONE),c.feromones_.at(HOME_FEROMONE)}},
                {"regen_o2",c.regen_o2_tick_},{"reserved",c.reserved_}});
        }
        for(const auto& p:m.pops_) {require(p->alive_,"dead pop between ticks");j["pops"].push_back(pop(*p));}
        auto ids=[](const auto& bucket){auto out=Json::array();for(const auto& p:bucket)out.push_back(p->pop_id_);return out;};
        j["manager"]={{"next_id",m.repro_next_id_},{"next_index",m.repro_next_index_},{"cycle",m.cycle_counter_},{"generation",m.generation_count_},{"forced",m.forced_respawn_active_},
            {"sense",ids(m.sense_bucket_)},{"think",ids(m.think_bucket_)},{"act",ids(m.act_bucket_)},{"backlog",Json::array()}};
        for(const auto& entry:m.repro_backlog_) if(auto p=entry.pop.lock();p && p->alive_)j["manager"]["backlog"].push_back(p->pop_id_);
        j["manager"]["recovery_bank"] = Json::array();
        for (const auto& seed : m.recovery_bank_)
            j["manager"]["recovery_bank"].push_back({{"genes", genes(seed.genome.get_genes())},
                {"traits", {seed.phy.mitochondrions, seed.phy.chloroplasts, seed.phy.sensitiveness,
                            seed.phy.adipose_stock_max}},
                {"parent_id", seed.parent_id}, {"profile_key", seed.profile_key}});
        return j;
    }
    static void restore(const Json& j,GridWorld& w,PopsManager& m) {
        require(j.at("version")==1 && j.at("model")==model,"unsupported format/model");finite(j);
        const bool partial=j.at("partial");
        require(partial || j.at("constants")==signature(),"simulation constants changed");
        require(j.at("width")==w.width_ && j.at("height")==w.height_,"world/config dimensions differ");
        require(j.at("pops").size()<=m.config_->get_max_population_allowed() && !j.at("pops").empty(),"population size");
        if(partial) {
            w.init();
            for (auto& source : w.sources_) source->despawn();
            w.sources_.clear();
            for (auto& reaper : w.reapers_) reaper->despawn();
            w.reapers_.clear();
        }
        else {
            require(j.at("cells").size()==w.cells_.size(),"cell count");
            const auto& env=j.at("world");w.environment_tick_counter_=env.at("environment_tick");
            w.glucose_regenerated_=env.at("regenerated_glucose");w.oxygen_regenerated_=env.at("regenerated_o2");
            w.reaper_kills_=env.value("reaper_kills",uint64_t(0));
            w.source_settings_.count = env.at("sources").size();
            require(w.source_settings_.count <= 32, "source count");
            w.reset_sources(false);
            require(env.at("sources").size()==w.sources_.size(),"source count");
            w.rotation_tick_counter_=env.at("rotation_tick");w.temp_rbf_set_=rbf(env.at("temperature"));
            w.elevation_rbf_set_=rbf(env.at("elevation"));w.glucose_rbf_set_=rbf(env.at("glucose"));w.water_rbf_set_=rbf(env.at("water"));
            for(unsigned i=0;i<w.cells_.size();++i) {
                auto& c=dynamic_cast<Cell&>(*w.cells_[i]);const auto& d=j.at("cells")[i];const auto& o=d.at("organics");
                require(o.size()==7,"organics size");
                const unsigned caps[]={MaxC6h12o6,MaxLipids,MaxO2,MaxCo2,MaxH2o,MaxN2,MaxCaco3};
                for(unsigned k=0;k<7;++k)require(o[k].get<int64_t>()>=0 && o[k].get<uint64_t>()<=caps[k],"cell capacity");
                c.temperature_=d.at("temperature");c.elevation_=d.at("elevation");c.organics_={o[0],o[1],o[2],o[3],o[4],o[5],o[6]};
                c.regen_o2_tick_=d.at("regen_o2");c.reserved_=d.at("reserved");
                require(c.regen_o2_tick_<=O2RegenInterval,"oxygen regeneration phase");
                require(d.at("pheromones").size()==4,"pheromone channels");
                for(int k=0;k<4;++k){float value=d.at("pheromones")[k];require(value>=0 && value<=MaxFeromones,"pheromone range");c.feromones_[FeromoneT(k)]=value;}
            }
            for (std::size_t i=0;i<w.sources_.size();++i)
                require(w.sources_[i]->restore(env.at("sources")[i]),"source state/occupancy");
        }
        // Legacy migration retains original terrain only; current resources/weather were unavailable.
        if(partial && j.contains("terrain")) {
            require(j.at("terrain").size()==w.cells_.size(),"legacy terrain");
            for(unsigned i=0;i<w.cells_.size();++i)w.cells_[i]->set_elevation(j.at("terrain")[i]);
        }
        std::map<std::string,std::shared_ptr<Pop>> by_id;
        uint64_t next=j.at("next_id");
        for(const auto& d:j.at("pops")) {
            Genome genome(genes(d.at("genes")),Genome::encoding_from_config(*m.config_));
            auto p=std::make_shared<Pop>(m.world_,m.logger_,m.config_,std::move(genome));
            if(partial)legacy_pop(*p,d);else pop(*p,d);
            require(p->alive_ && p->age_<=MaxAge && p->energy_>0 && p->energy_<=MaxEnergy,"invalid living pop");
            require(p->pos_.x>=0 && p->pos_.x<w.width_ && p->pos_.y>=0 && p->pos_.y<w.height_,"position bounds");
            require(int(p->next_phase_)>=0 && int(p->next_phase_)<3 && int(p->current_state_)>=0 && int(p->current_state_)<3,"phase");
            require(p->sensor_values_.size()==m.config_->get_brain_size_s(),"sensor count");
            require(p->pop_id_.starts_with("pop_") && by_id.emplace(p->pop_id_,p).second,"duplicate/invalid pop id");
            next=std::max(next, uint64_t(std::stoull(p->pop_id_.substr(4))+1));
            auto& c=dynamic_cast<Cell&>(*w.get_cell(p->pos_));require(c.occupant_.expired(),"overlapping occupants");
            c.occupant_=p; // A living occupant can legitimately share its cell with deposited carbonate.
            m.pops_.push_back(p);
        }
        if (!partial && j.at("world").contains("reapers")) {
            const auto& saved = j.at("world").at("reapers");
            require(saved.is_array() && saved.size() <= 4,"reaper count");
            w.reaper_settings_.count = saved.size();
            w.reset_reapers(false);
            for (std::size_t i=0;i<w.reapers_.size();++i)
                require(w.reapers_[i]->restore(saved[i]),"reaper state/occupancy");
        } else {
            // Older checkpoints predate Reaper. Introduce it only after all saved Pops are placed.
            w.reset_reapers();
        }
        if(partial) {
            for(const auto& p:m.pops_) {
                auto& bucket=p->next_phase_==State::SENSE?m.sense_bucket_:p->next_phase_==State::THINK?m.think_bucket_:m.act_bucket_;
                bucket.push_back(p);
            }
            m.cycle_counter_=j.at("source_tick");
        } else {
            const auto& manager=j.at("manager");m.cycle_counter_=manager.at("cycle");
            m.repro_next_id_=manager.at("next_id");m.repro_next_index_=manager.at("next_index");
            m.generation_count_=manager.at("generation");m.forced_respawn_active_=manager.at("forced");
            // Old checkpoints have no archive; it will fill as the restored population dies.
            if (manager.contains("recovery_bank")) {
                const auto& bank = manager.at("recovery_bank");
                require(bank.is_array() && bank.size() <= PopsManager::recovery_bank_capacity_, "recovery bank size");
                std::set<std::string> profiles, parents;
                for (const auto& seed : bank) {
                    const auto& t = seed.at("traits");
                    require(t.is_array() && t.size() == 4, "recovery traits");
                    for (const auto& value : t)
                        require(value.is_number_integer() && value.get<int64_t>() >= 0 &&
                                value.get<uint64_t>() <= UINT32_MAX, "recovery trait range");
                    require(t[0].get<unsigned>() > 0 && t[2].get<unsigned>() > 0, "recovery physiology");
                    const auto parent = seed.at("parent_id").get<std::string>();
                    const auto key = seed.at("profile_key").get<std::string>();
                    require(parent.starts_with("pop_") && parent.size() > 4 && parent.size() <= 24 &&
                            std::all_of(parent.begin() + 4, parent.end(), [](char c) { return c >= '0' && c <= '9'; }) &&
                            parents.insert(parent).second && !by_id.contains(parent), "recovery donor id");
                    require(!key.empty() && key.size() <= 128 && profiles.insert(key).second, "recovery profile key");
                    next = std::max(next, uint64_t(std::stoull(parent.substr(4)) + 1));
                    m.recovery_bank_.push_back({Genome(genes(seed.at("genes")), Genome::encoding_from_config(*m.config_)),
                                               {t[0], t[1], t[2], t[3]}, parent, key, false});
                }
            }
            std::set<std::string> placed;
            auto bucket=[&](const char* key,auto& target,State phase){
                for(const auto& id:manager.at(key)) {
                    auto p=by_id.at(id.get<std::string>());require(placed.insert(p->pop_id_).second && (p->first_run_ || p->next_phase_==phase),"bucket phase/order");target.push_back(p);
                }
            };
            bucket("sense",m.sense_bucket_,State::SENSE);bucket("think",m.think_bucket_,State::THINK);bucket("act",m.act_bucket_,State::ACT);
            require(placed.size()==m.pops_.size(),"missing bucket member");
            require(manager.at("backlog").size()<=m.config_->get_max_repro_backlog(),"backlog capacity");
            for(const auto& id:manager.at("backlog")) {
                auto p=by_id.at(id.get<std::string>());require(m.repro_backlog_ids_.insert(p->pop_id_).second,"duplicate backlog");
                m.repro_backlog_.push_back({p,p->pop_id_});
            }
        }
        if (partial) w.reset_sources();
        Pop::reserve_ids(next);
    }
    static uint64_t checksum(const std::vector<uint8_t>& bytes) {
        uint64_t value=14695981039346656037ull;for(auto c:bytes){value^=c;value*=1099511628211ull;}return value;
    }
    static void write(const std::filesystem::path& path,const Json& document) {
        finite(document);
        auto bytes=Json::to_cbor(document);
        std::ostringstream header;header<<"POPXCP1\n"<<std::hex<<checksum(bytes)<<'\n';
        std::filesystem::create_directories(path.parent_path());
        require(!std::filesystem::exists(path),"destination exists");
        auto temp=path.string()+".tmp";
        int fd=::open(temp.c_str(),O_WRONLY|O_CREAT|O_EXCL,0600);require(fd>=0,"cannot create temporary file");
        try {
            auto put=[&](const void* data,size_t size){auto p=static_cast<const char*>(data);while(size){auto n=::write(fd,p,size);if(n<0 && errno==EINTR)continue;require(n>0,"write failed");p+=n;size-=n;}};
            auto h=header.str();put(h.data(),h.size());put(bytes.data(),bytes.size());require(::fsync(fd)==0,"fsync failed");::close(fd);fd=-1;
            auto check=read(temp);require(check==document,"round-trip verification failed");
            std::filesystem::rename(temp,path);
            int dir=::open(path.parent_path().c_str(),O_DIRECTORY|O_RDONLY);require(dir>=0,"directory open failed");int ok=::fsync(dir);::close(dir);require(ok==0,"directory sync failed");
        } catch(...) {if(fd>=0)::close(fd);std::filesystem::remove(temp);throw;}
    }
    static Json read(const std::filesystem::path& path) {
        require(std::filesystem::file_size(path)<=128*1024*1024,"file too large");
        std::ifstream in(path,std::ios::binary);require(bool(in),"cannot open file");std::string magic,sum;
        std::getline(in,magic);std::getline(in,sum);require(magic=="POPXCP1","file magic");
        std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(in)),{});
        require(checksum(bytes)==std::stoull(sum,nullptr,16),"checksum mismatch");
        auto j=Json::from_cbor(bytes);require(j.at("version")==1 && j.at("model")==model,"unsupported version");finite(j);return j;
    }
};

// Common-garden interventions, separate from evolved behaviour and production policy.
#include "brain.h"
#include "cell.h"
#include "config.h"
#include "console_logger.h"
#include "grid_world.h"
#include "pop.h"
#include "pops_manager.h"

#include <array>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <nlohmann/json.hpp>
#include <random>
#include <set>
#include <string>
#include <vector>

using Json = nlohmann::json;
class GardenConfig : public Config {
  public:
    explicit GardenConfig(const std::string& path) : Config(path) {}
    double get_point_mutation_rate() const override {
        return 0;
    }
    bool get_hebbian_inheritance() const override {
        return false;
    }
    bool get_newgen_enabled() const override {
        return false;
    }
    double get_phy_init_temperature() const override {
        return 37;
    }
    bool get_brain_serialization_enabled() const override {
        return false;
    }
};

class SimulationAudit {
  public:
    static constexpr unsigned horizon = 3000;
    static constexpr int width = 16;
    static void empty_cell(const std::shared_ptr<ICell>& c) {
        c->take_glucose(MaxC6h12o6);
        c->take_water(MaxH2o);
        c->take_calcium(MaxCaco3);
        c->take_co2(MaxCo2);
        c->take_o2(MaxO2);
        c->take_lipids(MaxLipids);
        c->set_temperature(37);
        c->set_elevation(31);
    }
    static Json trial(const std::shared_ptr<ILogger>& logger, const std::shared_ptr<IConfig>& cfg, unsigned seed,
                      bool photo, bool patch, const std::string& strategy) {
        auto world = std::make_shared<GridWorld>(width, width, logger);
        const auto cells = world->get_cells();
        for (const auto& c : cells) {
            empty_cell(c);
            c->set_water(MaxH2o);
            c->give_co2(MaxCo2);
            c->give_o2(MaxO2);
        }
        if (!patch)
            for (const auto& c : cells)
                c->set_glucose(128);
        else {
            // Equal total 32768, concentrated in 72 cells; founder starts outside the patch.
            unsigned remainder = 32768;
            for (int y = 4; y <= 12; ++y)
                for (int x = 8; x < 16; ++x) {
                    const unsigned amount = std::min(456u, remainder);
                    world->get_cell(PositionT{x, y})->set_glucose(amount);
                    remainder -= amount;
                }
            if (remainder)
                throw std::runtime_error("patch budget mismatch");
        }
        const PhyT traits{1, photo ? 14u : 0u, 1, 80};
        PopsManager manager(world, logger, cfg);
        auto founder = std::make_shared<Pop>(world, logger, cfg, Genome(0), traits);
        founder->lazyness_ = 0;
        founder->last_direction_ = {1, 0};
        if (!founder->try_spawn({3, 8}))
            throw std::runtime_error("founder spawn failed");
        manager.pops_.push_back(founder);
        std::map<std::string, std::shared_ptr<Pop>> seen{{founder->pop_id_, founder}};
        std::map<std::string, unsigned> serial{{founder->pop_id_, 0}};
        std::vector<unsigned> depths{0};
        std::vector<std::mt19937> rngs;
        rngs.emplace_back(seed);
        uint64_t attempts = 0, successes = 0, blocked_boundary = 0, blocked_occupancy = 0, other_failures = 0;
        uint64_t glucose_uptake = 0, water_uptake = 0, founder_glucose_uptake = 0, cap_ticks = 0;
        uint64_t population_ticks = 0;
        double max_temperature = 37, min_temperature = 37;
        Json births = Json::array(), samples = Json::array();
        unsigned tick = 0;
        for (tick = 1; tick <= horizon; ++tick) {
            // Constant oxygen is an explicit experimental reservoir, identical for all strategies.
            for (const auto& c : cells)
                c->give_o2(MaxO2);
            const auto alive_before = manager.pops_;
            for (const auto& p : alive_before) {
                if (!p->alive_)
                    continue;
                const unsigned phase = p->age_ % 3;
                if (phase == 0)
                    p->sense();
                else if (phase == 1)
                    p->think();
                else {
                    int action = -1;
                    if (strategy == "constant_east")
                        action = static_cast<int>(Action::MOVE_EAST);
                    else if (strategy == "patrol") {
                        if (p->pos_.x == width - 1)
                            action = static_cast<int>(Action::MOVE_WEST);
                        else if (p->pos_.x == 0)
                            action = static_cast<int>(Action::MOVE_EAST);
                        else
                            action = static_cast<int>(p->last_direction_.x < 0 ? Action::MOVE_WEST : Action::MOVE_EAST);
                    } else if (strategy == "random_cardinal") {
                        constexpr Action directions[] = {Action::MOVE_NORTH, Action::MOVE_EAST, Action::MOVE_SOUTH,
                                                         Action::MOVE_WEST};
                        action = static_cast<int>(
                            directions[std::uniform_int_distribution<int>(0, 3)(rngs[serial.at(p->pop_id_)])]);
                    }
                    p->last_action_ = action; // Diagnostic intervention; no production policy is changed.
                    const auto before = p->pos_;
                    const double distance = p->lifetime_distance_;
                    PositionT target = before;
                    if (action == static_cast<int>(Action::MOVE_EAST))
                        ++target.x;
                    if (action == static_cast<int>(Action::MOVE_WEST))
                        --target.x;
                    if (action == static_cast<int>(Action::MOVE_NORTH))
                        --target.y;
                    if (action == static_cast<int>(Action::MOVE_SOUTH))
                        ++target.y;
                    const bool boundary = target.x < 0 || target.y < 0 || target.x >= width || target.y >= width;
                    const bool occupied = !boundary && !world->get_cell(target)->get_occupant().expired();
                    p->act();
                    if (action >= 0) {
                        ++attempts;
                        if (p->lifetime_distance_ > distance)
                            ++successes;
                        else if (boundary)
                            ++blocked_boundary;
                        else if (occupied)
                            ++blocked_occupancy;
                        else
                            ++other_failures;
                    }
                }
                const auto c = world->get_cell(p->pos_);
                const auto g = std::min(5u, c->get_glucose()), w = std::min(5u, c->get_water());
                glucose_uptake += g;
                water_uptake += w;
                if (p == founder)
                    founder_glucose_uptake += g;
                p->first_run_ = true; // Freeze learning to isolate the prescribed strategy.
                p->update();
                ++population_ticks;
                min_temperature = std::min(min_temperature, p->temperature_);
                max_temperature = std::max(max_temperature, p->temperature_);
            }
            for (const auto& p : manager.pops_)
                if (!p->alive_)
                    world->remove_entity(p);
            manager.pops_.erase(
                std::remove_if(manager.pops_.begin(), manager.pops_.end(), [](const auto& p) { return !p->alive_; }),
                manager.pops_.end());
            const auto parents = manager.pops_;
            unsigned budget = cfg->get_max_reproducers_per_tick();
            for (auto parent : parents) {
                if (!budget || manager.pops_.size() >= MaxPopulationAllowed)
                    break;
                if (!parent->wants_to_reproduce())
                    continue;
                --budget;
                const auto size = manager.pops_.size();
                manager.try_reproduce(parent);
                if (manager.pops_.size() > size) {
                    auto child = manager.pops_.back();
                    child->lazyness_ = 0;
                    child->last_direction_ = {1, 0};
                    const unsigned n = seen.size();
                    seen[child->pop_id_] = child;
                    serial[child->pop_id_] = n;
                    rngs.emplace_back(seed + 104729u * n);
                    depths.push_back(depths[serial.at(parent->pop_id_)] + 1);
                    births.push_back({{"tick", tick},
                                      {"serial", n},
                                      {"parent_serial", serial.at(parent->pop_id_)},
                                      {"initial_energy", child->energy_},
                                      {"depth", depths[n]}});
                }
            }
            if (manager.pops_.size() >= MaxPopulationAllowed)
                ++cap_ticks;
            if (tick % 500 == 0 || manager.pops_.empty())
                samples.push_back({{"tick", tick},
                                   {"alive", manager.pops_.size()},
                                   {"births", births.size()},
                                   {"founder_energy", founder->energy_},
                                   {"founder_glucose", founder->glucose_},
                                   {"founder_distance", founder->lifetime_distance_},
                                   {"glucose_uptake", glucose_uptake},
                                   {"water_uptake", water_uptake}});
            if (manager.pops_.empty())
                break;
        }
        Json individuals = Json::array();
        uint64_t respirations = 0, photosynthesis = 0;
        double energy_gain = 0, movement_cost = 0, basal = 0, repro_cost = 0, cap_loss = 0;
        unsigned thermal_deaths = 0, energy_deaths = 0, descendants_mature = 0, descendants_alive = 0;
        for (const auto& [id, p] : seen) {
            respirations += p->total_respiration_count_;
            photosynthesis += p->total_photosynthesis_count_;
            energy_gain += p->total_respiration_energy_gain_;
            movement_cost += p->total_movement_energy_loss_;
            basal += p->total_metabolism_energy_loss_;
            repro_cost += p->total_reproduction_energy_loss_;
            cap_loss += p->total_energy_cap_loss_;
            thermal_deaths += p->death_cause_ == DeathCause::TemperatureOutOfRange;
            energy_deaths += p->death_cause_ == DeathCause::EnergyDepletion;
            if (p != founder) {
                descendants_mature += p->age_ > 1000;
                descendants_alive += p->alive_;
            }
            individuals.push_back({{"serial", serial.at(id)},
                                   {"age", p->age_},
                                   {"alive", p->alive_},
                                   {"offspring", p->offspring_count_},
                                   {"distance", p->lifetime_distance_},
                                   {"energy", p->energy_}});
        }
        return {{"seed", seed},
                {"photosynthesis", photo},
                {"distribution", patch ? "patch" : "uniform"},
                {"strategy", strategy},
                {"tick_horizon", std::min(tick, horizon)},
                {"initial_glucose_world", 32768},
                {"movement_attempts", attempts},
                {"movement_successes", successes},
                {"boundary_failures", blocked_boundary},
                {"occupancy_failures", blocked_occupancy},
                {"other_movement_failures", other_failures},
                {"founder_glucose_uptake", founder_glucose_uptake},
                {"glucose_uptake", glucose_uptake},
                {"water_uptake", water_uptake},
                {"respirations", respirations},
                {"photosynthesis_reactions", photosynthesis},
                {"energy_income", energy_gain},
                {"movement_cost", movement_cost},
                {"basal_cost", basal},
                {"reproduction_cost", repro_cost},
                {"energy_discarded_at_cap", cap_loss},
                {"founder_age", founder->age_},
                {"founder_alive", founder->alive_},
                {"founder_energy", founder->energy_},
                {"founder_offspring", founder->offspring_count_},
                {"descendants_born", births.size()},
                {"descendants_alive", descendants_alive},
                {"descendants_mature", descendants_mature},
                {"thermal_deaths", thermal_deaths},
                {"energy_deaths", energy_deaths},
                {"population_ticks", population_ticks},
                {"population_cap_ticks", cap_ticks},
                {"body_temperature_range", {min_temperature, max_temperature}},
                {"samples", samples},
                {"births", births},
                {"individuals", individuals}};
    }

    static Json boundary_probe(const std::shared_ptr<ILogger>& logger, const std::shared_ptr<IConfig>& cfg) {
        auto world = std::make_shared<GridWorld>(120, 120, logger);
        for (const auto& c : world->get_cells()) {
            empty_cell(c);
            c->set_glucose(128);
            c->set_water(MaxH2o);
            c->give_o2(MaxO2);
        }
        Genome genome(0);
        genome.add_gene(Gene(1, 0, 7, static_cast<unsigned>(Action::MOVE_EAST), 8192));
        auto p = std::make_shared<Pop>(world, logger, cfg, genome, PhyT{1, 0, 1, 80});
        p->lazyness_ = 0;
        p->try_spawn({60, 60});
        unsigned requests = 0, successes = 0, first_block = 0;
        for (unsigned tick = 1; tick <= 3000; ++tick) {
            if ((tick - 1) % 3 == 0)
                p->sense();
            else if ((tick - 1) % 3 == 1)
                p->think();
            else {
                if (p->last_action_ != static_cast<int>(Action::MOVE_EAST))
                    throw std::runtime_error("encoded direction changed");
                ++requests;
                const auto before = p->lifetime_distance_;
                p->act();
                if (p->lifetime_distance_ > before)
                    ++successes;
                else if (!first_block)
                    first_block = tick;
            }
            p->first_run_ = true;
            p->update();
            if (!p->alive_)
                throw std::runtime_error("boundary probe died unexpectedly");
        }
        if (requests != 1000 || successes != 59 || first_block != 180)
            throw std::runtime_error("boundary probe mismatch");
        return {{"world", {120, 120}},
                {"start", {60, 60}},
                {"final", {p->pos_.x, p->pos_.y}},
                {"genome_gene", {1, 0, 7, static_cast<unsigned>(Action::MOVE_EAST), 8192}},
                {"ticks", 3000},
                {"requests", requests},
                {"successes", successes},
                {"boundary_failures", requests - successes},
                {"first_blocked_tick", first_block},
                {"movement_energy_cost", p->total_movement_energy_loss_},
                {"basal_energy_cost", p->total_metabolism_energy_loss_},
                {"policy_override", false},
                {"plasticity", false},
                {"reproduction_enabled", false}};
    }

    static Json network_replay(const std::shared_ptr<ILogger>& logger, const std::shared_ptr<IConfig>& cfg,
                               const Json& corpus) {
        Json results = Json::array();
        for (const auto& item : corpus) {
            Genome genome(0);
            for (const auto& g : item["genes"])
                genome.add_gene(Gene(g[0].get<unsigned>(), g[1].get<unsigned>(), g[2].get<unsigned>(),
                                     g[3].get<unsigned>(), g[4].get<int>()));
            auto world = std::make_shared<GridWorld>(width, width, logger);
            for (const auto& c : world->get_cells()) {
                empty_cell(c);
                c->set_glucose(128);
                c->set_water(MaxH2o);
                c->give_o2(MaxO2);
            }
            auto p = std::make_shared<Pop>(world, logger, cfg, genome, PhyT{1, 0, 1, 80});
            p->lazyness_ = 0;
            p->last_direction_ = {1, 0};
            p->try_spawn({3, 8});
            std::set<std::pair<int, int>> visited{{3, 8}};
            std::array<unsigned, 21> choices{};
            unsigned attempts = 0, success = 0, boundary = 0, zero = 0, changes = 0;
            int previous = -1;
            for (unsigned t = 0; t < 120 && p->alive_; ++t) {
                if (t % 3 == 0)
                    p->sense();
                else if (t % 3 == 1)
                    p->think();
                else {
                    int a = p->last_action_;
                    if (a >= 0 && a < 21)
                        ++choices[a];
                    if (previous >= 0 && a != previous)
                        ++changes;
                    previous = a;
                    PositionT target = p->pos_;
                    RandomUtility predictor = *p->random_util_;
                    const auto direction = p->last_direction_;
                    const float response = p->responsiveness_;
                    switch (static_cast<Action>(a)) {
                        case Action::MOVE_FORWARD:
                            target.x += direction.x * response;
                            target.y += direction.y * response;
                            break;
                        case Action::MOVE_LEFT:
                            target.x -= direction.y * response;
                            target.y += direction.x * response;
                            break;
                        case Action::MOVE_RIGHT:
                            target.x += direction.y * response;
                            target.y -= direction.x * response;
                            break;
                        case Action::MOVE_RANDOM:
                            target.x += predictor.rnd_int(-1, 1) * response;
                            target.y += predictor.rnd_int(-1, 1) * response;
                            break;
                        case Action::MOVE_EAST:
                            target.x += response;
                            break;
                        case Action::MOVE_WEST:
                            target.x -= response;
                            break;
                        case Action::MOVE_NORTH:
                            target.y -= response;
                            break;
                        case Action::MOVE_SOUTH:
                            target.y += response;
                            break;
                        default:
                            break;
                    }
                    const auto d = p->lifetime_distance_;
                    p->act();
                    if (a >= 0 && a < 8) {
                        ++attempts;
                        success += p->lifetime_distance_ > d;
                        if (p->lifetime_distance_ == d) {
                            boundary += target.x < 0 || target.y < 0 || target.x >= width || target.y >= width;
                            zero += target.x == p->pos_.x && target.y == p->pos_.y;
                        }
                    }
                    visited.emplace(p->pos_.x, p->pos_.y);
                }
                p->first_run_ = true;
                p->update();
            }
            unsigned active = 0;
            for (auto count : choices)
                active += count > 0;
            results.push_back({{"corpus_id", item["id"]},
                               {"distinct_actions", active},
                               {"action_changes", changes},
                               {"action_counts", choices},
                               {"movement_requests", attempts},
                               {"movement_successes", success},
                               {"boundary_failures", boundary},
                               {"zero_displacement_requests", zero},
                               {"visited_cells", visited.size()},
                               {"distance", p->lifetime_distance_}});
        }
        return results;
    }
};

int main(int argc, char** argv) {
    if (argc < 3)
        return 2;
    auto logger = std::make_shared<ConsoleLogger>();
    logger->set_level(LogLevel::Error);
    auto cfg = std::make_shared<GardenConfig>("config/popx.ini");
    Json report;
    report["metadata"] = {{"horizon", 3000},
                          {"world", {16, 16}},
                          {"environment_temperature", 37},
                          {"elevation", 31},
                          {"founder_energy", 100},
                          {"founder_position", {3, 8}},
                          {"traits", {{1, 0, 1, 80}, {1, 14, 1, 80}}},
                          {"policy_seeds", {17, 29, 43, 61}},
                          {"mutation", 0},
                          {"plasticity", false},
                          {"forced_reproduction", false},
                          {"resource_regeneration", false},
                          {"oxygen", "constant per-cell reservoir"},
                          {"water_per_cell", 1000},
                          {"co2_per_cell", 1000},
                          {"scope", "Prescribed policies in separate identical gardens; descendants follow their "
                                    "family's policy. No spontaneous evolution claim."}};
    report["trials"] = Json::array();
    auto start = std::chrono::steady_clock::now();
    const bool boundary_only = argc > 3 && std::string(argv[3]) == "boundary";
    const bool replay_only = boundary_only || (argc > 3 && std::string(argv[3]) == "replay");
    if (!replay_only)
        for (unsigned seed : {17u, 29u, 43u, 61u}) {
            for (bool photo : {false, true})
                for (bool patch : {false, true})
                    for (const std::string strategy : {"stationary", "constant_east", "patrol", "random_cardinal"})
                        report["trials"].push_back(SimulationAudit::trial(logger, cfg, seed, photo, patch, strategy));
            std::cout << "Completed paired block seed " << seed << std::endl;
            if (std::chrono::steady_clock::now() - start > std::chrono::seconds(30))
                break;
        }
    std::ifstream file(argv[1]);
    Json corpus;
    file >> corpus;
    if (!boundary_only)
        report["network_replay"] = SimulationAudit::network_replay(logger, cfg, corpus["run"]["initial_genomes"]);
    report["boundary_probe"] = SimulationAudit::boundary_probe(logger, cfg);
    report["network_replay_note"] = "80 original genomes, common traits/environment, frozen learning, 120 ticks. "
                                    "Production RANDOM/emission RNGs remain unseeded: exploratory only.";
    report["wall_seconds"] = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
    std::ofstream out(argv[2]);
    out << report.dump(2);
    if (!out.good())
        return 3;
    std::cout << "Trials: " << report["trials"].size() << ", seconds: " << report["wall_seconds"] << std::endl;
}

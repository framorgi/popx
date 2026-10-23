#include "diagnostic_output.h"
// Controlled physiology and finite-resource gardens; policies are explicit diagnostic interventions.
#include "config.h"
#include "console_logger.h"
#include "grid_world.h"
#include "pop.h"
#include "pops_manager.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>
#include <stdexcept>

using Json = nlohmann::json;
class ThermalConfig : public Config {
  public:
    ThermalConfig(const std::string& path, std::string variant) : Config(path), variant_(std::move(variant)) {}
    double get_phy_init_temperature() const override {
        return variant_ == "initial37" ? 37.0 : Config::get_phy_init_temperature();
    }
    double get_phy_heat_per_respiration() const override {
        if (variant_ == "heat0.03")
            return 0.03;
        if (variant_ == "heat0.1" || variant_ == "heat0.1_alpha0.02")
            return 0.1;
        if (variant_ == "heat0.2")
            return 0.2;
        return Config::get_phy_heat_per_respiration();
    }
    double get_phy_efficiency_half_range_c() const override {
        return variant_ == "half_range20" ? 20.0 : Config::get_phy_efficiency_half_range_c();
    }
    double get_phy_alpha_min() const override {
        if (variant_ == "alpha_min0.05")
            return 0.05;
        if (variant_ == "alpha_min0.02" || variant_ == "heat0.1_alpha0.02")
            return 0.02;
        return Config::get_phy_alpha_min();
    }

  private:
    std::string variant_;
};

class ModerateConfig : public ThermalConfig {
  public:
    ModerateConfig(const std::string& path, const std::string& variant) : ThermalConfig(path, variant) {}
    double get_point_mutation_rate() const override {
        return 0;
    } // Paired diagnostic only.
    bool get_newgen_enabled() const override {
        return false;
    }
};

class SimulationAudit {
  public:
    static Json garden(const std::shared_ptr<ILogger>& logger, const std::shared_ptr<IConfig>& config, double cold,
                       double warm, unsigned mitochondria, bool moving, unsigned placement) {
        constexpr int width = 24, founders = 24;
        auto world = std::make_shared<GridWorld>(width, width, logger);
        for (int y = 0; y < width; ++y)
            for (int x = 0; x < width; ++x) {
                auto c = world->get_cell({x, y});
                c->set_temperature(cold + (warm - cold) * x / (width - 1));
                c->set_elevation(0);
                c->set_glucose(500);
                c->set_water(1000);
                c->take_o2(MaxO2);
                c->give_o2(1000);
                c->take_co2(MaxCo2);
                c->take_calcium(MaxCaco3);
                c->take_lipids(MaxLipids);
                c->give_lipids(80);
            }
        PopsManager manager(world, logger, config);
        std::vector<std::shared_ptr<Pop>> seen;
        Json births = Json::array(), samples = Json::array();
        for (int i = 0; i < founders; ++i) {
            auto p = std::make_shared<Pop>(world, logger, config, Genome(0, Genome::encoding_from_config(*config)),
                                           PhyT{mitochondria, 0, 1, 80});
            int x = 1 + (7 * i) % 22, y = i; // One founder per row: no initial patrol traffic jam.
            if (placement)
                x = width - 1 - x;
            if (!p->try_spawn({x, y}))
                throw std::runtime_error("garden spawn failed");
            p->lazyness_ = 0;
            p->last_direction_ = {placement ? -1 : 1, 0};
            manager.pops_.push_back(p);
            seen.push_back(p);
        }
        uint64_t attempts = 0, successes = 0, cap_ticks = 0, exposure = 0, glucose_uptake = 0;
        unsigned ticks = 0;
        const auto start = std::chrono::steady_clock::now();
        for (unsigned tick = 1; tick <= 2000 && !manager.pops_.empty(); ++tick) {
            ticks = tick;
            for (auto& p : manager.pops_) {
                const auto phase = p->age_ % 3;
                if (phase == 0)
                    p->sense();
                else if (phase == 1)
                    p->think();
                else {
                    // Explicit imposed policy, not evolved behaviour. Learning/inheritance stay enabled.
                    int action = -1;
                    if (moving) {
                        const bool west = p->pos_.x == width - 1 || (p->pos_.x != 0 && p->last_direction_.x < 0);
                        action = static_cast<int>(west ? Action::MOVE_WEST : Action::MOVE_EAST);
                    }
                    p->last_action_ = action;
                    const double before = p->lifetime_distance_;
                    p->act();
                    attempts += moving;
                    successes += p->lifetime_distance_ > before;
                }
                glucose_uptake += std::min(5u, world->get_cell(p->pos_)->get_glucose());
                p->update();
                if (!std::isfinite(p->energy_) || !std::isfinite(p->temperature_))
                    throw std::runtime_error("nonfinite garden state");
                ++exposure;
            }
            for (auto& p : manager.pops_)
                if (!p->alive_)
                    world->remove_entity(p);
            std::erase_if(manager.pops_, [](const auto& p) { return !p->alive_; });
            // Real FIFO/cost/donation/placement; no forced births or periodic culling in this fixture.
            manager.forced_respawn_active_ = false;
            manager.collect_reproducer_candidates();
            manager.process_reproduction_backlog();
            for (auto& p : manager.pops_) {
                if (std::find(seen.begin(), seen.end(), p) != seen.end())
                    continue;
                auto parent =
                    std::find_if(seen.begin(), seen.end(), [&](const auto& q) { return q->pop_id_ == p->parent_id_; });
                if (parent == seen.end() || p->birth_was_forced_)
                    throw std::runtime_error("invalid birth");
                births.push_back({{"tick", tick},
                                  {"serial", seen.size()},
                                  {"parent", std::distance(seen.begin(), parent)},
                                  {"energy", p->energy_}});
                p->lazyness_ = 0;
                p->last_direction_ = {placement ? -1 : 1, 0};
                seen.push_back(p);
            }
            if (tick % 500 == 0 || manager.pops_.empty()) {
                double gain = 0, costs = 0, alive_energy = 0;
                uint64_t reactions = 0;
                for (const auto& q : seen) {
                    gain += q->total_respiration_energy_gain_;
                    costs += q->total_metabolism_energy_loss_ + q->total_movement_energy_loss_ +
                             q->total_reproduction_energy_loss_;
                    reactions += q->total_respiration_count_;
                    if (q->alive_)
                        alive_energy += q->energy_;
                }
                samples.push_back({{"tick", tick},
                                   {"alive", manager.pops_.size()},
                                   {"births", births.size()},
                                   {"energy_gain", gain},
                                   {"costs", costs},
                                   {"alive_energy", alive_energy},
                                   {"reactions", reactions}});
            }
            // Buckets unused: local age drives each ordered sense/think/act cycle above.
            manager.sense_bucket_.clear();
            manager.think_bucket_.clear();
            manager.act_bucket_.clear();
            cap_ticks += manager.pops_.size() == config->get_max_population_allowed();
            if (std::chrono::steady_clock::now() - start > std::chrono::seconds(30))
                throw std::runtime_error("garden time budget exceeded");
        }
        Json individuals = Json::array();
        double energy_total = 0, gain = 0, cost = 0, cap_loss = 0, roundoff = 0;
        long long glucose = 0, oxygen = 0, respirations = 0;
        unsigned thermal = 0, energetic = 0, alive_founders = 0, alive_children = 0;
        for (const auto& c : world->get_cells()) {
            glucose += c->get_glucose();
            oxygen += c->get_organics().o2;
        }
        for (std::size_t i = 0; i < seen.size(); ++i) {
            auto& p = seen[i];
            glucose += p->glucose_;
            oxygen += p->o2_;
            respirations += p->total_respiration_count_;
            energy_total += p->energy_;
            gain += p->total_respiration_energy_gain_;
            cost +=
                p->total_metabolism_energy_loss_ + p->total_movement_energy_loss_ + p->total_reproduction_energy_loss_;
            cap_loss += p->total_energy_cap_loss_;
            roundoff += p->total_energy_roundoff_;
            thermal += p->death_cause_ == DeathCause::TemperatureOutOfRange;
            energetic += p->death_cause_ == DeathCause::EnergyDepletion;
            alive_founders += i < founders && p->alive_;
            alive_children += i >= founders && p->alive_;
            individuals.push_back({{"serial", i},
                                   {"alive", p->alive_},
                                   {"age", p->age_},
                                   {"energy", p->energy_},
                                   {"glucose", p->glucose_},
                                   {"offspring", p->offspring_count_},
                                   {"temperature", p->temperature_},
                                   {"death", static_cast<int>(p->death_cause_)}});
        }
        const auto glucose_residual = glucose + respirations - (width * width * 500LL + founders * 100);
        const auto oxygen_residual = oxygen + respirations - (width * width * 1000LL + founders * 20);
        const double energy_residual = energy_total - (founders * 100 + gain - cost - cap_loss + roundoff);
        if (glucose_residual || oxygen_residual || std::abs(energy_residual) > 1e-4)
            throw std::runtime_error("garden ledger failed: " + std::to_string(energy_residual));
        return {{"cold", cold},
                {"warm", warm},
                {"mitochondria", mitochondria},
                {"moving", moving},
                {"placement", placement},
                {"ticks", ticks},
                {"founders_alive", alive_founders},
                {"children_alive", alive_children},
                {"natural_births", births.size()},
                {"thermal_deaths", thermal},
                {"energy_deaths", energetic},
                {"exposure", exposure},
                {"attempts", attempts},
                {"successes", successes},
                {"cap_ticks", cap_ticks},
                {"glucose_uptake", glucose_uptake},
                {"respirations", respirations},
                {"energy_gain", gain},
                {"energy_costs", cost},
                {"energy_cap_loss", cap_loss},
                {"energy_residual", energy_residual},
                {"glucose_residual", glucose_residual},
                {"oxygen_residual", oxygen_residual},
                {"individuals", individuals},
                {"births", births},
                {"samples", samples}};
    }

    static Json run(const std::shared_ptr<ILogger>& logger, const std::shared_ptr<IConfig>& config, double environment,
                    unsigned mitochondria, unsigned lipids, bool supplied, bool environmental_lipids) {
        auto world = std::make_shared<GridWorld>(1, 1, logger);
        auto cell = world->get_cell({0, 0});
        cell->set_temperature(environment);
        cell->set_elevation(0);
        cell->set_glucose(500);
        cell->set_water(1000);
        cell->take_o2(MaxO2);
        cell->give_o2(1000);
        cell->take_co2(MaxCo2);
        cell->take_calcium(MaxCaco3);
        cell->take_lipids(MaxLipids);
        if (environmental_lipids)
            cell->give_lipids(80);
        auto pop = std::make_shared<Pop>(world, logger, config, Genome(0, Genome::encoding_from_config(*config)),
                                         PhyT{mitochondria, 0, 1, 80});
        if (!pop->try_spawn({0, 0}))
            throw std::runtime_error("spawn failed");
        pop->lipids_ = lipids;
        unsigned zero_efficiency = 0, eligible_ticks = 0, first_eligible = 0;
        unsigned glucose_supplied = 0, oxygen_supplied = 0;
        double peak_temperature = pop->temperature_, minimum_temperature = pop->temperature_;
        // No cell.update: finite control has no regeneration; supplied control explicitly adds resources.
        for (unsigned tick = 1; tick <= 2000 && pop->alive_; ++tick) {
            if (supplied) {
                glucose_supplied += cell->give_glucose(MaxC6h12o6);
                oxygen_supplied += cell->give_o2(MaxO2);
            }
            zero_efficiency += std::abs(pop->temperature_ - config->get_phy_opt_temperature()) >=
                               config->get_phy_efficiency_half_range_c();
            pop->update();
            peak_temperature = std::max(peak_temperature, pop->temperature_);
            minimum_temperature = std::min(minimum_temperature, pop->temperature_);
            if (pop->wants_to_reproduce()) {
                ++eligible_ticks;
                if (!first_eligible)
                    first_eligible = tick;
            }
        }
        const double energy_expected = 100.0 + pop->total_respiration_energy_gain_ -
                                       pop->total_metabolism_energy_loss_ - pop->total_energy_cap_loss_ +
                                       pop->total_energy_roundoff_;
        const double residual = pop->energy_ - energy_expected;
        const double heat_residual =
            pop->total_respiration_heat_ - pop->total_respiration_count_ * static_cast<double>(static_cast<float>(
                                                                               config->get_phy_heat_per_respiration()));
        // die() transfers reserves; rejected overflow remains in the dead Pop for ledger accounting.
        const long long glucose_residual = static_cast<long long>(pop->glucose_) + cell->get_glucose() +
                                           pop->total_respiration_count_ - 600LL - glucose_supplied;
        const long long oxygen_residual = static_cast<long long>(pop->o2_) + cell->get_organics().o2 +
                                          pop->total_respiration_count_ - 1020LL - oxygen_supplied;
        if (!std::isfinite(pop->temperature_) || !std::isfinite(pop->energy_) || std::abs(residual) > 1e-5 ||
            glucose_residual != 0 || oxygen_residual != 0 || std::abs(heat_residual) > 1e-8)
            throw std::runtime_error("thermal fixture ledger failed");
        return {{"environment", environment},
                {"mitochondria", mitochondria},
                {"lipids", lipids},
                {"supplied", supplied},
                {"environmental_lipids", environmental_lipids ? 80 : 0},
                {"alive", pop->alive_},
                {"observed_ticks", pop->age_},
                {"death_cause", static_cast<int>(pop->death_cause_)},
                {"temperature", pop->temperature_},
                {"peak_temperature", peak_temperature},
                {"minimum_temperature", minimum_temperature},
                {"energy", pop->energy_},
                {"respirations", pop->total_respiration_count_},
                {"energy_produced", pop->total_respiration_energy_gain_},
                {"energy_cap_loss", pop->total_energy_cap_loss_},
                {"outside_legacy_efficiency_band_ticks", zero_efficiency},
                {"first_eligible_tick", first_eligible},
                {"eligible_ticks", eligible_ticks},
                {"glucose_supplied", glucose_supplied},
                {"oxygen_supplied", oxygen_supplied},
                {"energy_residual", residual},
                {"heat_residual", heat_residual},
                {"glucose_residual", glucose_residual},
                {"oxygen_residual", oxygen_residual}};
    }
};
int main(int argc, char** argv) {
    try {
        if (argc != 3 && argc != 4)
            throw std::runtime_error("usage: thermal_comparison CONFIG OUTPUT [moderate]");
        auto logger = std::make_shared<ConsoleLogger>();
        logger->set_level(LogLevel::Error);
        Json report;
        report["respiration_model"] = "lineages_genetic_sensing_v7";
        std::ifstream config_file(argv[1]);
        report["config_contents"] = std::string(std::istreambuf_iterator<char>(config_file), {});
        report["protocol"] = "2000 ticks; isolated immobile individual; no learning/reproduction/photosynthesis; "
                             "finite or explicit glucose/O2 supply; no world regeneration. All effective state "
                             "deterministic; setup RNG is unused by physiology.";
        report["trials"] = Json::array();
        const bool moderate = argc == 4 && std::string(argv[3]) == "moderate";
        if (moderate) {
            report["moderate_protocol"] =
                "24 founders; 24x24 finite uniform resources (500 glucose,1000 oxygen/water,80 lipids per cell), "
                "linear x temperature gradient; 2000 ticks max; no photo/regeneration/forced births/NewGen; mutation "
                "zero only in fixture; learned weight inheritance unchanged; imposed stationary or horizontal patrol; "
                "one founder per row with mirrored starts; no effective RNG draws. Children retain production "
                "donation/initialization; unused directions/lazyness normalized.";
            for (const std::string variant :
                 {"baseline", "heat0.1", "heat0.2", "alpha_min0.02", "alpha_min0.05", "heat0.1_alpha0.02"}) {
                auto config = std::make_shared<ThermalConfig>(argv[1], variant);
                for (bool supplied : {false, true})
                    for (double environment : {18., 22., 25., 28., 32.})
                        for (unsigned mitochondria : {1u, 5u, 25u}) {
                            auto trial =
                                SimulationAudit::run(logger, config, environment, mitochondria, 20, supplied, true);
                            trial["variant"] = variant;
                            report["trials"].push_back(trial);
                        }
            }
            report["gardens"] = Json::array();
            for (const std::string variant : {"baseline", "heat0.1", "alpha_min0.02", "heat0.1_alpha0.02"}) {
                auto config = std::make_shared<ModerateConfig>(argv[1], variant);
                for (const auto climate : {std::pair{0., 50.}, std::pair{18., 32.}, std::pair{25., 32.}})
                    for (unsigned mitochondria : {1u, 5u, 25u})
                        for (bool moving : {false, true})
                            for (unsigned placement : {0u, 1u}) {
                                auto trial = SimulationAudit::garden(logger, config, climate.first, climate.second,
                                                                     mitochondria, moving, placement);
                                trial["variant"] = variant;
                                report["gardens"].push_back(trial);
                            }
            }
        } else {
            for (const std::string variant :
                 {"baseline", "initial37", "heat0.03", "half_range20", "alpha_min0.05", "cell_lipids80"}) {
                auto config = std::make_shared<ThermalConfig>(argv[1], variant);
                for (bool supplied : {false, true})
                    for (double environment : {0., 15., 25., 37., 50.})
                        for (unsigned mitochondria : {1u, 5u, 25u})
                            for (unsigned lipids : {20u, 80u}) {
                                auto trial = SimulationAudit::run(logger, config, environment, mitochondria, lipids,
                                                                  supplied, variant == "cell_lipids80");
                                trial["variant"] = variant;
                                report["trials"].push_back(trial);
                            }
            }
        }
        persist_diagnostic(report, "thermal_fixture");
        std::cout << "PASS: " << report["trials"].size() << " isolated controls; "
                  << (report.contains("gardens") ? report["gardens"].size() : 0) << " gardens; all ledgers PASS\n";
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}

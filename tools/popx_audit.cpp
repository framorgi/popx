#include "diagnostic_output.h"
// Short headless diagnostics using the production simulation, without GUI timing.
#include "brain.h"
#include "cell.h"
#include "config.h"
#include "console_logger.h"
#include "grid_world.h"
#include "pop.h"
#include "pops_manager.h"
#include "simulator.h"

#include <array>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <map>
#include <nlohmann/json.hpp>
#include <set>
#include <string>
#include <vector>

using Json = nlohmann::json;

class AuditWorld : public GridWorld {
  public:
    AuditWorld(int width, int height, const std::shared_ptr<ILogger>& logger) : GridWorld(width, height, logger) {}
    bool update_cycle() override {
        const auto before = get_total_organics();
        const bool result = GridWorld::update_cycle();
        const auto after = get_total_organics();
        glucose_regenerated += static_cast<long long>(after.c6h12o6) - before.c6h12o6;
        oxygen_regenerated += static_cast<long long>(after.o2) - before.o2;
        return result;
    }
    long long glucose_regenerated = 0;
    long long oxygen_regenerated = 0;
};

class SimulationAudit {
  public:
    static std::array<long long, 6> resources(const Pop& p) {
        return {p.glucose_, p.water_, p.calcium_, p.co2_, p.o2_, p.lipids_};
    }
    static std::array<long long, 6> resources(const ICell& c) {
        const auto r = c.get_organics();
        return {r.c6h12o6, r.h2o, r.caco3, r.co2, r.o2, r.lipids};
    }
    static std::array<long long, 6> total(const Pop& p, const ICell& c) {
        auto a = resources(p);
        const auto b = resources(c);
        for (int i = 0; i < 6; ++i)
            a[i] += b[i];
        return a;
    }
    static Json genes(const Genome& g) {
        Json out = Json::array();
        for (const auto& x : g.get_genes())
            out.push_back(
                {x.get_source_type(), x.get_source_num(), x.get_sink_type(), x.get_sink_num(), x.get_weight()});
        return out;
    }
    static Genome action_genome(Action action) {
        Genome g(0);
        g.add_gene(Gene(0, 0, 7, static_cast<unsigned>(action), 8192));
        return g;
    }
    static std::vector<int> signature(const Genome& genome, const std::shared_ptr<IConfig>& config) {
        Brain brain(config);
        brain.wire(genome);
        std::vector<int> out;
        std::vector<float> input(config->get_brain_size_s(), 0.0f);
        out.push_back(brain.feed_forward(input));
        for (float x : {0.25f, 0.5f, 0.75f, 1.0f}) {
            std::fill(input.begin(), input.end(), x);
            out.push_back(brain.feed_forward(input));
        }
        for (unsigned i = 0; i < input.size(); ++i) {
            std::fill(input.begin(), input.end(), 0.0f);
            input[i] = 1.0f;
            out.push_back(brain.feed_forward(input));
        }
        return out;
    }
    static double agreement(const std::vector<int>& a, const std::vector<int>& b) {
        unsigned equal = 0;
        for (unsigned i = 0; i < a.size(); ++i)
            equal += a[i] == b[i];
        return static_cast<double>(equal) / a.size();
    }
    static Json probes(std::shared_ptr<ILogger> logger, std::shared_ptr<IConfig> config) {
        Json out;
        Cell empty;
        empty.take_glucose(MaxC6h12o6);
        out["give_glucose_empty"] = {
            {"requested", 10}, {"returned", empty.give_glucose(10)}, {"stored", empty.get_glucose()}};
        Cell cap;
        cap.set_water(MaxH2o - 2);
        const auto old = cap.get_water();
        const auto returned = cap.give_water(10);
        out["give_water_capacity"] = {{"returned", returned}, {"actual_increase", cap.get_water() - old}};
        for (Action action : {Action::LEAVE_GLUCOSE, Action::LEAVE_H2O, Action::LEAVE_CALCIUM}) {
            auto w = std::make_shared<GridWorld>(3, 3, logger);
            auto c = w->get_cell(PositionT{1, 1});
            c->take_glucose(MaxC6h12o6);
            c->set_glucose(50);
            c->take_water(MaxH2o);
            c->set_water(50);
            c->take_calcium(MaxCaco3);
            auto p = std::make_shared<Pop>(w, logger, config, action_genome(action), PhyT{1, 0, 1, 80});
            p->try_spawn({1, 1});
            p->glucose_ = 100;
            p->water_ = 100;
            p->calcium_ = 100;
            const auto before = total(*p, *c);
            p->sense();
            p->think();
            p->act();
            const auto after = total(*p, *c);
            std::array<long long, 6> delta{};
            for (int i = 0; i < 6; ++i)
                delta[i] = after[i] - before[i];
            out["release_actions"].push_back({{"action", static_cast<int>(action)}, {"total_delta", delta}});
        }
        auto w = std::make_shared<GridWorld>(3, 3, logger);
        auto c = w->get_cell(PositionT{1, 1});
        c->take_glucose(MaxC6h12o6);
        c->take_o2(MaxO2);
        c->take_co2(MaxCo2);
        c->take_water(MaxH2o);
        c->take_calcium(MaxCaco3);
        c->take_lipids(MaxLipids);
        auto p = std::make_shared<Pop>(w, logger, config, Genome(0), PhyT{1, 0, 1, 80});
        p->try_spawn({1, 1});
        p->temperature_ = 37;
        p->glucose_ = 10;
        p->o2_ = 10;
        p->co2_ = 0;
        const auto before = total(*p, *c);
        p->energy_ = 0.0f; // Full demand at optimal temperature: exactly one reaction.
        const auto e = p->energy_;
        p->run_mitochondrions();
        const auto after = total(*p, *c);
        out["one_respiration"] = {{"glucose_delta", after[0] - before[0]},
                                  {"o2_delta", after[4] - before[4]},
                                  {"co2_delta", after[3] - before[3]},
                                  {"energy_gain", p->energy_ - e}};
        const auto pre_cost = p->energy_;
        p->add_reproduction_energy_loss(0.3f);
        out["reproduction_cost"] = {{"energy_before", pre_cost},
                                    {"energy_after", p->energy_},
                                    {"recorded", p->total_reproduction_energy_loss_}};
        p->energy_ = 100.0f; // Independent birth fixture: sufficient parental reserve.
        auto manager = std::make_shared<PopsManager>(w, logger, config);
        manager->pops_.push_back(p);
        const auto parent_before = resources(*p);
        const auto energy_before = p->energy_;
        manager->try_reproduce(p);
        if (manager->pops_.size() == 2) {
            auto child = manager->pops_.back();
            auto combined = resources(*p);
            const auto cr = resources(*child);
            std::array<long long, 6> delta{};
            for (int i = 0; i < 6; ++i)
                delta[i] = combined[i] + cr[i] - parent_before[i];
            out["one_birth"] = {{"material_delta", delta},
                                {"parent_energy_before", energy_before},
                                {"parent_energy_after", p->energy_},
                                {"child_energy", child->energy_},
                                {"energy_delta", p->energy_ + child->energy_ - energy_before}};
        }
        const auto before_blocked_birth = p->energy_;
        const auto population_before = manager->pops_.size();
        for (const auto& cell : w->get_cells())
            cell->give_calcium(MaxCaco3);
        manager->try_reproduce(p);
        out["blocked_birth"] = {{"energy_unchanged", p->energy_ == before_blocked_birth},
                                {"population_unchanged", manager->pops_.size() == population_before}};
        for (const auto& cell : w->get_cells())
            cell->take_calcium(MaxCaco3);
        const float saved_energy = p->energy_;
        p->energy_ = 0.1f;
        manager->try_reproduce(p);
        out["unaffordable_birth"] = {{"population_unchanged", manager->pops_.size() == population_before},
                                     {"parent_energy", p->energy_}};
        p->energy_ = saved_energy;
        // Capacity must prevent loss/creation in live transfers; overflow at death is an explicit sink.
        c->set_elevation(config->get_phy_photo_min_elevation());
        c->take_o2(MaxO2);
        c->give_o2(MaxO2);
        p->phy_.chloroplasts = std::max(config->get_phy_min_chloroplasts(), config->get_phy_max_chloroplasts_ref());
        p->water_ = 1;
        p->co2_ = 1;
        const auto photo_before = total(*p, *c);
        p->run_chloroplasts();
        const auto photo_after = total(*p, *c);
        out["photosynthesis_full_cell"] = {{"oxygen_delta", photo_after[4] - photo_before[4]},
                                           {"glucose_delta", photo_after[0] - photo_before[0]},
                                           {"water_delta", photo_after[1] - photo_before[1]},
                                           {"co2_delta", photo_after[3] - photo_before[3]}};
        p->phy_.chloroplasts = 0;
        p->lipids_ = p->phy_.adipose_stock_max;
        c->take_lipids(MaxLipids);
        c->give_lipids(30);
        p->update_physiology();
        out["lipid_storage"] = {{"capacity", p->phy_.adipose_stock_max},
                                {"stored", p->lipids_},
                                {"remaining_in_cell", c->get_organics().lipids}};
        c->set_glucose(MaxC6h12o6 - 1);
        p->glucose_ = 10;
        p->die();
        const auto after_death = resources(*c);
        p->die();
        out["death"] = {{"glucose_overflow_sink", p->glucose_},
                        {"cell_glucose", c->get_glucose()},
                        {"idempotent", resources(*c) == after_death}};
        bool ok = out["give_glucose_empty"]["returned"] == 10 && out["give_glucose_empty"]["stored"] == 10;
        ok = ok && out["give_water_capacity"]["returned"] == out["give_water_capacity"]["actual_increase"];
        for (const auto& a : out["release_actions"])
            for (const auto& d : a["total_delta"])
                ok = ok && d == 0;
        ok = ok && out["one_respiration"]["co2_delta"] == 1;
        ok = ok && std::abs(out["reproduction_cost"]["energy_before"].get<double>() -
                            out["reproduction_cost"]["energy_after"].get<double>() - 0.3) < 1e-4;
        if (!out.contains("one_birth"))
            ok = false;
        else {
            for (const auto& d : out["one_birth"]["material_delta"])
                ok = ok && d == 0;
            ok = ok && std::abs(out["one_birth"]["energy_delta"].get<double>() +
                                config->get_phy_energy_per_respiration()) < 1e-4;
        }
        ok = ok && out["photosynthesis_full_cell"]["oxygen_delta"] == 1;
        ok = ok && out["lipid_storage"]["stored"] == out["lipid_storage"]["capacity"] &&
             out["lipid_storage"]["remaining_in_cell"] == 30;
        ok = ok && out["death"]["idempotent"].get<bool>() && out["death"]["glucose_overflow_sink"] == 9;
        ok = ok && out["blocked_birth"]["energy_unchanged"].get<bool>() &&
             out["blocked_birth"]["population_unchanged"].get<bool>();
        ok = ok && out["unaffordable_birth"]["population_unchanged"].get<bool>();
        const std::array<unsigned, 6> capacities = {MaxC6h12o6, MaxH2o, MaxCaco3, MaxCo2, MaxO2, MaxLipids};
        using Transfer = unsigned (ICell::*)(unsigned);
        const std::array<Transfer, 6> give = {&ICell::give_glucose, &ICell::give_water, &ICell::give_calcium,
                                              &ICell::give_co2,     &ICell::give_o2,    &ICell::give_lipids};
        const std::array<Transfer, 6> take = {&ICell::take_glucose, &ICell::take_water, &ICell::take_calcium,
                                              &ICell::take_co2,     &ICell::take_o2,    &ICell::take_lipids};
        Cell capacity_fixture;
        for (int i = 0; i < 6; ++i) {
            (capacity_fixture.*take[i])(std::numeric_limits<unsigned>::max());
            const auto accepted = (capacity_fixture.*give[i])(std::numeric_limits<unsigned>::max());
            const auto rejected = (capacity_fixture.*give[i])(10);
            ok = ok && accepted == capacities[i] && rejected == 0;
        }
        out["all_capacity_and_overflow_checks"] = ok;
        for (const auto& [count, obstructed] :
             std::array<std::pair<unsigned, bool>, 3>{{{3, false}, {10, false}, {10, true}}}) {
            auto nw = std::make_shared<GridWorld>(4, 4, logger);
            for (const auto& cell : nw->get_cells())
                for (int i = 0; i < 6; ++i)
                    (cell.get()->*take[i])(std::numeric_limits<unsigned>::max());
            PopsManager nm(nw, logger, config);
            std::array<long long, 6> nb{};
            for (unsigned i = 0; i < count; ++i) {
                auto np = std::make_shared<Pop>(nw, logger, config, Genome(0), PhyT{1, 0, 1, 80});
                np->try_spawn({static_cast<int>(i % 4), static_cast<int>(i / 4)});
                np->energy_ = 100.0f + i;
                nm.pops_.push_back(np);
                const auto r = resources(*np);
                for (int j = 0; j < 6; ++j)
                    nb[j] += r[j];
            }
            if (obstructed) {
                for (const auto& cell : nw->get_cells()) {
                    cell->give_calcium(MaxCaco3);
                    const auto r = resources(*cell);
                    for (int i = 0; i < 6; ++i)
                        nb[i] += r[i];
                }
            }
            nm.trigger_new_generation();
            std::array<long long, 6> na{};
            for (const auto& cell : nw->get_cells()) {
                const auto r = resources(*cell);
                for (int i = 0; i < 6; ++i)
                    na[i] += r[i];
            }
            for (const auto& np : nm.pops_) {
                const auto r = resources(*np);
                for (int i = 0; i < 6; ++i)
                    na[i] += r[i];
            }
            std::array<long long, 6> nd{};
            for (int i = 0; i < 6; ++i) {
                nd[i] = na[i] - nb[i];
                ok = ok && nd[i] == 0;
            }
            ok = ok && (count != 3 || nm.pops_.size() == 3);
            out["newgen_checks"].push_back({{"initial_population", count},
                                            {"obstructed", obstructed},
                                            {"survivors", nm.pops_.size()},
                                            {"deaths", nm.total_deaths_},
                                            {"material_delta", nd}});
        }
        out["conservation_checks_passed"] = ok;
        return out;
    }
    static Json thermal_probe(const std::shared_ptr<ILogger>& logger, const std::shared_ptr<IConfig>& config) {
        Json trials = Json::array();
        for (double environment : {0.0, 25.0, 37.0})
            for (unsigned mitochondria : {1u, 5u, 25u})
                for (unsigned lipid : {20u, 80u}) {
                    auto world = std::make_shared<GridWorld>(3, 3, logger);
                    auto cell = world->get_cell(PositionT{1, 1});
                    cell->set_temperature(environment);
                    cell->set_elevation(0);
                    cell->take_lipids(MaxLipids);
                    cell->take_water(MaxH2o);
                    cell->take_co2(MaxCo2);
                    cell->take_calcium(MaxCaco3);
                    auto pop = std::make_shared<Pop>(world, logger, config, Genome(0), PhyT{mitochondria, 0, 1, 80});
                    pop->try_spawn({1, 1});
                    pop->lipids_ = lipid;
                    for (unsigned tick = 0; tick < 150 && pop->alive_; ++tick) {
                        cell->set_glucose(MaxC6h12o6);
                        cell->give_o2(MaxO2);
                        pop->update(); // No sense/think/act: physiology-only diagnostic, no neural strategy.
                    }
                    trials.push_back({{"environment", environment},
                                      {"mitochondria", mitochondria},
                                      {"lipids", lipid},
                                      {"ticks", pop->age_},
                                      {"alive", pop->alive_},
                                      {"body_temperature", pop->temperature_},
                                      {"death_cause", static_cast<int>(pop->death_cause_)},
                                      {"respirations", pop->total_respiration_count_}});
                }
        return trials;
    }

    static Json run(const std::shared_ptr<ILogger>& logger, const std::shared_ptr<IConfig>& config, unsigned limit) {
        auto world = std::make_shared<AuditWorld>(MapSize, MapSize, logger);
        auto manager = std::make_shared<PopsManager>(world, logger, config);
        auto capture = std::make_unique<DiagnosticCapture>();
        auto* captured = capture.get();
        auto central = std::make_shared<Stats>(std::move(capture));
        central->begin_session({{"kind", "audit_native"}});
        manager->set_stats(central);
        Simulator simulator(world, manager, logger);
        simulator.init();
        central->sample(0, *manager, *world);
        Json out;
        out["requested_tick_limit"] = limit;
        unsigned registered = 0;
        for (const auto& p : manager->pops_)
            registered += world->get_cell(p->pos_)->get_occupant().lock().get() == p.get();
        out["initial_alive"] = manager->pops_.size();
        out["initial_registered"] = registered;
        if (registered != manager->pops_.size()) {
            out["ticks_executed"] = 0;
            out["stop_reason"] = "population_grid_invariant_failed";
            return out;
        }
        std::map<std::string, std::shared_ptr<Pop>> seen;
        std::set<std::vector<int>> signatures;
        const auto initial_world = world->get_total_organics();
        std::array<long long, 6> initial_total = {initial_world.c6h12o6, initial_world.h2o, initial_world.caco3,
                                                  initial_world.co2,     initial_world.o2,  initial_world.lipids};
        double initial_energy = 0;
        Json initial = Json::array();
        for (const auto& p : manager->pops_) {
            initial_energy += p->energy_;
            const auto r = resources(*p);
            for (int i = 0; i < 6; ++i)
                initial_total[i] += r[i];
            seen[p->pop_id_] = p;
            auto sig = signature(p->genome_, config);
            signatures.insert(sig);
            initial.push_back(
                {{"id", p->pop_id_},
                 {"genes", genes(p->genome_)},
                 {"signature", sig},
                 {"pos", {p->pos_.x, p->pos_.y}},
                 {"phy",
                  {p->phy_.mitochondrions, p->phy_.chloroplasts, p->phy_.sensitiveness, p->phy_.adipose_stock_max}},
                 {"lazyness", p->lazyness_},
                 {"direction", {p->last_direction_.x, p->last_direction_.y}}});
        }
        Json field = Json::array();
        for (const auto& c : world->get_cells())
            field.push_back({{"resources", resources(*c)},
                             {"temperature", c->get_temperature()},
                             {"elevation", c->get_elevation()}});
        out["initial_environment"] = field;
        out["initial_genomes"] = initial;
        out["initial_unique_action_signatures"] = signatures.size();
        std::array<uint64_t, 21> requests{};
        uint64_t movement_attempts = 0, movement_successes = 0, forced_births = 0, normal_births = 0;
        Json samples = Json::array(), births = Json::array();
        unsigned tick = 0;
        auto start = std::chrono::steady_clock::now();
        for (tick = 1; tick <= limit; ++tick) {
            std::vector<std::pair<std::shared_ptr<Pop>, double>> moves;
            for (const auto& p : manager->act_bucket_)
                if (p->alive_ && p->next_phase_ == State::ACT) {
                    int a = p->last_action_;
                    if (a >= 0 && a < 21)
                        ++requests[a];
                    if (a >= 0 && a < 8) {
                        ++movement_attempts;
                        moves.emplace_back(p, p->lifetime_distance_);
                    }
                }
            // Production update; per-agent accounting below is observational, not an action override.
            central->set_tick(tick);
            simulator.update();
            for (const auto& [p, d] : moves)
                movement_successes += p->lifetime_distance_ > d;
            for (const auto& p : manager->pops_)
                if (!seen.count(p->pop_id_)) {
                    seen[p->pop_id_] = p;
                    const bool forced = p->birth_was_forced_;
                    forced_births += forced;
                    normal_births += !forced;
                    births.push_back({{"tick", tick},
                                      {"id", p->pop_id_},
                                      {"parent_id", p->parent_id_},
                                      {"forced", forced},
                                      {"energy", p->energy_},
                                      {"genes", genes(p->genome_)},
                                      {"signature", signature(p->genome_, config)}});
                }
            if (tick % 100 == 0 || manager->pops_.empty()) {
                central->sample(tick, *manager, *world);
                double distance = 0, energy = 0;
                unsigned eligible = 0;
                std::array<long long, 4> death_causes{};
                for (const auto& [id, p] : seen) {
                    distance += p->lifetime_distance_;
                    if (!p->alive_)
                        ++death_causes[static_cast<int>(p->death_cause_)];
                }
                for (const auto& p : manager->pops_) {
                    energy += p->energy_;
                    eligible += p->wants_to_reproduce();
                }
                samples.push_back({{"tick", tick},
                                   {"alive", manager->pops_.size()},
                                   {"births", manager->total_births_ - static_cast<int>(initial.size())},
                                   {"deaths", manager->total_deaths_},
                                   {"eligible", eligible},
                                   {"total_distance_all_seen", distance},
                                   {"movement_attempts", movement_attempts},
                                   {"movement_successes", movement_successes},
                                   {"mean_energy_alive", manager->pops_.empty() ? 0 : energy / manager->pops_.size()},
                                   {"death_causes", death_causes},
                                   {"backlog", manager->repro_backlog_.size()}});
                // A cohort dying before maturity with only forced births cannot establish selection.
                if (tick >= 1200 && normal_births == 0) {
                    out["stop_reason"] = "no_normal_births_after_maturation_window";
                    break;
                }
            }
            if (manager->pops_.empty()) {
                out["stop_reason"] = "extinction";
                break;
            }
            if (std::chrono::steady_clock::now() - start > std::chrono::seconds(30)) {
                out["stop_reason"] = "wall_time_budget_30s";
                break;
            }
        }
        out["ticks_executed"] = std::min(tick, limit);
        if (!out.contains("stop_reason"))
            out["stop_reason"] = "short_tick_budget";
        out["samples"] = samples;
        out["birth_events"] = births;
        out["action_requests"] = requests;
        out["movement_attempts"] = movement_attempts;
        out["movement_successes"] = movement_successes;
        out["forced_births"] = forced_births;
        out["normal_births"] = normal_births;
        Json lifetimes = Json::array();
        for (const auto& [id, p] : seen)
            lifetimes.push_back({{"id", id},
                                 {"parent_id", p->parent_id_},
                                 {"forced_birth", p->birth_was_forced_},
                                 {"alive", p->alive_},
                                 {"age", p->age_},
                                 {"offspring", p->offspring_count_},
                                 {"distance", p->lifetime_distance_},
                                 {"energy", p->energy_},
                                 {"death_cause", static_cast<int>(p->death_cause_)}});
        out["individuals"] = lifetimes;
        const auto final_world = world->get_total_organics();
        std::array<long long, 6> final_total = {final_world.c6h12o6, final_world.h2o, final_world.caco3,
                                                final_world.co2,     final_world.o2,  final_world.lipids};
        std::array<long long, 6> death_overflow{};
        long long respirations = 0, photosynthesis = 0;
        double final_energy = 0, dead_energy = 0, income = 0, metabolism = 0, movement = 0, thermo = 0,
               reproduction = 0, cap_loss = 0, roundoff = 0;
        for (const auto& [id, p] : seen) {
            const auto r = resources(*p);
            for (int i = 0; i < 6; ++i) {
                if (p->alive_)
                    final_total[i] += r[i];
                else
                    death_overflow[i] += r[i];
            }
            if (p->alive_)
                final_energy += p->energy_;
            else
                dead_energy += p->energy_;
            respirations += p->total_respiration_count_;
            photosynthesis += p->total_photosynthesis_count_;
            income += p->total_respiration_energy_gain_;
            metabolism += p->total_metabolism_energy_loss_;
            movement += p->total_movement_energy_loss_;
            thermo += p->total_thermoregolation_energy_loss_;
            reproduction += p->total_reproduction_energy_loss_;
            cap_loss += p->total_energy_cap_loss_;
            roundoff += p->total_energy_roundoff_;
        }
        const std::array<long long, 6> expected = {world->glucose_regenerated + photosynthesis - respirations,
                                                   -photosynthesis,
                                                   0,
                                                   respirations - photosynthesis,
                                                   world->oxygen_regenerated + photosynthesis - respirations,
                                                   0};
        std::array<long long, 6> residual{};
        for (int i = 0; i < 6; ++i)
            residual[i] = final_total[i] - initial_total[i] - expected[i] + death_overflow[i];
        out["resource_ledger"] = {{"initial_total", initial_total},
                                  {"final_total", final_total},
                                  {"regenerated_glucose", world->glucose_regenerated},
                                  {"regenerated_o2", world->oxygen_regenerated},
                                  {"respirations", respirations},
                                  {"photosynthesis", photosynthesis},
                                  {"death_capacity_overflow", death_overflow},
                                  {"unexplained_residual", residual}};
        const double expected_energy = income - metabolism - movement - thermo - reproduction - cap_loss - dead_energy;
        out["energy_ledger"] = {{"initial", initial_energy},
                                {"final_alive", final_energy},
                                {"respiration_gain", income},
                                {"basal_cost", metabolism},
                                {"movement_cost", movement},
                                {"thermoregulation_cost", thermo},
                                {"reproduction_cost", reproduction},
                                {"energy_cap_discarded", cap_loss},
                                {"dead_energy_sink", dead_energy},
                                {"roundoff_balance", roundoff},
                                {"residual_before_roundoff", final_energy - initial_energy - expected_energy},
                                {"unexplained_residual", final_energy - initial_energy - expected_energy - roundoff}};
        // Short common-input inheritance assay, separate from the ecosystem trajectory.
        Json assays = Json::array();
        unsigned tested = 0;
        for (const auto& [id, p] : seen) {
            if (tested++ >= 16)
                break;
            auto parent_sig = signature(p->genome_, config);
            Genome clone = p->genome_.mutated(0);
            auto clone_sig = signature(clone, config);
            const double rate = config->get_point_mutation_rate();
            auto offspring = p->make_offspring_genome();
            auto learned = p->brain_->get_learned_genome();
            assays.push_back(
                {{"id", id},
                 {"clone_original_agreement", agreement(parent_sig, clone_sig)},
                 {"offspring_vs_original_agreement", agreement(parent_sig, signature(offspring, config))},
                 {"learned_vs_original_agreement", learned ? agreement(parent_sig, signature(*learned, config)) : 1.0},
                 {"mutation_rate_assay", rate},
                 {"original_length", p->genome_.get_genes().size()},
                 {"offspring_length", offspring.get_genes().size()}});
        }
        out["inheritance_assay"] = assays;
        central->end_session(out["ticks_executed"].get<uint64_t>(), out["stop_reason"].get<std::string>(), *manager,
                             *world);
        out["central_records"] = std::move(captured->records);
        out["central_statistics"] = central->latest();
        out["wall_seconds"] = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
        return out;
    }
};

int main(int argc, char** argv) {
    if (argc < 3) {
        std::cerr << "usage: popx_audit CONFIG RECEIVER [TICKS|probes]\n";
        return 2;
    }
    auto logger = std::make_shared<ConsoleLogger>();
    logger->set_level(LogLevel::Error);
    auto config = std::make_shared<Config>(argv[1]);
    std::srand(20261014);
    Json report;
    report["metadata"] = {
        {"config", std::filesystem::absolute(argv[1]).string()},
        {"c_rng_seed", 20261014},
        {"rng_reproducible", false},
        {"resource_order", {"glucose", "water", "calcium", "co2", "o2", "lipids"}},
        {"note", "Production RNGs use random_device; this is one pilot, not a paired causal comparison."}};
    std::ifstream config_file(argv[1]);
    report["metadata"]["config_contents"] = std::string(std::istreambuf_iterator<char>(config_file), {});
    report["resource_probes"] = SimulationAudit::probes(logger, config);
    report["thermal_probe"] = SimulationAudit::thermal_probe(logger, config);
    const bool probes_only = argc > 3 && std::string(argv[3]) == "probes";
    const bool probes_passed = report["resource_probes"]["conservation_checks_passed"].get<bool>();
    if (!probes_only && probes_passed)
        report["run"] = SimulationAudit::run(logger, config, argc > 3 ? std::stoul(argv[3]) : 2000);
    else if (!probes_only)
        report["run"] = {{"ticks_executed", 0}, {"stop_reason", "resource_check_failed"}};
    bool valid = probes_passed;
    if (!probes_only) {
        const auto& run = report["run"];
        valid = valid && run.contains("resource_ledger");
        if (run.contains("resource_ledger"))
            for (const auto& value : run["resource_ledger"]["unexplained_residual"])
                valid = valid && value == 0;
        if (run.contains("energy_ledger"))
            valid = valid && std::abs(run["energy_ledger"]["unexplained_residual"].get<double>()) < 1e-5;
    }
    report["validation_passed"] = valid;
    persist_diagnostic(report, "audit");
    std::cout << (valid ? "PASS" : "FAIL") << ": conservation probes";
    if (!probes_only) {
        const auto& run = report["run"];
        std::cout << "; ticks=" << run["ticks_executed"] << "; reason=" << run["stop_reason"];
        if (run.contains("central_statistics"))
            std::cout << "; alive=" << run["central_statistics"]["alive"];
    }
    std::cout << '\n';
    return valid ? 0 : 1;
}

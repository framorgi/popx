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
#include <set>

namespace {
void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
class LearningConfig : public Config {
  public:
    LearningConfig() : Config("/dev/null") {}
    double decay = 0.00003, trace = 0.99;
    unsigned cap = 240;
    unsigned get_max_population_allowed() const override { return cap; }
    double get_learning_weight_decay() const override { return decay; }
    double get_learning_trace_decay() const override { return trace; }
    unsigned get_brain_size_s() const override { return 1; }
    unsigned get_start_population() const override { return 1; }
    unsigned get_brain_size_n() const override { return 1; }
    unsigned get_brain_size_y() const override { return 22; }
    unsigned get_brain_num_hidden_layers() const override { return 1; }
    bool get_hebbian_inheritance() const override { return true; }
};
float weight(const Brain& brain, unsigned output) {
    const auto snapshot = brain.snapshot();
    for (const auto& connection : snapshot["connections"])
        for (const auto& entry : connection["entries"])
            if (entry["row"] == output) return entry["value"].get<float>();
    throw std::runtime_error("Missing edge");
}
void learning(const std::shared_ptr<LearningConfig>& config) {
    Scorer scorer;
    ScoreObservation observation{1000, 0, 10, 100, 100, 37};
    for (int i = 0; i < 1000; ++i)
        require(scorer.observe(observation, *config) == 0, "constant reward generated learning signal");
    observation.energy = observation.previous_energy = 200;
    require(scorer.observe(observation, *config) > 0, "improvement should exceed recent baseline");
    for (int i = 0; i < 2000; ++i) scorer.observe(observation, *config);
    require(std::abs(scorer.signal()) < 1e-5, "constant reward did not become neutral");
    observation.energy = observation.previous_energy = 100;
    require(scorer.observe(observation, *config) < 0, "deterioration should be below baseline");
    scorer.reset();
    require(scorer.observe(observation, *config) == 0, "baseline survived reset");

    Genome genome(0);
    genome.add_gene(Gene(0, 0, 7, 4, 8192));
    genome.add_gene(Gene(0, 0, 7, 5, 4096));
    Brain traced(config), control(config);
    traced.wire(genome); control.wire(genome);
    require(traced.learning_stats().mean_abs_weight == 0.75, "birth weight telemetry incorrect");
    traced.feed_forward({1}); control.feed_forward({1});
    traced.record_action(4);
    for (int i = 0; i < 3; ++i) {
        traced.hebbian_update(0); control.hebbian_update(0);
    }
    traced.hebbian_update(1); control.hebbian_update(1);
    require(weight(traced, 4) > weight(control, 4), "delayed reward did not reach the executed action");
    require(weight(traced, 5) == weight(control, 5), "unchosen actuator received new credit");
    require(control.learning_stats().abs_plasticity_sum == 0, "inference without action created credit");
    require(control.learning_stats().mean_abs_plasticity == 0 && control.learning_stats().mean_abs_decay > 0,
            "plasticity telemetry confused decay with learning");
    require(std::abs(traced.learning_stats().mean_abs_plasticity -
                     config->get_learning_rate() * traced.learning_stats().mean_abs_trace) < 1e-8,
            "instantaneous plasticity does not match signal times trace");
    require(traced.learning_stats().updates == 4 && traced.learning_stats().abs_weight_change_sum > 0,
            "learning counters incorrect");
    Brain child(config);
    child.wire(*traced.get_learned_genome()); child.feed_forward({1}); child.hebbian_update(1);
    require(child.learning_stats().abs_plasticity_sum == 0, "child inherited eligibility trace");
    traced.wire(genome); traced.feed_forward({1}); traced.hebbian_update(1);
    require(traced.learning_stats().abs_plasticity_sum == 0, "rewire retained trace");
    traced.resize(1, 1, 22, 1); traced.hebbian_update(1);
    require(traced.learning_stats().updates == 0, "resize retained learning state");
}
void neutral_output_credit(const std::shared_ptr<LearningConfig>& config) {
    Genome tiny(0);
    tiny.add_gene(Gene(0, 0, 7, 4, 1));
    for (float input : {-1.0f, 1.0f}) {
        Brain positive(config), negative(config), neutral(config);
        for (auto* brain : {&positive, &negative, &neutral}) {
            brain->wire(tiny); brain->feed_forward({input}); brain->record_action(4);
        }
        positive.hebbian_update(1); negative.hebbian_update(-1); neutral.hebbian_update(0);
        require(input * (weight(positive, 4) - weight(neutral, 4)) > 0,
                "positive credit failed near neutral sigmoid");
        require(input * (weight(negative, 4) - weight(neutral, 4)) < 0,
                "negative credit failed near neutral sigmoid");
        require(neutral.learning_stats().mean_abs_plasticity == 0, "zero signal caused plastic drift");
    }
    ScoreObservation cold{1000, 0, 10, 100, 100, 17};
    const float before = Scorer::components(cold, *config).temperature;
    cold.temperature = 18;
    require(Scorer::components(cold, *config).temperature > before, "thermal reward still clipped outside band");
    cold.temperature = 37;
    require(std::abs(Scorer::components(cold, *config).temperature - Scorer::temperature_weight) < 1e-6,
            "thermal optimum changed");
}
void relocation_clears_credit() {
    auto config = std::make_shared<LearningConfig>();
    config->decay = 0;
    Genome genome(0); genome.add_gene(Gene(0, 0, 7, 4, 8192));
    Brain relocated(config), control(config);
    for (auto* brain : {&relocated, &control}) {
        brain->wire(genome);
        brain->feed_forward({1});
        brain->record_action(4);
    }
    const float before = weight(relocated, 4);
    relocated.clear_eligibility_traces();
    require(weight(relocated, 4) == before, "relocation erased learned weights");
    relocated.hebbian_update(1);
    control.hebbian_update(1);
    require(relocated.learning_stats().mean_abs_plasticity == 0 && weight(relocated, 4) == before,
            "old location retained action credit");
    require(weight(control, 4) > before, "credit control did not update");
}
void parameter_comparison() {
    Genome genome(0); genome.add_gene(Gene(0, 0, 7, 4, 8192));
    for (double decay : {1e-4, 3e-5, 1e-5}) {
        auto config = std::make_shared<LearningConfig>(); config->decay = decay;
        Brain brain(config); brain.wire(genome); brain.feed_forward({1});
        for (int tick = 0; tick < 20000; ++tick) brain.hebbian_update(0);
        const double expected = MinDecayingSynapseMagnitude +
            (1 - MinDecayingSynapseMagnitude) * std::pow(1 - decay, 20000);
        require(std::abs(weight(brain, 4) - expected) < 2e-5, "configured decay differs from recurrence");
        std::cout << "decay=" << decay << " weight_after_20000=" << weight(brain, 4) << '\n';
    }
    for (double trace : {0.9, 0.97, 0.99}) {
        auto config = std::make_shared<LearningConfig>(); config->trace = trace; config->decay = 0;
        Brain brain(config); brain.wire(genome); brain.feed_forward({1}); brain.record_action(4);
        brain.hebbian_update(0); const double initial = brain.learning_stats().mean_abs_trace;
        for (int tick = 0; tick < 30; ++tick) brain.hebbian_update(0);
        const double retained = brain.learning_stats().mean_abs_trace / initial;
        require(std::abs(retained - std::pow(trace, 30)) < 1e-6, "trace persistence differs from recurrence");
        std::cout << "trace=" << trace << " retained_after_30=" << retained << '\n';
    }
}
void mutation() {
    Genome genome(0);
    for (unsigned i = 0; i < 200; ++i) genome.add_gene(Gene(i % 7, 0x80000000u + i, 7, 1000 + i, 0));
    const auto child = genome.mutated(0, GeneEncoding{64, 64}, 1, 256);
    require(child.get_genes().size() == genome.get_genes().size(), "local mutation changed length");
    for (unsigned i = 0; i < 200; ++i) {
        const auto& before = genome.get_genes()[i]; const auto& after = child.get_genes()[i];
        require(before.get_source_type() == after.get_source_type() && before.get_source_num() == after.get_source_num()
                && before.get_sink_type() == after.get_sink_type() && before.get_sink_num() == after.get_sink_num(),
                "local mutation changed endpoints or truncated high indices");
        require(after.get_weight() != 0 && std::abs(int(after.get_weight())) <= 256, "incorrect mutation step");
        require(before.get_weight() == 0, "mutation changed parent");
    }
    const auto replacement = genome.mutated(1, GeneEncoding{64, 64}, 1, 256);
    for (const auto& gene : replacement.get_genes())
        require(gene.get_source_num() < 64 && gene.get_sink_num() < 64, "whole-gene replacement lost domains");
}
} // namespace

class SimulationAudit {
  public:
    static void admission(const std::shared_ptr<LearningConfig>& config) {
        auto logger = std::make_shared<ConsoleLogger>(); logger->set_level(LogLevel::Error);
        auto world = std::make_shared<GridWorld>(24, 24, logger);
        PopsManager manager(world, logger, config);
        for (int i = 0; i < 159; ++i) {
            auto pop = std::make_shared<Pop>(world, logger, config, Genome(0));
            pop->age_ = 2000; pop->energy_ = 100; pop->glucose_ = 100;
            manager.pops_.push_back(pop);
        }
        std::set<std::string> served;
        for (int round = 0; round < 1000; ++round) {
            manager.collect_reproducer_candidates();
            for (int n = 0; n < 3 && !manager.repro_backlog_.empty(); ++n) {
                auto entry = manager.repro_backlog_.front(); manager.repro_backlog_.pop_front();
                manager.repro_backlog_ids_.erase(entry.pop_id); served.insert(entry.pop_id);
            }
        }
        require(served.size() == 159, "admission starved persistent eligible individuals");
        std::cout << "admission: served=" << served.size() << "/159 after 1000 rounds\n";
    }
    static void reproduction(const std::shared_ptr<LearningConfig>& config) {
        config->cap = 32;
        auto logger = std::make_shared<ConsoleLogger>(); logger->set_level(LogLevel::Error);
        auto world = std::make_shared<GridWorld>(40, 40, logger);
        // Cell constructors randomize stocks; the fixture must start without accidental calcium barriers.
        for (const auto& cell : world->get_cells()) cell->take_calcium(MaxCaco3);
        PopsManager manager(world, logger, config);
        require(manager.spawn_population(), "initial founder spawn failed");
        require(manager.reproduction_stats()["births"] == 0, "founders counted as reproductive births");
        manager.reset_population_state();
        auto parent = std::make_shared<Pop>(world, logger, config, Genome(0));
        require(parent->try_spawn({10, 10}), "parent spawn failed");
        parent->age_ = 1001; parent->energy_ = 100;
        manager.pops_.push_back(parent);
        for (unsigned i = 1; i < config->get_max_population_allowed(); ++i) {
            auto pop = std::make_shared<Pop>(world, logger, config, Genome(0));
            require(pop->try_spawn({int(i % 40), int(i / 40)}), "fixture spawn failed");
            manager.pops_.push_back(pop);
        }
        manager.collect_reproducer_candidates(); manager.process_reproduction_backlog();
        auto counters = manager.reproduction_stats();
        require(counters["capacity"] == config->get_max_population_allowed(), "configured cap absent from statistics");
        require(counters["cap_blocked_agent_ticks"] == 1 && counters["attempts"] == 0,
                "cap blocks must be agent-ticks, not attempts");
        require(manager.get_repro_dropped_total() == 0, "full cap generated queue rejection noise");
        manager.pops_.pop_back();
        for (int y = 9; y <= 11; ++y)
            for (int x = 9; x <= 11; ++x)
                if (x != 10 || y != 10) world->get_cell({x, y})->give_calcium(MaxCaco3);
        manager.collect_reproducer_candidates(); manager.process_reproduction_backlog();
        counters = manager.reproduction_stats();
        require(counters["attempts"] == 1 && counters["no_space"] == 1, "space failure not counted");
        world->get_cell({10, 9})->take_calcium(MaxCaco3);
        manager.collect_reproducer_candidates(); manager.process_reproduction_backlog();
        counters = manager.reproduction_stats();
        require(counters["attempts"] == 2 && counters["no_space"] == 1 && counters["births"] == 1,
                "reproduction accounting does not balance");
        Stats stats; stats.begin_session({{"kind", "learning_fixture"}}); stats.sample(0, manager, *world);
        require(stats.latest()["learning"]["n"] == config->get_max_population_allowed(), "learning cohort incorrect");
        require(stats.latest()["reproduction"] == counters, "central stats lost reproduction counters");
        parent->brain_->feed_forward({1});
        parent->brain_->record_action(21);
        parent->brain_->hebbian_update(0);
        stats.sample(1, manager, *world);
        require(stats.latest()["totals"]["learning_updates"] == 1, "live learning counter missing");
        parent->die();
        stats.record_event({LifeEvent::Death, parent->telemetry(false)});
        manager.pops_.erase(manager.pops_.begin());
        stats.sample(2, manager, *world);
        require(stats.latest()["totals"]["learning_updates"] == 1 &&
                    stats.latest()["active"]["learning_updates"] == 0,
                "learning history disappeared on death or was counted twice");
        manager.reset_population_state();
        require(manager.reproduction_stats()["attempts"] == 0, "restart retained counters");
    }
};

int main() {
    try {
        auto config = std::make_shared<LearningConfig>();
        learning(config); neutral_output_credit(config); relocation_clears_credit(); parameter_comparison(); mutation(); SimulationAudit::admission(config); SimulationAudit::reproduction(config);
        std::cout << "PASS: baseline, delayed action credit, unchosen outputs, reset/inheritance, local mutation, cap/space/birth accounting\n";
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}

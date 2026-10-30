// Standalone deterministic tests of decision, decay, inheritance and idle physiology.
#include "brain.h"
#include "config.h"
#include "console_logger.h"
#include "grid_world.h"
#include "pop.h"

#include <cmath>
#include <iostream>
#include <stdexcept>

namespace {
void require(bool condition, const char* message) {
    if (!condition)
        throw std::runtime_error(message);
}
class NeuralConfig : public Config {
  public:
    unsigned outputs = 22;
    NeuralConfig() : Config("/dev/null") {}
    unsigned get_brain_size_s() const override { return 2; }
    unsigned get_brain_size_n() const override { return 2; }
    unsigned get_brain_num_hidden_layers() const override { return 1; }
    unsigned get_brain_size_y() const override { return outputs; }
    bool get_hebbian_inheritance() const override { return true; }
    double get_phy_init_temperature() const override { return 37; }
};
Genome weights(std::initializer_list<std::pair<unsigned, int16_t>> entries) {
    Genome genome(0);
    for (const auto& [output, weight] : entries)
        genome.add_gene(Gene(0, 0, 7, output, weight));
    return genome;
}
double weight_at(const Brain& brain, int output, int source = 0) {
    const auto snapshot = brain.snapshot();
    for (const auto& matrix : snapshot["connections"])
        if (matrix["from_layer"] == 0 && matrix["to_layer"] == 2)
            for (const auto& entry : matrix["entries"])
                if (entry["row"] == output && entry["col"] == source)
                    return entry["value"].get<double>();
    throw std::runtime_error("Expected synapse missing");
}
int inherited_weight(Brain& brain, unsigned output) {
    const auto genome = brain.get_learned_genome();
    require(bool(genome), "Learned genome missing");
    for (const auto& gene : genome->get_genes())
        if (gene.get_sink_type() == 7 && gene.get_sink_num() == output && gene.get_source_num() == 0)
            return gene.get_weight();
    throw std::runtime_error("Inherited synapse missing");
}
void decisions(const std::shared_ptr<NeuralConfig>& config) {
    Brain brain(config);
    const auto idle = static_cast<int>(Action::DO_NOTHING);
    brain.wire(Genome(0));
    require(brain.feed_forward({1, 0.5f}) == idle, "empty network moved");
    brain.wire(weights({{4, 0}, {5, 0}}));
    require(brain.feed_forward({1, 0.5f}) == idle, "zero network moved");
    brain.wire(weights({{4, 8192}, {5, 8192}}));
    const int first_tie = brain.feed_forward({1, 0.5f});
    const int second_tie = brain.feed_forward({1, 0.5f});
    require((first_tie == 4 || first_tie == 5) && (second_tie == 4 || second_tie == 5) &&
                first_tie != second_tie, "active tie did not rotate fairly");
    brain.wire(weights({{4, 8192}, {21, 8192}}));
    require(brain.feed_forward({1, 0.5f}) == 4, "tied idle suppressed an equally ranked action");
    brain.wire(weights({{4, -8192}, {5, -8192}}));
    require(brain.feed_forward({1, 0.5f}) == idle, "neutral top of inhibited network chose an arbitrary action");
    brain.wire(weights({{4, 8192}, {5, 8192}, {7, 16384}}));
    require(brain.feed_forward({1, 0.5f}) == 7, "lower tie masked a unique winner");
    brain.wire(weights({{4, 1}}));
    require(brain.feed_forward({1, 0.5f}) == 4, "weak but distinct preference was suppressed");
    brain.wire(weights({{4, 8192}, {21, 16384}}));
    require(brain.feed_forward({1, 0.5f}) == idle, "explicit idle actuator unreachable");
    require(brain.snapshot()["activations"][2][21].get<double>() >
                brain.snapshot()["activations"][2][4].get<double>(), "idle was not the neural winner");
    config->outputs = 21;
    Brain legacy(config);
    legacy.wire(Genome(0));
    require(legacy.feed_forward({1, 0.5f}) == idle, "legacy network lacks safe fallback");
    config->outputs = 1;
    Brain single(config);
    single.wire(Genome(0));
    require(single.feed_forward({1, 0.5f}) == idle, "single neutral output moved");
    single.wire(weights({{0, 8192}}));
    require(single.feed_forward({1, 0.5f}) == 0, "single active output was disabled");
    Genome inhibited(0);
    for (int i = 0; i < 64; ++i)
        inhibited.add_gene(Gene(0, 0, 7, 0, -32768));
    single.wire(inhibited);
    require(single.feed_forward({1, 0.5f}) == idle, "all-zero saturated output moved");
    config->outputs = 22;
}
void decay_and_inheritance(const std::shared_ptr<NeuralConfig>& config) {
    Brain brain(config);
    brain.wire(weights({{4, 8192}, {5, -8192}, {6, 1}, {7, -1}, {8, 0}}));
    brain.feed_forward({0.5f, 0.5f});
    for (unsigned step = 0; step < static_cast<unsigned>(20 / config->get_learning_weight_decay()); ++step)
        brain.hebbian_update(0);
    for (int output : {4, 5, 6, 7}) {
        const double value = weight_at(brain, output);
        require(std::abs(value) >= MinDecayingSynapseMagnitude, "decay crossed its magnitude floor");
        require(std::abs(value) < 1.01 * MinDecayingSynapseMagnitude, "large weight did not decay");
        const int sign = output == 4 || output == 6 ? 1 : -1;
        require(value * sign > 0 && inherited_weight(brain, output) == sign, "minimum sign lost in inheritance");
    }
    require(weight_at(brain, 8) == 0 && inherited_weight(brain, 8) == 0, "decay floor resurrected a zero");

    // Plasticity can still make a nonzero connection smaller than the decay floor.
    for (int output : {4, 5}) {
        brain.wire(weights({{4, 1}, {5, -1}, {8, 0}}));
        brain.feed_forward({1, 0.5f});
        const float before = weight_at(brain, output);
        const float post = 1 / (1 + std::exp(-before));
        const float injection = 1 - std::pow(config->get_learning_trace_decay(), 3);
        brain.record_action(output);
        brain.hebbian_update(-0.75f * before / (config->get_learning_rate() * injection * post * (1-post)));
        const double value = weight_at(brain, output);
        require(std::abs(value) > 0 && std::abs(value) < MinDecayingSynapseMagnitude / 2,
                "plasticity could not weaken below the decay floor");
        require(inherited_weight(brain, output) == (output == 4 ? 1 : -1), "subquantum sign truncated to zero");
        require(inherited_weight(brain, 8) == 0, "exact zero did not remain zero");
    }

    // Rounding avoids systematic loss of one quantum at every reproduction.
    brain.wire(weights({{4, 2}, {5, -2}, {8, 0}}));
    for (int generation = 0; generation < 100; ++generation) {
        brain.feed_forward({0.5f, 0.5f});
        brain.hebbian_update(0);
        require(inherited_weight(brain, 4) == 2 && inherited_weight(brain, 5) == -2,
                "inheritance systematically eroded quantized weights");
        const auto inherited = brain.get_learned_genome();
        brain.wire(*inherited);
    }

    auto reversal = weights({{4, 1}});
    reversal.add_gene(Gene(0, 1, 7, 4, 32767));
    brain.wire(reversal);
    for (int step = 0; step < 3; ++step) {
        brain.feed_forward({1, 1});
        brain.record_action(4);
        brain.hebbian_update(-1);
    }
    require(weight_at(brain, 4) < 0, "floor prevented an intentional Hebbian sign reversal");
}
} // namespace

class SimulationAudit {
  public:
    static void age_sensor() {
        class AgeConfig : public NeuralConfig {
          public:
            unsigned get_brain_size_s() const override { return 57; }
        };
        auto config = std::make_shared<AgeConfig>();
        auto logger = std::make_shared<ConsoleLogger>();
        logger->set_level(LogLevel::Error);
        auto world = std::make_shared<GridWorld>(3, 3, logger);
        Genome genome(0);
        genome.add_gene(Gene(0, static_cast<unsigned>(Sensor::AGE), 7, 21, 8192));
        for (const auto [index, age] : {std::pair{0, 300u}, {1, 3000u}, {2, MaxAge}}) {
            auto pop = std::make_shared<Pop>(world, logger, config, genome, PhyT{1, 0, 1, 80});
            require(pop->try_spawn({index, 1}), "age sensor fixture spawn failed");
            pop->age_ = age;
            pop->sense();
            const auto value = pop->sensor_values_[static_cast<unsigned>(Sensor::AGE)];
            const auto expected = std::log1p(float(age)) / std::log1p(float(MaxAge));
            require(std::abs(value - expected) < 1e-6f, "age sensor normalization incorrect");
            if (age == 300) require(value < .5f, "age sensor saturated at one generation");
        }
    }
    static void idle_pop(const std::shared_ptr<NeuralConfig>& config, bool explicit_idle) {
        auto logger = std::make_shared<ConsoleLogger>();
        logger->set_level(LogLevel::Error);
        auto world = std::make_shared<GridWorld>(3, 3, logger);
        for (const auto& cell : world->get_cells()) {
            cell->take_calcium(MaxCaco3);
            cell->take_glucose(MaxC6h12o6);
            cell->take_co2(MaxCo2);
            cell->set_elevation(0);
            cell->set_temperature(37);
        }
        Genome genome(0);
        if (explicit_idle)
            genome.add_gene(Gene(1, 0, 7, static_cast<unsigned>(Action::DO_NOTHING), 8192));
        auto pop = std::make_shared<Pop>(world, logger, config, genome, PhyT{1, 0, 1, 80});
        require(pop->try_spawn({1, 1}), "idle fixture spawn failed");
        pop->energy_ = 50;
        pop->glucose_ = 0;
        pop->sense();
        pop->think();
        pop->act();
        pop->act(); // The phase guard must not count an idle twice.
        require(pop->last_action_ == static_cast<int>(Action::DO_NOTHING), "idle not dispatched");
        require(pop->pos_.x == 1 && pop->pos_.y == 1 && pop->movement_attempts_ == 0 &&
                    pop->movement_successes_ == 0 && pop->total_movement_energy_loss_ == 0,
                "idle created movement or charged movement energy");
        require(pop->telemetry(false)["actions"][21] == 1, "idle action telemetry incorrect");
        const auto age = pop->age_;
        pop->update();
        require(pop->age_ == age + 1 && pop->total_metabolism_energy_loss_ > 0 && pop->energy_ < 50,
                "idle stopped normal physiology");
    }
};

int main() {
    try {
        auto config = std::make_shared<NeuralConfig>();
        decisions(config);
        decay_and_inheritance(config);
        SimulationAudit::age_sensor();
        SimulationAudit::idle_pop(config, false);
        SimulationAudit::idle_pop(config, true);
        std::cout << "PASS: neutral fallback, active tie rotation, explicit idle, age range, signed decay floor, inheritance, physiology\n";
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}

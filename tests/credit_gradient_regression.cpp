#include "brain.h"
#include "config.h"
#include "neuron.h"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <vector>

namespace {
void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

class GradientConfig : public Config {
  public:
    GradientConfig() : Config("/dev/null") {}
    unsigned get_brain_size_s() const override { return 2; }
    unsigned get_brain_size_n() const override { return 2; }
    unsigned get_brain_size_y() const override { return 22; }
    unsigned get_brain_num_hidden_layers() const override { return 2; }
    double get_learning_trace_decay() const override { return 0; }
    double get_learning_rate() const override { return 0.01; }
    double get_learning_weight_decay() const override { return 0; }
};

float edge(const Brain& brain, unsigned src, unsigned dst, unsigned col, unsigned row) {
    const auto snapshot = brain.snapshot();
    for (const auto& matrix : snapshot.at("connections"))
        if (matrix.at("from_layer") == src && matrix.at("to_layer") == dst)
            for (const auto& entry : matrix.at("entries"))
                if (entry.at("col") == col && entry.at("row") == row)
                    return entry.at("value").get<float>();
    throw std::runtime_error("Missing test edge");
}

float output(Brain& brain, const std::vector<float>& sensors) {
    brain.feed_forward(sensors);
    return brain.snapshot().at("activations").back()[4].get<float>();
}

struct Edge { unsigned src, dst, col, row; };
} // namespace

int main() {
    try {
        auto config = std::make_shared<GradientConfig>();
        Genome genome(0);
        genome.add_gene(Gene(0, 0, 1, 0, 2048));
        genome.add_gene(Gene(0, 1, 1, 1, -4096));
        genome.add_gene(Gene(1, 0, 2, 0, 8192));
        genome.add_gene(Gene(1, 1, 7, 4, 8192));
        genome.add_gene(Gene(2, 0, 7, 4, 8192));
        genome.add_gene(Gene(0, 0, 7, 4, 1024));
        genome.add_gene(Gene(0, 1, 2, 1, 2048)); // hidden neuron disconnected from output 4
        const std::vector<float> sensors{1.0f, 0.6f};
        Brain rewarded(config), punished(config);
        rewarded.wire(genome); punished.wire(genome);
        const float before = output(rewarded, sensors);
        require(rewarded.feed_forward(sensors) == 4, "fixture did not choose output 4");
        punished.feed_forward(sensors);
        rewarded.record_action(4); punished.record_action(4);
        rewarded.hebbian_update(1); punished.hebbian_update(-1);
        require(output(rewarded, sensors) > before, "positive signal weakened chosen output");
        require(output(punished, sensors) < before, "negative signal strengthened chosen output");

        constexpr Edge tested[] = {{0, 1, 0, 0}, {0, 1, 1, 1}, {1, 2, 0, 0},
                                   {1, 3, 1, 4}, {2, 3, 0, 4}, {0, 3, 0, 4}, {0, 2, 1, 1}};
        for (const auto& e : tested) {
            Brain perturbed(config), original(config);
            perturbed.wire(genome); original.wire(genome);
            const float weight_before = edge(original, e.src, e.dst, e.col, e.row);
            constexpr float epsilon = 0.01f;
            perturbed.edit_weight(e.src, e.dst, e.col, e.row, weight_before + epsilon);
            const float plus = output(perturbed, sensors);
            perturbed.edit_weight(e.src, e.dst, e.col, e.row, weight_before - epsilon);
            const float minus = output(perturbed, sensors);
            const float numeric = (plus - minus) / (2 * epsilon);
            const float observed = (edge(rewarded, e.src, e.dst, e.col, e.row) - weight_before) /
                                   static_cast<float>(config->get_learning_rate());
            require(std::abs(observed - numeric) < 2e-4f, "credit differs from finite-difference gradient");
            if (e.src == 0 && e.dst == 2 && e.row == 1)
                require(observed == 0 && numeric == 0, "disconnected hidden edge received credit");
        }

        Genome adverse(0);
        adverse.add_gene(Gene(0, 0, 1, 0, -2048));
        adverse.add_gene(Gene(1, 0, 7, 4, 32767));
        Brain prior_failure(config);
        prior_failure.wire(adverse);
        const float adverse_before = output(prior_failure, {1.0f, 0.0f});
        prior_failure.record_action(4);
        prior_failure.hebbian_update(1);
        require(output(prior_failure, {1.0f, 0.0f}) > adverse_before,
                "positive reward still reduces chosen output in prior failure fixture");
        std::cout << "PASS: hidden/skip gradients match finite differences; disconnected branch zero; "
                     "reward sign improves chosen output\n";
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}

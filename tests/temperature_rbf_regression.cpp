// Deterministic geometry fixture, independent of evolutionary runs and their RNG.
#include "random_utility.h"

#include <cmath>
#include <iostream>
#include <stdexcept>

namespace {
void require(bool condition, const char* message) {
    if (!condition)
        throw std::runtime_error(message);
}
void close(double actual, double expected, double tolerance, const char* message) {
    require(std::abs(actual - expected) <= tolerance, message);
}
double hot_fraction(RandomUtility& random, const RBFSet& field) {
    unsigned hot = 0;
    for (unsigned y = 0; y < 120; ++y)
        for (unsigned x = 0; x < 120; ++x)
            hot += random.evaluate_rbf_set(field, x, y) >= 47.0;
    return static_cast<double>(hot) / (120 * 120);
}
} // namespace

int main() {
    try {
        RandomUtility random;
        RBFSet original{90.0, 0.0, {}};
        for (unsigned i = 0; i < 10; ++i) {
            const double angle = i * 0.6283185307179586;
            original.data.push_back({50.0 + i * 4.0, 60 + 38 * std::cos(angle), 60 + 38 * std::sin(angle),
                                     6.0 + i, 8.0 + i});
        }
        auto field = original;
        constexpr unsigned updates = 100000;
        for (unsigned tick = 0; tick <= updates; ++tick) {
            field = random.rotate_rbf_set(field, tick, 20, 60, 60);
            for (unsigned i = 0; i < field.data.size(); ++i) {
                const auto& before = original.data[i];
                const auto& after = field.data[i];
                require(after.stddev_x == before.stddev_x && after.stddev_y == before.stddev_y,
                        "rotation must not widen or narrow Gaussian bells");
                require(after.amplitude == before.amplitude, "rotation changed peak amplitude");
                close(std::hypot(after.mean_x - 60, after.mean_y - 60), 38, 1e-8,
                      "center drifted away from its orbit");
            }
        }
        require(field.max_value == original.max_value && field.min_value == original.min_value,
                "rotation changed field limits");
        require(original.data.front().mean_x == 98 && original.data.front().stddev_x == 6,
                "rotation mutated its input");
        for (unsigned i = 0; i < field.data.size(); ++i) {
            const auto& before = original.data[i];
            const auto& after = field.data[i];
            close(random.evaluate_gaussian(after, after.mean_x + after.stddev_x, after.mean_y),
                  before.amplitude * std::exp(-0.5), 1e-12, "local Gaussian profile changed");
            close(random.evaluate_gaussian(after, after.mean_x, after.mean_y + after.stddev_y),
                  before.amplitude * std::exp(-0.5), 1e-12, "local vertical profile changed");
        }
        const auto zero_tick = random.rotate_rbf_set(original, 0, 20, 60, 60);
        close(zero_tick.data[0].mean_x, 98, 0, "tick zero should not rotate");
        close(zero_tick.data[0].mean_y, 60, 0, "tick zero should not rotate");
        const auto first = random.rotate_rbf_set(original, 1, 20, 60, 60);
        close(first.data[0].mean_x, 60 + 38 * std::cos(0.009), 1e-12, "wrong angular step");
        close(first.data[0].mean_y, 60 + 38 * std::sin(0.009), 1e-12, "wrong angular step");
        const auto disabled = random.rotate_rbf_set(original, 1, 0, 60, 60);
        close(disabled.data[0].mean_x, 98, 0, "zero radius must disable motion");
        require(random.rotate_rbf_set(RBFSet{0, 0, {}}, 1, 20, 60, 60).data.empty(), "empty set changed");

        // Same centers and amplitudes isolate the removed cumulative widening at 10,000 updates.
        // This is a diagnostic counterfactual, not a measurement of the running world.
        auto fixed = original;
        for (unsigned tick = 0; tick <= 10000; ++tick)
            fixed = random.rotate_rbf_set(fixed, tick, 20, 60, 60);
        auto widened = fixed;
        for (auto& bell : widened.data) {
            bell.stddev_x += 0.01 + 10000 * std::cos(0.009) * 0.01;
            bell.stddev_y += 10000 * std::sin(0.009) * 0.01;
        }
        std::cout << "Fixture cells >=47 C, fixed widths: " << hot_fraction(random, fixed)
                  << "; legacy widening at identical centers: " << hot_fraction(random, widened) << '\n';
        std::cout << "PASS: 100000 rotations preserve widths, peaks, orbits and local profiles; zero/empty cases\n";
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}

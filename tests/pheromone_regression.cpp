// Verify real emission -> typed sensing -> decision -> movement, without reward shaping.
#include "cell.h"
#include "config.h"
#include "console_logger.h"
#include "grid_world.h"
#include "pop.h"

#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace {
void require(bool condition, const char* message) {
    if (!condition)
        throw std::runtime_error(message);
}
class SignalConfig : public Config {
  public:
    SignalConfig() : Config("/dev/null") {}
    unsigned get_brain_size_s() const override {
        return static_cast<unsigned>(Sensor::NUM_SENSES);
    }
    unsigned get_brain_size_y() const override {
        return 21;
    }
    double get_phy_init_temperature() const override {
        return 37;
    }
};
constexpr FeromoneT types[] = {FOOD_FEROMONE, DANGER_FEROMONE, MATE_FEROMONE, HOME_FEROMONE};
constexpr unsigned gradient_base[] = {24, 37, 41, 45};
void clear_signals(const std::shared_ptr<GridWorld>& w) {
    for (const auto& c : w->get_cells())
        for (auto [type, value] : c->get_feromone_map())
            c->set_feromone(type, -value);
}
Genome emitter_genome(unsigned type) {
    Genome g(0);
    g.add_gene(Gene(1, 0, 7, 8 + type, 8192));
    return g;
}
Genome receiver_genome(unsigned type) {
    Genome g(0);
    constexpr unsigned outputs[] = {6, 5, 4, 7};
    for (unsigned i = 0; i < 4; ++i)
        g.add_gene(Gene(0, gradient_base[type] + i, 7, outputs[i], 8192));
    return g;
}
} // namespace

class SimulationAudit {
  public:
    static void run(const std::shared_ptr<ILogger>& logger, const std::shared_ptr<IConfig>& config) {
        for (unsigned type = 0; type < 4; ++type) {
            auto world = std::make_shared<GridWorld>(13, 13, logger);
            for (const auto& c : world->get_cells()) {
                c->set_temperature(37);
                c->set_elevation(0);
                c->take_calcium(MaxCaco3);
            }
            auto emitter = std::make_shared<Pop>(world, logger, config, emitter_genome(type), PhyT{1, 0, 1, 80});
            auto receiver = std::make_shared<Pop>(world, logger, config, receiver_genome(type), PhyT{1, 0, 1, 80});
            require(emitter->try_spawn({8, 6}) && receiver->try_spawn({6, 6}), "Signal fixture placement failed");
            receiver->lazyness_ = 0;
            emitter->sense();
            emitter->think();
            emitter->act();
            const auto intensity = world->get_feromone_strength(types[type], {8, 6});
            require(intensity >= 70 && intensity <= 100, "Emission intensity outside declared range");
            receiver->sense();
            receiver->think();
            require(receiver->sensor_values_[gradient_base[type] + 2] > 0.5f, "East gradient did not carry signal");
            require(receiver->last_action_ == static_cast<int>(Action::MOVE_EAST), "Typed signal did not select east");
            receiver->act();
            require(receiver->pos_.x == 7 && receiver->pos_.y == 6, "Receiver did not approach emitter");

            // Scalar channels remain normalized and isolated from other types.
            world->remove_entity(emitter);
            Genome local_genome(0);
            for (unsigned i = 0; i < 4; ++i)
                local_genome.add_gene(Gene(0, 20 + i, 7, 4 + i, 8192));
            auto local = std::make_shared<Pop>(world, logger, config, local_genome, PhyT{1, 0, 1, 80});
            require(local->try_spawn({8, 6}), "Local receiver placement failed");
            local->sense();
            for (unsigned i = 0; i < 4; ++i)
                require(std::abs(local->sensor_values_[20 + i] - (i == type ? intensity / MaxFeromones : 0)) < 1e-6f,
                        "Scalar channel not normalized or type leakage");
            world->get_cell({8, 6})->update();
            require(std::abs(world->get_feromone_strength(types[type], {8, 6}) - intensity * FeromoneDecayRate) < 1e-4f,
                    "Signal decay did not run");
            clear_signals(world);
            require(world->get_feromone_magnitude({8, 6}, types[type]) == 0, "Signal washout failed");

            // All four directions for this channel, with identical probe distance and zero background.
            auto probe_world = std::make_shared<GridWorld>(13, 13, logger);
            for (const auto& c : probe_world->get_cells()) {
                c->set_elevation(0);
                c->take_calcium(MaxCaco3);
            }
            auto probe = std::make_shared<Pop>(probe_world, logger, config, receiver_genome(type), PhyT{1, 0, 1, 80});
            require(probe->try_spawn({6, 6}), "Probe placement failed");
            constexpr PositionT locations[] = {{6, 4}, {4, 6}, {8, 6}, {6, 8}};
            constexpr unsigned outputs[] = {6, 5, 4, 7};
            for (unsigned direction = 0; direction < 4; ++direction) {
                clear_signals(probe_world);
                probe_world->set_feromone(locations[direction], types[type], 100);
                probe->next_phase_ = State::SENSE;
                probe->sense();
                probe->think();
                require(probe->last_action_ == static_cast<int>(outputs[direction]),
                        "Directional signal mapped incorrectly");
            }
            clear_signals(probe_world);
            for (const auto p : locations)
                probe_world->set_feromone(p, types[type], 100);
            probe_world->set_feromone({6, 6}, types[type], 100);
            probe->next_phase_ = State::SENSE;
            probe->sense();
            for (unsigned i = 0; i < 4; ++i)
                require(std::isfinite(probe->sensor_values_[gradient_base[type] + i]) &&
                            probe->sensor_values_[gradient_base[type] + i] >= 0 &&
                            probe->sensor_values_[gradient_base[type] + i] <= 1,
                        "Gradient outside finite normalized domain");
            clear_signals(probe_world);
            for (const auto& c : probe_world->get_cells())
                c->set_feromone(types[type], 50);
            probe->next_phase_ = State::SENSE;
            probe->sense();
            for (unsigned i = 0; i < 4; ++i)
                require(std::abs(probe->sensor_values_[gradient_base[type] + i] - 0.5f) < 1e-6f,
                        "Uniform field created a spurious gradient");
        }
    }
};

int main() {
    try {
        Cell cell;
        cell.set_feromone(FOOD_FEROMONE, 70);
        cell.set_feromone(FOOD_FEROMONE, 70);
        require(cell.get_feromone_map().at(FOOD_FEROMONE) == MaxFeromones, "Accumulation cap broken");
        cell.set_feromone(FOOD_FEROMONE, std::numeric_limits<float>::quiet_NaN());
        require(cell.get_feromone_map().at(FOOD_FEROMONE) == MaxFeromones, "Nonfinite emission poisoned field");
        cell.set_feromone(FOOD_FEROMONE, -static_cast<float>(MaxFeromones));
        require(cell.get_feromone_map().at(FOOD_FEROMONE) == 0, "Negative removal broken");
        auto logger = std::make_shared<ConsoleLogger>();
        logger->set_level(LogLevel::Error);
        SimulationAudit::run(logger, std::make_shared<SignalConfig>());
        std::cout
            << "PASS: four typed emitters, local sensing, 16 directional paths, approach, cap, decay and washout\n";
    } catch (const std::exception& e) {
        std::cerr << "FAIL: " << e.what() << '\n';
        return 1;
    }
}

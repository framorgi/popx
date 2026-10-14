// Standalone regression checks; link against the simulation objects, without the GUI/main.
#include "config.h"
#include "console_logger.h"
#include "grid_world.h"
#include "pop.h"
#include "pops_manager.h"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
class TestConfig : public Config {
  public:
    TestConfig() : Config("/dev/null") {}
    unsigned get_brain_size_s() const override {
        return 32;
    }
    unsigned get_brain_size_n() const override {
        return 16;
    }
    unsigned get_brain_size_y() const override {
        return 21;
    }
    unsigned get_start_population() const override {
        return 24;
    }
    unsigned get_genome_min_length() const override {
        return 1;
    }
    unsigned get_genome_max_length() const override {
        return 1;
    }
    unsigned get_phy_init_mitochondrions_max() const override {
        return 1;
    }
    unsigned get_phy_init_chloroplasts_max() const override {
        return 0;
    }
    double get_phy_init_temperature() const override {
        return 37.0;
    }
};

void require(bool condition, const std::string& message) {
    if (!condition)
        throw std::runtime_error(message);
}

bool same_position(PositionT a, PositionT b) {
    return a.x == b.x && a.y == b.y;
}

std::shared_ptr<GridWorld> make_world(const std::shared_ptr<ILogger>& logger) {
    auto world = std::make_shared<GridWorld>(9, 9, logger);
    for (const auto& cell : world->get_cells()) {
        cell->set_elevation(0.0);
        cell->set_temperature(37.0);
        cell->take_calcium(MaxCaco3);
    }
    return world;
}

Genome action_genome(Action action) {
    Genome genome(0);
    genome.add_gene(Gene(0, static_cast<unsigned>(Sensor::LOC_X), 7, static_cast<unsigned>(action), 8192));
    return genome;
}

void check_phase_guards(const std::shared_ptr<ILogger>& logger, const std::shared_ptr<IConfig>& config) {
    auto world = make_world(logger);
    auto pop = std::make_shared<Pop>(world, logger, config, action_genome(Action::MOVE_EAST));
    require(pop->try_spawn({4, 4}), "fixture spawn failed");

    pop->think();
    pop->act();
    require(same_position(pop->get_position(), {4, 4}), "think/act ran before first sense");
    pop->sense();
    pop->act();
    require(same_position(pop->get_position(), {4, 4}), "act ran before think");
    pop->think();
    pop->sense(); // Must not overwrite the sensors of a pending decision.
    pop->think();
    pop->act();
    require(same_position(pop->get_position(), {5, 4}), "sense -> think -> act did not move east");
    pop->act();
    pop->think();
    pop->act();
    require(same_position(pop->get_position(), {5, 4}), "stale decision executed twice");
    pop->sense();
    pop->think();
    pop->act();
    require(same_position(pop->get_position(), {6, 4}), "second complete cycle failed");

    pop->init();
    pop->think();
    pop->act();
    require(same_position(pop->get_position(), {6, 4}), "init retained a pending decision");
    pop->sense();
    pop->think();
    pop->act();
    require(same_position(pop->get_position(), {7, 4}), "cycle after init failed");
    pop->sense();
    pop->think();
    pop->act();
    require(same_position(pop->get_position(), {8, 4}), "cycle before boundary failed");
    pop->sense();
    pop->think();
    pop->act(); // Blocked by boundary; the next sense must still be permitted.
    require(same_position(pop->get_position(), {8, 4}), "boundary allowed an invalid move");
    pop->sense();
    require(pop->get_render_state().size < 1.0f, "failed act did not release next sense phase");
}

void check_random_direction(const std::shared_ptr<ILogger>& logger, const std::shared_ptr<IConfig>& config) {
    auto world = make_world(logger);
    for (const auto action : {Action::MOVE_FORWARD, Action::MOVE_LEFT, Action::MOVE_RIGHT}) {
        for (int sample = 0; sample < 16; ++sample) {
            const auto genome = action_genome(action);
            auto pop = sample % 2 == 0 ? std::make_shared<Pop>(world, logger, config, genome)
                                       : std::make_shared<Pop>(world, logger, config, genome, PhyT{1, 0, 1, 80});
            require(pop->try_spawn({4, 4}), "relative fixture spawn failed");
            if (sample % 3 == 0)
                pop->init();
            pop->sense();
            pop->think();
            pop->act();
            const auto p = pop->get_position();
            const int dx = p.x - 4;
            const int dy = p.y - 4;
            require(std::abs(dx) <= 1 && std::abs(dy) <= 1 && (dx != 0 || dy != 0),
                    "initial relative movement did not use a nonzero neighbouring direction");
        }
    }
}

void check_bucket_rotation(const std::shared_ptr<ILogger>& logger, const std::shared_ptr<IConfig>& config) {
    auto world = make_world(logger);
    PopsManager manager(world, logger, config);
    std::srand(17); // Only controls the existing C RNG used for bucket assignment.
    require(manager.spawn_population(), "manager spawn failed");
    std::vector<std::shared_ptr<Pop>> pops;
    for (const auto& cell : world->get_cells()) {
        if (auto pop = std::dynamic_pointer_cast<Pop>(cell->get_occupant().lock()))
            pops.push_back(pop);
    }
    require(pops.size() == 24, "manager fixture population mismatch");
    std::vector<int> first_act(pops.size(), 0);
    for (int tick = 1; tick <= 8; ++tick) {
        manager.update_cycle();
        require(manager.get_alive_count() == 24, "fixture population unexpectedly changed");
        for (std::size_t i = 0; i < pops.size(); ++i) {
            const bool acted = pops[i]->get_render_state().size == 1.0f;
            if (first_act[i] == 0 && acted) {
                require(tick >= 3 && tick <= 5, "random initial bucket skipped/reversed a phase");
                first_act[i] = tick;
            }
            if (first_act[i] != 0)
                require(acted == ((tick - first_act[i]) % 3 == 0), "bucket order is not sense -> think -> act");
        }
    }
    for (const auto tick : first_act)
        require(tick != 0, "agent never completed its first cycle");

    manager.trigger_new_generation(); // Reassigns survivor buckets at arbitrary phase boundaries.
    require(manager.get_alive_count() >= 8, "NewGen fixture lost too many survivors");
    for (int tick = 0; tick < 8; ++tick)
        manager.update_cycle();
    require(manager.get_alive_count() >= 8, "survivors failed after bucket reassignment");
}
} // namespace

int main() {
    try {
        auto logger = std::make_shared<ConsoleLogger>();
        logger->set_level(LogLevel::Error);
        auto config = std::make_shared<TestConfig>();
        check_phase_guards(logger, config);
        check_random_direction(logger, config);
        check_bucket_rotation(logger, config);
        std::cout << "PASS: phase guards, bucket rotation, initial relative movement\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}

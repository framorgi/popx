#include "config.h"
#include "console_logger.h"
#include "grid_world.h"
#include "pops_manager.h"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <stdexcept>

namespace {
void require(bool condition, const char* message) {
    if (!condition)
        throw std::runtime_error(message);
}
class LocalConfig : public Config {
  public:
    LocalConfig() : Config("/dev/null") {}
    unsigned radius = 2;
    unsigned get_brain_size_s() const override { return 57; }
    unsigned get_brain_size_y() const override { return 22; }
    unsigned get_offspring_dispersal_radius() const override { return radius; }
};
void prepare(const std::shared_ptr<GridWorld>& world) {
    for (const auto& cell : world->get_cells()) {
        cell->take_calcium(MaxCaco3);
        cell->set_elevation(0);
    }
}
} // namespace

class SimulationAudit {
  public:
    static void run() {
        auto config = std::make_shared<LocalConfig>();
        auto logger = std::make_shared<ConsoleLogger>();
        logger->set_level(LogLevel::Error);
        auto world = std::make_shared<GridWorld>(9, 9, logger);
        prepare(world);
        Genome genome(0);
        for (unsigned i = 49; i <= 56; ++i)
            genome.add_gene(Gene(0, i, 7, 0, 8192));
        genome.add_gene(Gene(0, 32, 7, 0, 8192));
        auto pop = std::make_shared<Pop>(world, logger, config, genome);
        auto blocker = std::make_shared<Pop>(world, logger, config, Genome(0));
        require(pop->try_spawn({4, 4}) && blocker->try_spawn({5, 4}), "mobility fixture spawn failed");
        pop->lazyness_ = 0;
        world->get_cell({4, 3})->set_elevation(7);
        world->get_cell({4, 5})->give_calcium(MaxCaco3);
        pop->next_phase_ = State::SENSE;
        pop->sense();
        require(pop->sensor_values_[49] == 0 && pop->sensor_values_[51] == 0 &&
                    pop->sensor_values_[53] == 0 && pop->sensor_values_[55] == 1 &&
                    pop->sensor_values_[50] == 1 && pop->sensor_values_[32] == 0,
                "directional passability missed slope, occupant, carbonate, or free cell");
        require(!pop->try_move({5, 4}), "occupied move succeeded");
        pop->next_phase_ = State::SENSE;
        pop->sense();
        require(pop->sensor_values_[32] == 1, "failed movement was not sensed");
        require(pop->try_move({3, 4}), "free move failed");
        pop->next_phase_ = State::SENSE;
        pop->sense();
        require(pop->sensor_values_[32] == 0, "successful movement did not clear feedback");
        pop->responsiveness_ = 2;
        world->get_cell({1, 4})->give_calcium(MaxCaco3);
        pop->next_phase_ = State::SENSE;
        pop->sense();
        require(pop->sensor_values_[55] == 0 && pop->sensor_values_[51] == 0,
                "passability did not use actual movement stride");

        auto policy_world = std::make_shared<GridWorld>(9, 9, logger);
        prepare(policy_world);
        Genome policy(0);
        policy.add_gene(Gene(0, 51, 7, 4, 8192)); // East is blocked.
        policy.add_gene(Gene(0, 55, 7, 5, 8192)); // West is open.
        auto walker = std::make_shared<Pop>(policy_world, logger, config, policy);
        auto obstacle = std::make_shared<Pop>(policy_world, logger, config, Genome(0));
        require(walker->try_spawn({4, 4}) && obstacle->try_spawn({5, 4}), "policy fixture spawn failed");
        walker->lazyness_ = 0;
        walker->next_phase_ = State::SENSE;
        walker->sense();
        walker->think();
        require(walker->last_action_ == static_cast<int>(Action::MOVE_WEST),
                "open-direction signal did not select the reachable action");
        walker->act();
        require(walker->get_position().x == 3 && walker->get_position().y == 4,
                "open-direction policy did not move into its target cell");

        for (unsigned free_ring : {2u, 3u, 4u}) {
            for (unsigned radius : {free_ring - 1, free_ring}) {
                config->radius = radius;
                auto birth_world = std::make_shared<GridWorld>(13, 13, logger);
                prepare(birth_world);
                auto parent = std::make_shared<Pop>(birth_world, logger, config, Genome(0));
                require(parent->try_spawn({6, 6}), "parent fixture spawn failed");
                parent->age_ = 1001;
                parent->energy_ = 100;
                for (int dy = -static_cast<int>(free_ring - 1); dy <= static_cast<int>(free_ring - 1); ++dy)
                    for (int dx = -static_cast<int>(free_ring - 1); dx <= static_cast<int>(free_ring - 1); ++dx)
                        if (dx || dy)
                            birth_world->get_cell({6 + dx, 6 + dy})->give_calcium(MaxCaco3);
                PopsManager manager(birth_world, logger, config);
                manager.pops_.push_back(parent);
                const float energy_before = parent->energy_;
                const unsigned glucose_before = parent->glucose_;
                manager.try_reproduce(parent);
                const auto stats = manager.reproduction_stats();
                if (radius < free_ring) {
                    require(manager.pops_.size() == 1 && stats["no_space"] == 1 && stats["births"] == 0 &&
                                parent->energy_ == energy_before && parent->glucose_ == glucose_before,
                            "blocked inner rings spent resources or spawned a child");
                } else {
                    require(manager.pops_.size() == 2 && stats["births"] == 1 &&
                                stats["dispersed_births"] == 1 && stats["no_space"] == 0,
                            "outer-ring offspring was not accounted for");
                    const auto pos = manager.pops_.back()->get_position();
                    require(std::max(std::abs(pos.x - 6), std::abs(pos.y - 6)) == static_cast<int>(free_ring),
                            "offspring was placed outside the first free ring");
                    require(parent->glucose_ + manager.pops_.back()->glucose_ == glucose_before &&
                                parent->energy_ + manager.pops_.back()->energy_ < energy_before,
                            "offspring dispersal created resources or free energy");
                }
            }
        }
    }
};

int main() {
    SimulationAudit::run();
    std::cout << "PASS: directional passability, movement feedback and offspring dispersal through ring four\n";
}

// Every active actuator pays for the attempt, including ineffective ones.
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
} // namespace

class SimulationAudit {
  public:
    static void run() {
        auto logger = std::make_shared<ConsoleLogger>();
        logger->set_level(LogLevel::Error);
        auto config = std::make_shared<Config>("/dev/null");
        auto world = std::make_shared<GridWorld>(7, 7, logger);
        for (const auto& cell : world->get_cells())
            cell->take_calcium(MaxCaco3);
        auto pop = std::make_shared<Pop>(world, logger, config, Genome(0), PhyT{1, 0, 1, 80});
        require(pop->try_spawn({3, 3}), "Could not spawn cost fixture");

        for (int index = 0; index <= static_cast<int>(Action::KILL_FORWARD); ++index) {
            const auto action = static_cast<Action>(index);
            const double movement_before = pop->total_movement_energy_loss_;
            const double other_before = pop->total_other_action_energy_loss_;
            const double heat_before = pop->total_thermoregolation_energy_loss_;
            const float pending_before = pop->energy_cost_;
            pop->last_action_ = index;
            pop->next_phase_ = State::ACT;
            pop->act();
            const float expected = action == Action::DO_NOTHING ? 0.0f
                                   : action == Action::BURN_CALORIES ? 0.05f
                                                                     : 0.01f;
            require(std::abs((pop->energy_cost_ - pending_before) - expected) < 1e-6f,
                    "Wrong actuator attempt cost");
            require(std::abs(pop->total_movement_energy_loss_ - movement_before -
                                 (index <= static_cast<int>(Action::MOVE_SOUTH) ? 0.01 : 0.0)) < 1e-6,
                    "Movement cost was misclassified");
            require(std::abs(pop->total_other_action_energy_loss_ - other_before -
                                 (index > static_cast<int>(Action::MOVE_SOUTH) &&
                                          action != Action::DO_NOTHING && action != Action::BURN_CALORIES
                                      ? 0.01
                                      : 0.0)) < 1e-6,
                    "Non-movement action cost was misclassified");
            require(std::abs(pop->total_thermoregolation_energy_loss_ - heat_before -
                                 (action == Action::BURN_CALORIES ? 0.05 : 0.0)) < 1e-6,
                    "Thermoregulation cost changed");
            if (action == Action::GET_CALCIUM)
                require(pop->acquired_[2] == 0 && expected > 0,
                        "Empty-cell calcium attempt escaped its cost");
        }
    }
};

int main() {
    try {
        SimulationAudit::run();
        std::cout << "PASS: active actuator attempts pay once, idle stays free, energy categories agree\n";
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}

#include "config.h"
#include "console_logger.h"
#include "grid_world.h"
#include "pop.h"
#include "random_utility.h"

#include <memory>
#include <stdexcept>

int main() {
    RandomUtility::set_diagnostic_seed(17);
    RandomUtility first, second;
    const auto first_value = first.rnd_int(0, 1000000), second_value = second.rnd_int(0, 1000000);
    RandomUtility::set_diagnostic_seed(17);
    RandomUtility repeated_first, repeated_second;
    if (first_value != repeated_first.rnd_int(0, 1000000) ||
        second_value != repeated_second.rnd_int(0, 1000000))
        throw std::runtime_error("fixture RNG did not repeat constructor streams");
    RandomUtility::clear_diagnostic_seed();
    auto logger = std::make_shared<ConsoleLogger>();
    logger->set_level(LogLevel::Error);
    auto config = std::make_shared<Config>("/dev/null");
    auto world = std::make_shared<GridWorld>(5, 5, logger);
    world->init();
    for (const auto& cell : world->get_cells()) {
        cell->set_elevation(0);
        cell->take_calcium(MaxCaco3);
    }
    auto pop = std::make_shared<Pop>(world, logger, config, Genome(0));
    if (!pop->try_spawn({2, 2})) throw std::runtime_error("fixture spawn failed");
    if (pop->try_move({2, 2}) || pop->try_move({-1, 2}))
        throw std::runtime_error("zero displacement or boundary accepted");
    auto target = world->get_cell({1, 2});
    target->set_elevation(7);
    if (pop->try_move({1, 2})) throw std::runtime_error("slope accepted");
    target->set_elevation(0);
    target->give_calcium(MaxCaco3);
    if (pop->try_move({1, 2})) throw std::runtime_error("calcium barrier accepted");
    target->take_calcium(MaxCaco3);
    auto blocker = std::make_shared<Pop>(world, logger, config, Genome(0));
    if (!blocker->try_spawn({1, 2}) || pop->try_move({1, 2}))
        throw std::runtime_error("occupied destination accepted");
    world->remove_entity(blocker);
    if (!pop->try_move({1, 2})) throw std::runtime_error("free destination rejected");
    const auto totals = pop->telemetry()["totals"];
    const auto value = [&](const char* key) { return totals.at(key).get<unsigned>(); };
    if (value("movement_attempts") != 6 || value("movement_successes") != 1 ||
        value("movement_no_displacement") != 1 || value("movement_blocked_boundary") != 1 ||
        value("movement_blocked_slope") != 1 || value("movement_blocked_calcium") != 1 ||
        value("movement_blocked_occupied") != 1 || value("movement_blocked_other") != 0)
        throw std::runtime_error("movement reasons do not partition attempts");
}

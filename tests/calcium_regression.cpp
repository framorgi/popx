// Deterministic ecological and observational fixtures, no production policy overrides.
#include "config.h"
#include "console_logger.h"
#include "grid_world.h"
#include "pop.h"
#include "pops_manager.h"
#include "stats.h"

#include <cmath>
#include <iostream>
#include <stdexcept>

namespace {
void require(bool condition, const char* message) {
    if (!condition)
        throw std::runtime_error(message);
}
void set_calcium(const std::shared_ptr<ICell>& cell, unsigned amount) {
    cell->take_calcium(MaxCaco3);
    cell->give_calcium(amount);
}
void weather(const std::shared_ptr<ILogger>& logger, int size, PositionT source, unsigned background,
             unsigned expected) {
    auto world = std::make_shared<GridWorld>(size, size, logger);
    for (const auto& cell : world->get_cells())
        set_calcium(cell, background);
    set_calcium(world->get_cell(source), MaxCaco3);
    const auto before = world->get_total_organics().caco3;
    require(!world->is_free(source), "saturated calcium must still obstruct");
    for (unsigned tick = 1; tick < CalciumWeatheringInterval; ++tick)
        world->update_cycle();
    require(!world->is_free(source), "barrier eroded before its interval");
    world->update_cycle();
    require(world->get_cell(source)->get_calcium() == expected, "incorrect weathering flux");
    require(world->is_free(source) == (expected < MaxCaco3), "accessibility disagrees with calcium");
    require(world->get_total_organics().caco3 == before, "weathering destroyed calcium");
    for (const auto& cell : world->get_cells())
        require(cell->get_calcium() <= MaxCaco3, "calcium exceeded capacity");
    const auto after = world->get_cell(source)->get_calcium();
    for (unsigned tick = 0; tick < CalciumWeatheringInterval; ++tick)
        world->update_cycle();
    require(world->get_cell(source)->get_calcium() == after, "unsaturated deposits should persist");
    if (background < MaxCaco3 - 1)
        for (const auto& cell : world->get_cells())
            if (cell != world->get_cell(source))
                require(cell->get_calcium() < MaxCaco3, "erosion created another barrier");
}
} // namespace

class SimulationAudit {
  public:
    static void correlation(const std::shared_ptr<ILogger>& logger) {
        auto config = std::make_shared<Config>("config/popx.ini");
        auto world = std::make_shared<GridWorld>(12, 4, logger);
        for (const auto& cell : world->get_cells())
            set_calcium(cell, 0);
        PopsManager manager(world, logger, config);
        Stats stats;
        stats.begin_session({{"kind", "calcium_fixture"}});
        for (int i = 0; i < 3; ++i) {
            auto pop = std::make_shared<Pop>(world, logger, config, Genome(0));
            PositionT p{1 + i * 4, 1};
            require(pop->try_spawn(p), "fixture spawn failed");
            manager.pops_.push_back(pop);
            for (int dy = -1; dy <= 1; ++dy)
                for (int dx = -1; dx <= 1; ++dx)
                    if (dx || dy)
                        set_calcium(world->get_cell(PositionT{p.x + dx, p.y + dy}), i * 100);
        }
        stats.sample(0, manager, *world);
        require(stats.latest()["calcium_distance_correlation"]["pearson"].is_null(), "initial r must be undefined");
        for (int i = 0; i < 3; ++i)
            manager.pops_[i]->lifetime_distance_ = 2 - i;
        stats.sample(200, manager, *world);
        auto correlation = stats.latest()["calcium_distance_correlation"];
        require(correlation["n"] == 3 && correlation["window_ticks"] == 200, "incorrect paired cohort");
        require(std::abs(correlation["pearson"].get<double>() + 1) < 1e-12, "expected inverse correlation");
        stats.sample(200, manager, *world);
        require(stats.latest()["calcium_distance_correlation"] == correlation, "final duplicate changed correlation");
        stats.sample(400, manager, *world);
        require(stats.latest()["calcium_distance_correlation"]["pearson"].is_null(), "zero variance is not zero r");
        stats.begin_session({{"kind", "reset"}});
        stats.sample(0, manager, *world);
        require(stats.latest()["calcium_distance_correlation"]["n"] == 0, "correlation history leaked across runs");
    }
};

int main() {
    try {
        auto logger = std::make_shared<ConsoleLogger>();
        logger->set_level(LogLevel::Error);
        weather(logger, 3, {1, 1}, 0, 492);
        weather(logger, 3, {0, 0}, 498, 497);
        weather(logger, 3, {1, 1}, 499, 500);
        weather(logger, 3, {1, 1}, 500, 500);
        weather(logger, 1, {0, 0}, 0, 500);
        SimulationAudit::correlation(logger);
        std::cout << "PASS: calcium obstacles, slow conservative erosion, caps, borders, correlation and reset\n";
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}

#include "console_logger.h"
#include "grid_world.h"
#include "source.h"

#include <cmath>
#include <iostream>
#include <stdexcept>

int main() {
    try {
        auto logger = std::make_shared<ConsoleLogger>();
        logger->set_level(LogLevel::Error);
        auto world = std::make_shared<GridWorld>(12, 12, logger, SourceSettings{2, 2, 2, 3});
        world->init();
        auto agents = world->get_environment_agents();
        if (agents.size() != 2) throw std::runtime_error("Expected two sources");
        for (const auto& agent : agents) {
            auto p = agent->get_position();
            if (world->get_cell(p)->get_occupant().lock() != agent || world->is_free(p))
                throw std::runtime_error("Source does not occupy its cell");
        }
        const auto symbol = agents.front()->get_render_state();
        if (symbol.shape != RenderShape::Square || !std::holds_alternative<SourceVisualData>(symbol.payload) ||
            symbol.color.r != 0 || symbol.color.g != 0 || symbol.color.b != 0 || symbol.size <= 1.0f)
            throw std::runtime_error("Source visual identity changed");
        const auto first = std::dynamic_pointer_cast<Source>(agents.front());
        if (first->try_move(agents.back()->get_position()))
            throw std::runtime_error("Source crossed another occupied cell");
        for (const auto& cell : world->get_cells()) cell->set_glucose(0);
        const auto initial = world->get_total_organics().c6h12o6;
        world->update_cycle();
        if (world->get_total_organics().c6h12o6 != initial)
            throw std::runtime_error("Passive glucose regeneration remains");
        for (int tick = 0; tick < 11; ++tick) {
            world->update_cycle();
            for (const auto& agent : agents)
                if (world->get_cell(agent->get_position())->get_occupant().lock() != agent)
                    throw std::runtime_error("Source movement lost occupancy");
        }
        if (world->get_regeneration_totals().first != world->get_total_organics().c6h12o6)
            throw std::runtime_error("Accepted Source glucose is not conserved");

        // Enclose a deterministic source with saturated glucose and impassable calcium.
        auto flood_world = std::make_shared<GridWorld>(9, 9, logger);
        flood_world->init();
        auto trapped = std::make_shared<Source>(*flood_world, SourceSettings{1, 1, 1, 3}, 12345);
        if (!trapped->try_spawn({4, 4})) throw std::runtime_error("Fixture spawn failed");
        for (const auto& cell : flood_world->get_cells()) { cell->set_glucose(0); cell->set_elevation(0); }
        for (int dy = -1; dy <= 1; ++dy)
            for (int dx = -1; dx <= 1; ++dx)
                if (dx || dy) {
                    auto cell = flood_world->get_cell({4 + dx, 4 + dy});
                    cell->set_glucose(MaxC6h12o6);
                    cell->give_calcium(MaxCaco3);
                }
        const auto before = flood_world->get_total_organics().c6h12o6;
        trapped->update();
        if (trapped->get_position().x != 4 || trapped->get_position().y != 4)
            throw std::runtime_error("Source passed a calcium barrier");
        if (trapped->last_deposit() != 24 || flood_world->get_total_organics().c6h12o6 != before + 24)
            throw std::runtime_error("Overflow did not flood into second-ring cells");
        if (flood_world->get_cell({4, 4})->get_glucose() != 0)
            throw std::runtime_error("Flood deposited under Source");
        const auto state = trapped->snapshot();
        const double angle = state.at("heading").get<double>() + state.at("curvature").get<double>();
        const PositionT planned{static_cast<int>(std::lround(std::cos(angle))),
                                static_cast<int>(std::lround(std::sin(angle)))};
        const PositionT escape = (planned.x == 1 && planned.y == 0) ? PositionT{0, 1} : PositionT{1, 0};
        flood_world->get_cell({4 + escape.x, 4 + escape.y})->take_calcium(MaxCaco3);
        trapped->update();
        if (trapped->get_position().x != 4 + escape.x || trapped->get_position().y != 4 + escape.y)
            throw std::runtime_error("Blocked Source did not try another direction");
        if (flood_world->get_cell({4, 4})->get_occupant().lock() ||
            flood_world->get_cell(trapped->get_position())->get_occupant().lock() != trapped)
            throw std::runtime_error("Source move did not transfer occupancy");
        std::cout << "PASS: source count, occupancy, collision, glucose flood and accounting\n";
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}

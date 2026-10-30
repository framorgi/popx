#include "config.h"
#include "console_logger.h"
#include "grid_world.h"
#include "pop.h"
#include "reaper.h"

#include <iostream>
#include <stdexcept>

int main() {
    try {
        auto log = std::make_shared<ConsoleLogger>();
        log->set_level(LogLevel::Error);
        auto config = std::make_shared<Config>("/dev/null");
        auto world = std::make_shared<GridWorld>(12, 12, log);
        world->init();
        for (const auto& cell : world->get_cells()) {
            cell->set_elevation(0);
            cell->take_calcium(MaxCaco3);
        }
        const ReaperSettings settings{1, 100, 2, 1, 4, 0};
        auto reaper = std::make_shared<Reaper>(*world, settings, 42);
        auto state = reaper->snapshot();
        state["x"] = 4; state["y"] = 4; state["heading"] = 0.0;
        state["kill_tick"] = 1;
        if (!reaper->restore(state)) throw std::runtime_error("Reaper fixture restore failed");
        const auto symbol = reaper->get_render_state();
        if (!std::holds_alternative<ReaperVisualData>(symbol.payload) || symbol.size <= 1 ||
            world->get_cell({4, 4})->get_occupant().lock() != reaper)
            throw std::runtime_error("Reaper visual/physical identity invalid");
        auto victim = std::make_shared<Pop>(world, log, config, Genome(0));
        if (!victim->try_spawn({5, 4}) || reaper->try_move({5, 4}))
            throw std::runtime_error("Reaper crossed an occupied cell");
        reaper->update();
        if (victim->is_alive() || reaper->last_kills() != 1 ||
            world->get_cell({5, 4})->get_occupant().lock() ||
            world->get_feromone_magnitude({5, 4}, DANGER_FEROMONE) <= 0)
            throw std::runtime_error("Attack did not kill and announce ahead");
        reaper->update();
        if (reaper->last_kills() != 0) throw std::runtime_error("Attack cooldown ignored");

        const ReaperSettings protected_settings{1, 100, 1, 1, 4, 250};
        auto protected_reaper = std::make_shared<Reaper>(*world, protected_settings, 43);
        auto protected_state = protected_reaper->snapshot();
        protected_state["x"] = 7; protected_state["y"] = 7; protected_state["heading"] = 0.0;
        if (!protected_reaper->restore(protected_state)) throw std::runtime_error("Protected fixture restore failed");
        auto newborn = std::make_shared<Pop>(world, log, config, Genome(0));
        if (!newborn->try_spawn({8, 7})) throw std::runtime_error("Newborn fixture spawn failed");
        protected_reaper->update();
        if (!newborn->is_alive() || protected_reaper->last_kills() != 0)
            throw std::runtime_error("Newborn protection ignored");

        auto seek_world = std::make_shared<GridWorld>(15, 15, log);
        seek_world->init();
        for (const auto& cell : seek_world->get_cells()) {
            cell->set_elevation(0);
            cell->take_calcium(MaxCaco3);
        }
        auto target = std::make_shared<Pop>(seek_world, log, config, Genome(0));
        if (!target->try_spawn({12, 7})) throw std::runtime_error("Seek target spawn failed");
        auto seeker = std::make_shared<Reaper>(*seek_world, ReaperSettings{1, 1, 1000, 1000, 4, 0}, 44);
        auto seek_state = seeker->snapshot();
        seek_state["x"] = 2; seek_state["y"] = 7; seek_state["heading"] = 3.141592653589793;
        if (!seeker->restore(seek_state)) throw std::runtime_error("Seeker fixture restore failed");
        int closest = 10;
        for (int tick = 0; tick < 80; ++tick) {
            seeker->update();
            const auto p = seeker->get_position();
            closest = std::min(closest, std::max(std::abs(p.x - 12), std::abs(p.y - 7)));
        }
        if (closest > 2) throw std::runtime_error("Distant Reaper never found the Pop sector");
        std::cout << "PASS: Reaper occupancy, attack, warning, cooldown, newborn protection and distant seek\n";
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}

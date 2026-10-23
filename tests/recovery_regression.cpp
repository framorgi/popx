// Standalone diagnostic: natural eligibility, invariant mutation rate and conservative death-water routing.
#include "config.h"
#include "console_logger.h"
#include "grid_world.h"
#include "pop.h"
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

class FixedMutationConfig : public Config {
  public:
    FixedMutationConfig() : Config("config/popx.ini") {}
    void set_point_mutation_rate(double) override {
        throw std::runtime_error("Reproduction must not change the configured mutation rate");
    }
};
} // namespace

class SimulationAudit {
  public:
    static void reproduction(const std::shared_ptr<ILogger>& logger) {
        auto config = std::make_shared<FixedMutationConfig>();
        auto world = std::make_shared<GridWorld>(5, 5, logger);
        for (auto& cell : world->get_cells())
            cell->take_calcium(MaxCaco3);
        PopsManager manager(world, logger, config);
        auto parent = std::make_shared<Pop>(world, logger, config, Genome(0), PhyT{5, 0, 1, 80});
        require(parent->try_spawn({2, 2}), "parent spawn failed");
        manager.pops_.push_back(parent);
        for (const auto state : {0, 1, 2}) {
            parent->age_ = state == 0 ? 1000 : 1001;
            parent->energy_ = state == 1 ? 3.0f : 100.0f;
            parent->glucose_ = state == 2 ? 20 : 100;
            const auto energy = parent->energy_;
            const auto glucose = parent->glucose_;
            manager.collect_reproducer_candidates();
            require(manager.repro_backlog_.empty(), "low population enqueued an ineligible parent");
            manager.process_reproduction_backlog();
            manager.try_reproduce(parent);
            require(manager.pops_.size() == 1 && parent->energy_ == energy && parent->glucose_ == glucose,
                    "ineligible reproduction drained a parent");
        }
        parent->age_ = 1001;
        parent->energy_ = 100;
        parent->glucose_ = 100;
        manager.collect_reproducer_candidates();
        require(manager.repro_backlog_.size() == 1, "eligible parent not queued");
        parent->energy_ = 50; // Eligibility must be checked again after waiting in the FIFO.
        manager.process_reproduction_backlog();
        require(manager.pops_.size() == 1 && parent->energy_ == 50, "stale eligibility forced a birth");
        parent->energy_ = 100;
        manager.collect_reproducer_candidates();
        manager.process_reproduction_backlog();
        require(manager.pops_.size() == 2, "natural birth below 20 was disabled");
        auto child = manager.pops_.back();
        require(!child->birth_was_forced_ && child->parent_id_ == parent->pop_id_, "incorrect birth provenance");
        require(parent->glucose_ + child->glucose_ == 100, "birth created or destroyed glucose");
        require(std::abs(parent->energy_ + child->energy_ - 99.7f) < 1e-4f, "birth energy ledger changed");
        require(config->get_point_mutation_rate() == 0.01, "configured mutation rate changed");
    }

    static void water(const std::shared_ptr<ILogger>& logger, int width, PositionT position,
                      unsigned reserve, const std::vector<unsigned>& initial, unsigned expected_sink,
                      bool equal_neighbours) {
        auto config = std::make_shared<Config>("config/popx.ini");
        auto world = std::make_shared<GridWorld>(width, width, logger);
        const auto cells = world->get_cells();
        unsigned before = reserve;
        for (std::size_t i = 0; i < cells.size(); ++i) {
            cells[i]->set_water(initial[i]);
            cells[i]->take_calcium(MaxCaco3);
            before += initial[i];
        }
        auto pop = std::make_shared<Pop>(world, logger, config, Genome(0));
        require(pop->try_spawn(position), "water fixture spawn failed");
        pop->water_ = reserve;
        pop->die();
        unsigned stored = 0;
        std::vector<unsigned> neighbours;
        for (int y = 0; y < width; ++y) {
            for (int x = 0; x < width; ++x) {
                auto cell = world->get_cell(PositionT{x, y});
                const unsigned water = cell->get_water();
                stored += water;
                require(water <= MaxH2o, "water exceeded cell capacity");
                const bool adjacent = std::abs(x - position.x) <= 1 && std::abs(y - position.y) <= 1;
                if (!adjacent)
                    require(water == initial[y * width + x], "water travelled outside adjacent cells");
                else if (x != position.x || y != position.y)
                    neighbours.push_back(water - initial[y * width + x]);
            }
        }
        require(stored + pop->water_ == before, "death-water balance failed");
        require(pop->water_ == expected_sink, "incorrect water overflow sink");
        require(pop->released_[1] == reserve - expected_sink, "released water telemetry mismatch");
        if (equal_neighbours && !neighbours.empty()) {
            const auto [low, high] = std::minmax_element(neighbours.begin(), neighbours.end());
            require(*high - *low <= 1, "equal neighbours received unequal shares");
        }
        pop->die();
        require(world->get_total_organics().h2o == stored && pop->released_[1] == reserve - expected_sink,
                "repeated death duplicated water");
    }

    static void run() {
        auto logger = std::make_shared<ConsoleLogger>();
        logger->set_level(LogLevel::Error);
        reproduction(logger);
        water(logger, 3, {1, 1}, 83, {0, 0, 0, 0, MaxH2o - 3, 0, 0, 0, 0}, 0, true);
        water(logger, 3, {1, 1}, 100,
              {MaxH2o - 1, MaxH2o - 9, MaxH2o - 50, MaxH2o, MaxH2o, MaxH2o, MaxH2o, MaxH2o, MaxH2o}, 40, false);
        water(logger, 3, {0, 0}, 7, {MaxH2o, 0, 0, 0, 0, 0, 0, 0, 0}, 0, true);
        water(logger, 1, {0, 0}, 7, {MaxH2o}, 7, false);
        water(logger, 1, {0, 0}, 7, {0}, 0, false);
        water(logger, 3, {1, 1}, 0, std::vector<unsigned>(9, MaxH2o), 0, true);
        std::cout << "PASS: natural-only reproduction, fixed mutation rate, fair local water return, caps and sinks\n";
    }
};

int main() {
    try {
        SimulationAudit::run();
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}

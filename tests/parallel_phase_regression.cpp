#include "config.h"
#include "console_logger.h"
#include "grid_world.h"
#include "pops_manager.h"

#include <tbb/parallel_for.h>
#include <tbb/task_scheduler_observer.h>

#include <algorithm>
#include <cmath>
#include <iostream>
#include <mutex>
#include <set>
#include <stdexcept>
#include <thread>

class PhaseConfig : public Config {
  public:
    explicit PhaseConfig(unsigned threads) : Config("config/popx.ini"), threads_(threads) {}
    unsigned get_num_threads() const override {
        return threads_;
    }
    bool get_newgen_enabled() const override {
        return false;
    }

  private:
    unsigned threads_;
};
class Observer : public tbb::task_scheduler_observer {
  public:
    explicit Observer(tbb::task_arena& arena) : tbb::task_scheduler_observer(arena) {
        observe(true);
    }
    ~Observer() {
        observe(false);
    }
    void on_scheduler_entry(bool) override {
        std::lock_guard<std::mutex> lock(mutex);
        threads.insert(std::this_thread::get_id());
    }
    std::mutex mutex;
    std::set<std::thread::id> threads;
};
class SimulationAudit {
  public:
    static std::vector<double> run(unsigned threads) {
        auto logger = std::make_shared<ConsoleLogger>();
        logger->set_level(LogLevel::Error);
        auto config = std::make_shared<PhaseConfig>(threads);
        auto world = std::make_shared<GridWorld>(12, 12, logger);
        for (const auto& cell : world->get_cells()) {
            cell->set_temperature(25);
            cell->set_elevation(0);
            cell->set_glucose(500);
            cell->set_water(1000);
            cell->take_calcium(MaxCaco3);
            cell->take_lipids(MaxLipids);
            cell->take_co2(MaxCo2);
            cell->take_o2(MaxO2);
            cell->give_o2(1000);
        }
        PopsManager manager(world, logger, config);
        if (manager.phase_arena_.max_concurrency() != static_cast<int>(threads))
            throw std::runtime_error("thread configuration ignored");
        // An explicit forward connection keeps this movement fixture independent of the idle fallback.
        Genome genome(0, Genome::encoding_from_config(*config));
        genome.add_gene(Gene(1, 0, 7, static_cast<unsigned>(Action::MOVE_FORWARD), 8192));
        for (int i = 0; i < 120; ++i) {
            auto p = std::make_shared<Pop>(world, logger, config, genome,
                                           PhyT{1, 0, 1, 80});
            p->try_spawn({i % 12, i / 12});
            p->last_direction_ = {1, 0};
            p->lazyness_ = 0.0f;
            manager.pops_.push_back(p);
            if (i % 3 == 0)
                manager.sense_bucket_.push_back(p);
            else if (i % 3 == 1)
                manager.think_bucket_.push_back(p);
            else
                manager.act_bucket_.push_back(p);
        }
        Observer observer(manager.phase_arena_);
        for (int tick = 0; tick < 90; ++tick)
            manager.update_cycle();
        observer.observe(false);
        std::cout << "configured=" << threads << " observed_arena_threads=" << observer.threads.size() << '\n';
        if (threads == 1 && observer.threads.size() != 1)
            throw std::runtime_error("serial limit violated");
        const auto original = manager.pops_;
        std::vector<PositionT> positions;
        std::vector<int> buckets;
        for (const auto& p : original) {
            positions.push_back(p->pos_);
            int bucket = 0;
            for (const auto* group : {&manager.sense_bucket_, &manager.think_bucket_, &manager.act_bucket_}) {
                if (std::find(group->begin(), group->end(), p) != group->end())
                    buckets.push_back(bucket);
                ++bucket;
            }
        }
        if (buckets.size() != original.size())
            throw std::runtime_error("bucket membership not unique");
        manager.trigger_new_generation();
        if (manager.get_alive_count() != 108)
            throw std::runtime_error("NewGen culling changed");
        for (std::size_t i = 0; i < original.size(); ++i) {
            const auto& p = original[i];
            auto occupant = world->get_cell(positions[i])->get_occupant().lock();
            if (!p->is_alive()) {
                if (occupant.get() == p.get())
                    throw std::runtime_error("culled occupant retained");
                continue;
            }
            auto placed = world->get_cell(p->pos_)->get_occupant().lock();
            if (placed.get() != p.get() || p->next_phase_ != State::SENSE)
                throw std::runtime_error("NewGen respawn lost occupancy or retained a stale phase");
            const auto& group = buckets[i] == 0   ? manager.sense_bucket_
                                : buckets[i] == 1 ? manager.think_bucket_
                                                  : manager.act_bucket_;
            if (std::find(group.begin(), group.end(), p) == group.end())
                throw std::runtime_error("NewGen reassigned a survivor phase bucket");
        }
        std::vector<double> result;
        for (const auto& p : manager.pops_) {
            result.insert(result.end(), {p->energy_, p->temperature_,
                                         double(p->glucose_), double(p->o2_), double(p->age_), double(p->last_action_),
                                         double(static_cast<int>(p->next_phase_))});
        }
        return result;
    }
};
int main() {
    try {
        const auto serial = SimulationAudit::run(1);
        const auto parallel = SimulationAudit::run(4);
        if (serial != parallel)
            throw std::runtime_error("serial/parallel state differs");
        std::cout << "PASS: configured arena concurrency and identical controlled 90-tick trajectories\n";
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}

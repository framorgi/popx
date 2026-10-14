#include "config.h"
#include "console_logger.h"
#include "grid_world.h"
#include "pops_manager.h"
#include "run_clock.h"
#include "simulator.h"
#include "stats.h"

#include <iostream>
#include <stdexcept>

void require(bool value, const char* message) {
    if (!value)
        throw std::runtime_error(message);
}
class MemoryTransport final : public ICommunicator {
  public:
    std::vector<nlohmann::json> sent;
    void send(nlohmann::json data) override {
        sent.push_back(std::move(data));
    }
    std::vector<nlohmann::json> receive() override {
        return {};
    }
    nlohmann::json status() const override {
        return {{"connected", true}};
    }
    bool flush(unsigned) override {
        return true;
    }
};
/// @brief A kill between ACT and physiology must not charge unsettled movement energy twice.
class SimulationAudit {
  public:
    static void extinction() {
        auto log = std::make_shared<ConsoleLogger>();
        log->set_level(LogLevel::Error);
        auto config = std::make_shared<Config>("config/popx.ini");
        auto world = std::make_shared<GridWorld>(24, 24, log);
        auto manager = std::make_shared<PopsManager>(world, log, config);
        Simulator sim(world, manager, log);
        sim.set_duration(1);
        sim.init();
        sim.command("pause", {});
        sim.update();
        require(sim.tick() == 0, "paused simulation advanced");
        sim.command("resume", {});
        for (auto& pop : manager->pops_)
            pop->die();
        sim.update();
        require(sim.state() == "finished" && sim.reason() == "extinction" && sim.tick() == 1, "extinction boundary");
        sim.update();
        require(sim.tick() == 1, "finished simulation advanced");
    }
    static void endless_recovery() {
        auto log = std::make_shared<ConsoleLogger>();
        log->set_level(LogLevel::Error);
        auto config = std::make_shared<Config>("config/popx.ini");
        auto world = std::make_shared<GridWorld>(24, 24, log);
        auto manager = std::make_shared<PopsManager>(world, log, config);
        auto stats = std::make_shared<Stats>();
        stats->begin_session({{"kind", "endless_recovery_fixture"}});
        manager->set_stats(stats);
        Simulator sim(world, manager, log);
        sim.set_duration(0);
        sim.init();
        for (auto& pop : manager->pops_)
            pop->die();
        sim.update();
        require(sim.state() == "running" && sim.reason().empty(), "endless extinction ended the run");
        require(manager->get_alive_count() == MinPopulationAllowed, "endless extinction did not reseed forced births");
        require(sim.tick() == 1, "endless recovery changed the tick counter");
        stats->sample(sim.tick(), *manager, *world);
        require(stats->latest()["counts"]["forced_births"] == MinPopulationAllowed,
                "endless recovery was not recorded as forced births");
    }
    static void pending_action() {
        auto log = std::make_shared<ConsoleLogger>();
        log->set_level(LogLevel::Error);
        auto config = std::make_shared<Config>("config/popx.ini");
        auto world = std::make_shared<GridWorld>(24, 24, log);
        auto manager = std::make_shared<PopsManager>(world, log, config);
        auto stats = std::make_shared<Stats>();
        stats->begin_session({{"kind", "pending_action_fixture"}});
        manager->set_stats(stats);
        world->init();
        manager->spawn_population();
        stats->sample(0, *manager, *world);
        auto pop = manager->pops_.front();
        pop->next_phase_ = State::ACT;
        pop->last_action_ = static_cast<int>(Action::MOVE_EAST);
        pop->act();
        pop->die();
        stats->set_tick(1);
        manager->update_cycle();
        stats->sample(1, *manager, *world);
        require(stats->latest()["totals"]["pending_action_cost"].get<double>() > 0, "pending cost not observed");
        require(std::abs(stats->latest()["energy_ledger"]["residual"].get<double>()) < 1e-5,
                "death before physiology ledger");
        for (const auto& residual : stats->latest()["resource_ledger"]["residual"])
            require(residual == 0, "death resource returns");
    }
};

int main() {
    try {
        SimulationAudit::pending_action();
        SimulationAudit::extinction();
        SimulationAudit::endless_recovery();
        for (uint64_t tick = 0; tick < 1000000; ++tick)
            require(!RunClock::expired(tick, 0), "infinite duration stopped");
        require(!RunClock::expired(5999, 1) && RunClock::expired(6000, 1), "duration boundary");
        require(RunClock::interval(1).count() == 2 * RunClock::interval(2).count(), "speed changed biological time");
        auto log = std::make_shared<ConsoleLogger>();
        log->set_level(LogLevel::Error);
        auto config = std::make_shared<Config>("config/popx.ini");
        auto world = std::make_shared<GridWorld>(24, 24, log);
        auto manager = std::make_shared<PopsManager>(world, log, config);
        auto transport = std::make_unique<MemoryTransport>();
        auto* memory = transport.get();
        auto stats = std::make_shared<Stats>(std::move(transport));
        stats->begin_session({{"kind", "regression"}});
        manager->set_stats(stats);
        world->init();
        manager->spawn_population();
        stats->sample(0, *manager, *world);
        stats->record_tick_cost(100);
        stats->record_tick_cost(300);
        stats->record_frame(1000, 10000);
        stats->record_frame(3000, 30000);
        stats->sample(704201, *manager, *world);
        const auto& sample = stats->latest();
        require(sample["avg_tick_us"] == 200 && sample["avg_frame_us"] == 2000 && sample["fps"] == 50,
                "performance denominators");
        require(sample["counts"]["founders"] == 80, "founder count");
        manager->trigger_new_generation();
        stats->sample(704202, *manager, *world);
        require(stats->latest()["counts"]["culled"] == 8, "cull not counted exactly once");
        for (const auto& residual : stats->latest()["resource_ledger"]["residual"])
            require(residual == 0, "cull matter balance");
        require(std::abs(stats->latest()["energy_ledger"]["residual"].get<double>()) < 1e-6, "cull energy balance");
        const auto before_rotation = stats->latest()["sample_means"]["energy"].get<double>();
        for (uint64_t tick = 704203; tick < 704225; ++tick)
            stats->sample(tick, *manager, *world);
        require(stats->latest()["counts"]["deaths"] == 8, "history rotation lost events");
        require(std::abs(stats->latest()["sample_means"]["energy"].get<double>() - before_rotation) < 1e-6,
                "history rotation changed whole-run means");
        const auto before = memory->sent.size();
        stats->end_session(704225, "stop", *manager, *world);
        require(memory->sent.size() == before + 74, "final sample/censor/end events");
        stats->begin_session({{"kind", "second"}});
        stats->sample(0, *manager, *world);
        require(stats->latest()["avg_frame_us"] == 0 && stats->latest()["counts"]["deaths"] == 0,
                "restart leaked history");
        std::cout << "PASS: clocks beyond 704k, exact duration, stats denominators, culling ledgers, final censoring, "
                     "reset\n";
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}

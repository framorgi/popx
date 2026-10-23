// A paired, observational fork of a production checkpoint. Only NewGen changes.
#include "checkpoint.h"
#include "console_logger.h"
#include "diagnostic_output.h"
#include "simulator.h"

#include <chrono>
#include <cmath>
#include <iostream>
#include <memory>
#include <stdexcept>

using Json = nlohmann::json;

namespace {
class ComparisonConfig final : public Config {
  public:
    ComparisonConfig(const std::string& path, bool newgen) : Config(path, true), newgen_(newgen) {}
    bool get_newgen_enabled() const override { return newgen_; }

  private:
    bool newgen_;
};

constexpr const char* movement_keys[] = {"movement_attempts", "movement_successes", "movement_blocked_occupied",
                                         "movement_blocked_calcium", "movement_blocked_slope",
                                         "movement_blocked_boundary", "movement_no_displacement",
                                         "movement_blocked_other"};

Json case_run(const Json& checkpoint, const std::string& config_path, bool newgen, unsigned limit) {
    RandomUtility::set_diagnostic_seed(20261020);
    std::srand(20261020);
    auto logger = std::make_shared<ConsoleLogger>();
    logger->set_level(LogLevel::Error);
    auto config = std::make_shared<ComparisonConfig>(config_path, newgen);
    auto world = std::make_shared<GridWorld>(config->get_map_size(), config->get_map_size(), logger,
        SourceSettings{config->get_source_count(), config->get_source_move_interval_ticks(),
                       config->get_source_glucose_interval_ticks(), config->get_source_glucose_per_neighbor()});
    auto manager = std::make_shared<PopsManager>(world, logger, config);
    CheckpointCodec::restore(checkpoint, *world, *manager);
    auto stats = std::make_shared<Stats>();
    stats->begin_session({{"kind", "movement_checkpoint_fork"}, {"newgen", newgen}});
    manager->set_stats(stats);
    stats->sample(0, *manager, *world);
    const auto baseline = stats->latest();
    Simulator simulator(world, manager, logger);
    Json samples = Json::array();
    const auto began = std::chrono::steady_clock::now();
    for (unsigned tick = 1; tick <= limit; ++tick) {
        simulator.update();
        if (tick % 10000 != 0 && tick != limit) continue;
        stats->sample(tick, *manager, *world);
        const auto& d = stats->latest();
        samples.push_back({{"tick", tick}, {"alive", d["alive"]}, {"natural_births", d["counts"]["natural_births"]},
                           {"forced_births", d["counts"]["forced_births"]}, {"deaths", d["counts"]["deaths"]},
                           {"culled", d["counts"]["culled"]},
                           {"movement_attempts", d["totals"]["movement_attempts"].get<double>() -
                                                 baseline["totals"]["movement_attempts"].get<double>()},
                           {"movement_successes", d["totals"]["movement_successes"].get<double>() -
                                                  baseline["totals"]["movement_successes"].get<double>()}});
    }
    const auto& last = stats->latest();
    Json movement = Json::object();
    for (const auto* key : movement_keys)
        movement[key] = last["totals"][key].get<double>() - baseline["totals"][key].get<double>();
    const double attempts = movement["movement_attempts"], successes = movement["movement_successes"];
    double accounted = successes;
    for (const auto* key : movement_keys)
        if (std::string(key) != "movement_attempts" && std::string(key) != "movement_successes")
            accounted += movement[key].get<double>();
    if (std::abs(attempts - accounted) > 0.01)
        throw std::runtime_error("Movement reasons do not partition attempts");
    const auto alive = manager->telemetry(false);
    unsigned never_moved = 0;
    for (const auto& pop : alive)
        never_moved += pop["totals"]["movement_successes"].get<uint64_t>() == 0;
    const auto elapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - began).count();
    RandomUtility::clear_diagnostic_seed();
    return {{"newgen", newgen}, {"ticks", limit}, {"elapsed_seconds", elapsed}, {"alive", last["alive"]},
            {"never_moved_alive", never_moved}, {"generation", manager->get_generation_count()},
            {"counts", last["counts"]}, {"reproduction", last["reproduction"]},
            {"movement", movement}, {"samples", samples},
            {"resources", last["world_resources"]}, {"learning", last["learning"]}};
}
} // namespace

int main(int argc, char** argv) {
    if (argc != 5 || std::string(argv[2]) != "receiver") {
        std::cerr << "usage: popx_movement_comparison CONFIG receiver CHECKPOINT TICKS\n";
        return 2;
    }
    try {
        const auto checkpoint = CheckpointCodec::read(argv[3]);
        const Config saved_config(argv[1], true);
        if (saved_config.values() != checkpoint.at("config"))
            throw std::invalid_argument("INI differs from the checkpoint configuration");
        const auto limit = std::stoul(argv[4]);
        if (limit < 50000 || limit > 500000)
            throw std::invalid_argument("Comparison needs 50000..500000 ticks");
        const auto interval = saved_config.get_newgen_interval_ticks();
        const auto cycle = checkpoint.at("manager").at("cycle").get<uint64_t>();
        const auto first_newgen_tick = interval - cycle % interval;
        if (first_newgen_tick <= 10000)
            throw std::invalid_argument("Checkpoint is too close to NewGen for the preflight window");
        const auto preflight_on = case_run(checkpoint, argv[1], true, 10000);
        const auto preflight_off = case_run(checkpoint, argv[1], false, 10000);
        if (preflight_on["samples"] != preflight_off["samples"] ||
            preflight_on["movement"] != preflight_off["movement"] ||
            preflight_on["resources"] != preflight_off["resources"])
            throw std::runtime_error("Forks diverged before the first scheduled NewGen event");
        Json report = {{"source_run", checkpoint.at("source_run")},
                       {"source_tick", checkpoint.at("source_tick")},
                       {"checkpoint_model", checkpoint.at("model")},
                       {"ticks_per_case", limit},
                       {"diagnostic_seed", 20261020},
                       {"pre_newgen_10000_ticks_identical", true},
                       {"comparison_limit", "Fixture constructor RNG and rand reset; Source RNG comes from "
                                            "checkpoint. Exact long-run reproducibility is not claimed."},
                       {"cases", Json::array()}};
        report["cases"].push_back(case_run(checkpoint, argv[1], true, limit));
        report["cases"].push_back(case_run(checkpoint, argv[1], false, limit));
        for (std::size_t i = 0; i < report["cases"][0]["samples"].size(); ++i) {
            const auto& on = report["cases"][0]["samples"][i];
            const auto& off = report["cases"][1]["samples"][i];
            if (on["tick"].get<uint64_t>() < first_newgen_tick && on != off)
                throw std::runtime_error("Forks diverged before the first scheduled NewGen event");
        }
        report["first_newgen_tick"] = first_newgen_tick;
        persist_diagnostic(report, "movement_comparison");
        for (const auto& c : report["cases"])
            std::cout << "newgen=" << c["newgen"] << " alive=" << c["alive"]
                      << " attempts=" << c["movement"]["movement_attempts"]
                      << " successes=" << c["movement"]["movement_successes"] << '\n';
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Comparison failed: " << error.what() << '\n';
        return 1;
    }
}

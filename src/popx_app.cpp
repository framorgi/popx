#include "popx_app.h"

#include "checkpoint.h"
#include "console_logger.h"
#include "design_system.h"
#include "run_clock.h"
#include "udp_communicator.h"

#include <X11/Xlib.h>

#include <chrono>
#include <csignal>
#include <cstdlib>
#include <fstream>
#include <thread>

namespace {
volatile std::sig_atomic_t interrupted = 0;
void on_signal(int) {
    interrupted = 1;
}
using Clock = std::chrono::steady_clock;
} // namespace
PopXApp::PopXApp(AppOptions options) : options_(std::move(options)) {
    DesignSystem::load(options_.config_path);
    logger_ = std::make_shared<ConsoleLogger>();
    logger_->set_level(LogLevel::Error);
    stats_ = std::make_shared<Stats>(std::make_unique<UdpCommunicator>(options_.udp_port));
}
void PopXApp::start(const std::string& checkpoint_path) {
    nlohmann::json checkpoint;
    std::filesystem::path candidate;
    if (!checkpoint_path.empty()) {
        checkpoint = CheckpointCodec::read(checkpoint_path);
        candidate = options_.config_path + ".checkpoint-candidate";
        std::ofstream ini(candidate, std::ios::trunc);
        ini << checkpoint.at("ini").get<std::string>();
        // Older snapshots predate the Reaper. Persist the migration defaults in the
        // locked INI so future checkpoints also declare the predator's parameters.
        if (!checkpoint.at("config").contains("reaper_count"))
            ini << "\n# Reaper defaults added when restoring a pre-Reaper checkpoint\n"
                   "reaper_count = 1\n"
                   "reaper_move_interval_ticks = 100\n"
                   "reaper_kill_interval_ticks = 240\n"
                   "reaper_announce_interval_ticks = 20\n"
                   "reaper_warning_radius = 6\n"
                   "reaper_newborn_protection_ticks = 250\n";
        ini.close();
        if (!ini)
            throw std::runtime_error("Cannot stage checkpoint configuration");
    }
    struct RemoveCandidate {
        std::filesystem::path p;
        ~RemoveCandidate() {
            if (!p.empty()) {
                std::error_code ec;
                std::filesystem::remove(p, ec);
            }
        }
    } cleanup{candidate};
    auto config = std::make_shared<Config>(candidate.empty() ? options_.config_path : candidate.string(), true);
    if (!checkpoint.is_null()) {
        auto saved_config = checkpoint.at("config");
        // v4 checkpoints written before this parameter used the same 240-Pop default.
        if (!saved_config.contains("max_population_allowed"))
            saved_config["max_population_allowed"] = 240;
        if (!saved_config.contains("offspring_dispersal_radius"))
            saved_config["offspring_dispersal_radius"] = 1;
        if (!saved_config.contains("climate_rotation_speed"))
            saved_config["climate_rotation_speed"] = 1.0;
        const auto current_values = config->values();
        for (const auto& [key, value] : current_values.items())
            if ((key.starts_with("reward_") || key.starts_with("reaper_")) && !saved_config.contains(key))
                saved_config[key] = value;
        if (config->values() != saved_config)
            throw std::runtime_error("Checkpoint INI differs from saved configuration");
    }
    auto world = std::make_shared<GridWorld>(
        config->get_map_size(), config->get_map_size(), logger_,
        SourceSettings{config->get_source_count(), config->get_source_move_interval_ticks(),
                       config->get_source_glucose_interval_ticks(), config->get_source_glucose_per_neighbor()},
        config->get_climate_rotation_speed(),
        ReaperSettings{config->get_reaper_count(), config->get_reaper_move_interval_ticks(),
                       config->get_reaper_kill_interval_ticks(), config->get_reaper_announce_interval_ticks(),
                       config->get_reaper_warning_radius(), config->get_reaper_newborn_protection_ticks()});
    auto agents = std::make_shared<PopsManager>(world, logger_, config);
    auto sim = std::make_shared<Simulator>(world, agents, logger_);
    sim->set_duration(config->get_max_duration_minutes());
    sim->command("speed", {{"value", options_.speed}});
    if (checkpoint.is_null())
        sim->init();
    else {
        CheckpointCodec::restore(checkpoint, *world, *agents);
        sim->command("speed", {{"value", checkpoint.at("speed")}});
        sim->command("pause", {}); // The dashboard resumes only after storage/configuration is ready.
        int fd = ::open(candidate.c_str(), O_RDONLY);
        if (fd < 0)
            throw std::runtime_error("Cannot sync checkpoint INI");
        int ok = ::fsync(fd);
        ::close(fd);
        if (ok)
            throw std::runtime_error("Cannot sync checkpoint INI");
        std::filesystem::rename(candidate, options_.config_path);
        config->filepath_ = options_.config_path;
    }
    // All validation and reconstruction succeeded before replacing the old run.
    if (sim_ && !finished_recorded_)
        stats_->end_session(sim_->tick(), "restart", *agents_, *world_);
    const bool visible = bool(gui_);
    hide_gui();
    config_ = std::move(config);
    world_ = std::move(world);
    agents_ = std::move(agents);
    sim_ = std::move(sim);
    std::ifstream ini(options_.config_path);
    std::string raw((std::istreambuf_iterator<char>(ini)), {});
    active_ini_ = raw;
    locked_ini_ = checkpoint.is_null() ? std::string{} : raw;
    loaded_checkpoint_ = checkpoint_path;
    sandbox_temperature_undo_.clear();
    migration_origin_ = nullptr;
    if (!checkpoint.is_null()) {
        migration_origin_ = checkpoint.value("migration_origin", nlohmann::json(nullptr));
        if (checkpoint.at("partial").get<bool>())
            migration_origin_ = {
                {"source_run", checkpoint.at("source_run")},
                {"source_tick", checkpoint.at("source_tick")},
                {"reconstruction", checkpoint.value("reconstruction", std::string("Legacy partial migration"))}};
    }
    stats_->begin_session(
        {{"kind", "evolution"},
         {"checkpoint", checkpoint.is_null() ? nlohmann::json(nullptr)
                                             : nlohmann::json{{"path", checkpoint_path},
                                                              {"source_run", checkpoint.at("source_run")},
                                                              {"source_tick", checkpoint.at("source_tick")},
                                                              {"partial_migration", !migration_origin_.is_null()},
                                                              {"state_complete", !checkpoint.at("partial").get<bool>()},
                                                              {"migration_origin", migration_origin_}}},
         {"config_locked", !locked_ini_.empty()},
         {"sensor_model", "chemical_density_v2"},
         {"config", config_->values()},
         {"ini", raw},
         {"config_path", config_->path()},
         {"rng", "random_device and rand; no global deterministic seed"},
         {"build", __VERSION__},
         {"tick_seconds", 0.01},
         {"temperature_refresh_interval_ticks", TemperatureRefreshInterval},
         {"temperature_rbf_model", "orbiting_centers_fixed_widths_v1"},
         {"population_capacity", config_->get_max_population_allowed()},
         {"genetic_similarity_model", "endpoint_weight_v1"},
         {"genetic_color_model", "projection_v1"},
         {"extinction_recovery_model", "recent_distinct_heritable_profiles_64_v1"},
         {"model", "moving_glucose_source_v10"}});
    agents_->set_stats(stats_);
    for (auto& p : agents_->telemetry(true)) {
        if (!checkpoint.is_null()) {
            p["checkpoint_entry"] = true;
            p["entry_age"] = p["age"];
        }
        stats_->record_event({LifeEvent::Founder, std::move(p)});
    }
    stats_->record_environment(*world_);
    stats_->sample(0, *agents_, *world_);
    control_.selected_id.clear();
    control_.inspection = nullptr;
    step_remaining_ = 0;
    finished_recorded_ = false;
    reset_clock_ = true;
    if (visible) {
        try {
            show_gui();
        } catch (const std::exception& error) {
            control_.error = error.what();
            logger_->error(control_.error);
        }
    }
}
void PopXApp::show_gui() {
    if (gui_)
        return;
    // SFML aborts on an unavailable X server. Validate before constructing any SFML graphics object.
    Display* display = XOpenDisplay(nullptr);
    if (!display)
        throw std::runtime_error("Display unavailable; simulation remains headless");
    XCloseDisplay(display);
    auto gfx = std::make_shared<SfmlGraphicEngine>(logger_);
    gfx->bind_controls(control_);
    auto renderer = std::make_unique<Renderer>(gfx, world_, config_);
    renderer->init();
    renderer->imgui_init();
    auto gui = std::make_unique<ImGuiLayer>(*gfx, *stats_, *agents_, control_, flags_);
    gui->init();
    gfx_ = std::move(gfx);
    renderer_ = std::move(renderer);
    gui_ = std::move(gui);
    if (sim_->speed() == 0)
        sim_->command("speed", {{"value", 2}});
    reset_clock_ = true;
}
void PopXApp::hide_gui() {
    gui_.reset();
    renderer_.reset();
    gfx_.reset();
    reset_clock_ = true;
}
nlohmann::json PopXApp::status() const {
    return {{"state", sim_->state()},
            {"reason", sim_->reason()},
            {"tick", sim_->tick()},
            {"simulated_seconds", sim_->tick() * .01},
            {"speed", sim_->speed()},
            {"gui", bool(gui_)},
            {"alive", agents_->get_alive_count()},
            {"step_remaining", step_remaining_},
            {"effective_tps", effective_tps_},
            {"render_fps", render_fps_},
            {"runtime", config_->values()},
            {"appearance", DesignSystem::settings},
            {"observer", {{"selected_id", control_.selected_id}, {"zoom", gfx_ ? gfx_->zoom() : 1.f}}},
            {"checkpoint_supported", true},
            {"config_locked", !locked_ini_.empty()},
            {"run_id", stats_->run_id()},
            {"transport", stats_->transport_status()}};
}
void PopXApp::dispatch(nlohmann::json command) {
    const auto id = command.value("request_id", std::string{});
    if (!id.empty() && replies_.contains(id)) {
        stats_->publish("reply", replies_.at(id));
        return;
    }
    nlohmann::json reply = {{"request_id", id}, {"ok", true}};
    try {
        if (command.contains("expires_ms") &&
            command.at("expires_ms").get<int64_t>() < std::chrono::duration_cast<std::chrono::milliseconds>(
                                                          std::chrono::system_clock::now().time_since_epoch())
                                                          .count())
            throw std::runtime_error("Command expired");
        if (command.contains("run_id") && command["run_id"] != stats_->run_id())
            throw std::runtime_error("Run has changed; refresh status");
        const auto name = command.at("command").get<std::string>();
        const auto args = command.value("arguments", nlohmann::json::object());
        if (name == "appearance") {
            DesignSystem::apply(args);
            if (renderer_)
                renderer_->invalidate();
        } else if (name == "select_cell") {
            control_.cell_x = args.at("x");
            control_.cell_y = args.at("y");
            control_.selected_id = agents_->pop_at({control_.cell_x, control_.cell_y});
            control_.inspection = nullptr;
            if (!control_.selected_id.empty())
                control_.panel_request = 1;
        } else if (name == "sandbox_spawn" || name == "sandbox_remove" || name == "sandbox_cell" ||
                   name == "sandbox_weight") {
            if (sim_->state() != "paused" || step_remaining_)
                throw std::runtime_error("Pause before sandbox intervention");
            if (name == "sandbox_spawn" || name == "sandbox_remove" || name == "sandbox_cell") {
                if (!args.at("x").is_number_integer() || !args.at("y").is_number_integer())
                    throw std::invalid_argument("Integer coordinates required");
                int x = args.at("x"), y = args.at("y");
                if (x < 0 || y < 0 || x >= world_->get_width() || y >= world_->get_height())
                    throw std::invalid_argument("Cell outside world");
                if (name == "sandbox_spawn") {
                    const auto kind = args.value("kind", std::string("pop"));
                    if (kind == "pop") {
                        if (args.value("brush", false) &&
                            (!world_->is_free({x, y}) || agents_->get_alive_count() >= int(config_->get_max_population_allowed())))
                            reply["result"] = {{"changed", false}};
                        else reply["result"] = agents_->sandbox_spawn({x, y});
                    }
                    else if (kind == "source") {
                        const bool placed = world_->sandbox_spawn_source({x, y});
                        if (!placed && !args.value("brush", false)) throw std::runtime_error("Source cell unavailable or limit reached");
                        reply["result"] = {{"kind", "source"}, {"x", x}, {"y", y}, {"changed", placed}};
                    } else throw std::invalid_argument("Unknown spawn kind");
                } else if (name == "sandbox_remove") {
                    const auto kind = args.at("kind").get<std::string>();
                    bool removed = kind == "pop" ? agents_->sandbox_remove({x, y})
                                 : kind == "source" ? world_->sandbox_remove_source({x, y}) : false;
                    if (!removed && !args.value("brush", false)) throw std::runtime_error("Selected agent absent from cell");
                    reply["result"] = {{"removed", kind}, {"x", x}, {"y", y}, {"changed", removed}};
                } else {
                    auto cell = world_->get_cell(y * world_->get_width() + x);
                    const auto field = args.at("field").get<std::string>();
                    const bool erase = args.value("erase", false);
                    if (field == "temperature" && erase) {
                        auto& undo = sandbox_temperature_undo_[y * world_->get_width() + x];
                        if (undo.empty()) {
                            if (!args.value("brush", false)) throw std::runtime_error("No temperature edit to undo on this cell");
                            reply["result"] = {{"changed", false}};
                        } else {
                            cell->set_temperature(undo.back());
                            undo.pop_back();
                        }
                    } else {
                        const double value = erase ? 0.0 : args.at("value").get<double>();
                        if (!std::isfinite(value)) throw std::invalid_argument("Finite value required");
                        if (field == "temperature") {
                            if (value < -50 || value > 100) throw std::invalid_argument("Temperature outside -50..100 C");
                            sandbox_temperature_undo_[y * world_->get_width() + x].push_back(cell->get_temperature());
                            cell->set_temperature(value);
                        } else if (field == "food" || field == "danger" || field == "mate" || field == "home") {
                            if (value < 0 || value > 100) throw std::invalid_argument("Pheromone amount outside 0..100");
                            const FeromoneT type = field == "food" ? FOOD_FEROMONE : field == "danger" ? DANGER_FEROMONE
                                                    : field == "mate" ? MATE_FEROMONE : HOME_FEROMONE;
                            const float delta = erase ? -cell->get_feromone_map().at(type) : float(value);
                            cell->set_feromone(type, delta);
                        } else {
                            const auto& o = cell->get_organics();
                            unsigned present = 0, cap = 0;
                            std::function<unsigned(unsigned)> take, give;
                            if (field == "glucose") { present=o.c6h12o6; cap=MaxC6h12o6; take=[&](unsigned n){return cell->take_glucose(n);}; give=[&](unsigned n){return cell->give_glucose(n);}; }
                            else if (field == "water") { present=o.h2o; cap=MaxH2o; take=[&](unsigned n){return cell->take_water(n);}; give=[&](unsigned n){return cell->give_water(n);}; }
                            else if (field == "oxygen") { present=o.o2; cap=MaxO2; take=[&](unsigned n){return cell->take_o2(n);}; give=[&](unsigned n){return cell->give_o2(n);}; }
                            else if (field == "co2") { present=o.co2; cap=MaxCo2; take=[&](unsigned n){return cell->take_co2(n);}; give=[&](unsigned n){return cell->give_co2(n);}; }
                            else if (field == "lipids") { present=o.lipids; cap=MaxLipids; take=[&](unsigned n){return cell->take_lipids(n);}; give=[&](unsigned n){return cell->give_lipids(n);}; }
                            else if (field == "calcium") { present=o.caco3; cap=MaxCaco3; take=[&](unsigned n){return cell->take_calcium(n);}; give=[&](unsigned n){return cell->give_calcium(n);}; }
                            else if (field == "nitrogen") { present=o.n2; cap=MaxN2; }
                            else throw std::invalid_argument("Unknown cell field");
                            if (!erase && (value < 0 || value > cap || std::floor(value) != value))
                                throw std::invalid_argument("Resource amount outside capacity or fractional");
                            const unsigned amount = erase ? present : unsigned(value);
                            unsigned changed = field == "nitrogen" ? 0 : erase ? take(amount) : give(amount);
                            if (field == "nitrogen") {
                                // N2 has no cell transfer API; the concrete Cell owns this bounded resource.
                                auto concrete = std::dynamic_pointer_cast<Cell>(cell);
                                changed = erase ? concrete->take_n2(amount) : concrete->give_n2(amount);
                            }
                            if (field != "nitrogen") {
                                const unsigned ledger_index = field == "glucose" ? 0 : field == "water" ? 1
                                    : field == "calcium" ? 2 : field == "co2" ? 3 : field == "oxygen" ? 4 : 5;
                                stats_->record_sandbox_resource_delta(ledger_index,
                                                                      erase ? -int64_t(changed) : int64_t(changed));
                            }
                            reply["result"] = {{"changed", changed}, {"field", field}};
                        }
                    }
                    if (!reply.contains("result") || reply["result"].value("changed", 1) != 0)
                        agents_->reset_learning_credit();
                }
            } else
                agents_->sandbox_weight(args);
            if (!reply.contains("result") || reply["result"].value("changed", 1) != 0) {
                stats_->publish("diagnostic", {{"kind", name}, {"tick", sim_->tick()}, {"arguments", args}});
                if (renderer_) renderer_->invalidate();
            }
        } else if (name == "runtime_schema") {
            reply["result"] = Config::runtime_schema();
        } else if (name == "runtime_apply") {
            if (sim_->state() == "finished")
                throw std::runtime_error("Run finished");
            const auto before = config_->values();
            config_->apply_runtime(args);
            world_->set_climate_rotation_speed(config_->get_climate_rotation_speed());
            active_ini_ += "\n# Runtime sandbox overrides\n";
            for (const auto& [key, value] : args.items())
                active_ini_ += key + " = " + value.dump() + "\n";
            // Rebase delayed credit when the objective or its learning parameters change.
            agents_->reset_learning_credit();
            stats_->publish("diagnostic",
                            {{"kind", "sandbox_runtime"}, {"tick", sim_->tick()}, {"before", before}, {"patch", args}});
            reply["result"] = config_->values();
        } else if (name == "step") {
            if (sim_->state() != "paused" || step_remaining_)
                throw std::runtime_error("Pause before stepping");
            if (!args.at("ticks").is_number_integer())
                throw std::invalid_argument("Integer ticks required");
            auto count = args.at("ticks").get<int64_t>();
            if (count < 1 || count > 10000)
                throw std::invalid_argument("Tick count must be 1..10000");
            step_remaining_ = count;
        } else if (name == "checkpoint_save") {
            const auto path = args.at("path").get<std::string>();
            // Disk edits after Start are proposals for another run. A checkpoint must
            // carry the exact INI that constructed this live world and its Pop brains.
            if (active_ini_.empty())
                throw std::runtime_error("Active run INI is unavailable");
            auto doc = CheckpointCodec::capture(*world_, *agents_, active_ini_, config_->values(), stats_->run_id(),
                                                sim_->tick(), sim_->speed());
            doc["migration_origin"] = migration_origin_;
            CheckpointCodec::write(path, doc);
            if (args.value("pause", false)) {
                step_remaining_ = 0;
                if (sim_->state() == "running")
                    sim_->command("pause", {});
            }
            reply["result"] = {
                {"path", path}, {"tick", sim_->tick()}, {"run_id", stats_->run_id()}, {"complete", true}};
        } else if (name == "checkpoint_verify") {
            auto doc = CheckpointCodec::read(args.at("path").get<std::string>());
            if (doc.at("config") != config_->values())
                throw std::runtime_error("Verification requires matching active configuration");
            auto world = std::make_shared<GridWorld>(config_->get_map_size(), config_->get_map_size(), logger_,
                                                     SourceSettings{config_->get_source_count(),
                                                                    config_->get_source_move_interval_ticks(),
                                                                    config_->get_source_glucose_interval_ticks(),
                                                                    config_->get_source_glucose_per_neighbor()},
                                                     config_->get_climate_rotation_speed(),
                                                     ReaperSettings{config_->get_reaper_count(),
                                                                    config_->get_reaper_move_interval_ticks(),
                                                                    config_->get_reaper_kill_interval_ticks(),
                                                                    config_->get_reaper_announce_interval_ticks(),
                                                                    config_->get_reaper_warning_radius(),
                                                                    config_->get_reaper_newborn_protection_ticks()});
            PopsManager agents(world, logger_, config_);
            CheckpointCodec::restore(doc, *world, agents);
            reply["result"] = {{"valid", true}, {"populations", agents.get_alive_count()}};
        } else if (name == "checkpoint_load") {
            start(args.at("path").get<std::string>());
        } else if (name == "restart" || name == "start") {
            if (!locked_ini_.empty())
                throw std::runtime_error("Configuration locked: load a checkpoint to restart this run");
            if (name == "start" && sim_->state() != "finished")
                throw std::runtime_error("Run already active; use Restart");
            start();
        } else if (name == "headless")
            hide_gui();
        else if (name == "show_gui")
            show_gui();
        else if (name == "exit")
            quit_ = true;
        else if (name == "agents")
            reply["result"] = {{"tick", sim_->tick()}, {"agents", agents_->telemetry(false)}};
        else if (name == "inspect" || name == "snapshot") {
            auto snapshot = agents_->inspect(args.at("pop_id").get<std::string>());
            snapshot["tick"] = sim_->tick();
            snapshot["run_id"] = stats_->run_id();
            snapshot["snapshot_id"] = stats_->run_id() + "-" + snapshot["pop_id"].get<std::string>() + "-" +
                                      std::to_string(Clock::now().time_since_epoch().count());
            snapshot["save"] = name == "snapshot";
            reply["result"] = std::move(snapshot);
        } else {
            step_remaining_ = 0;
            sim_->command(name, args);
            if (name == "speed" && sim_->speed() == 0)
                hide_gui();
            reset_clock_ = true;
        }
        if (renderer_ && name == "newgen")
            renderer_->invalidate();
        finish_if_needed();
        reply["status"] = status();
    } catch (const std::exception& error) {
        reply["ok"] = false;
        reply["error"] = error.what();
        control_.error = error.what();
    }
    if (!id.empty()) {
        if (replies_.size() >= 2048)
            replies_.erase(replies_.begin());
        replies_[id] = reply;
        stats_->publish("reply", std::move(reply));
    }
    stats_->publish_status(status());
}
void PopXApp::finish_if_needed() {
    if (sim_->state() == "finished" && !finished_recorded_) {
        stats_->end_session(sim_->tick(), sim_->reason(), *agents_, *world_);
        finished_recorded_ = true;
    }
}
int PopXApp::run() {
    std::signal(SIGINT, on_signal);
    std::signal(SIGTERM, on_signal);
    start(options_.checkpoint_path);
    if (!options_.headless) {
        try {
            show_gui();
        } catch (const std::exception& e) {
            logger_->error(e.what());
        }
    }
    uint64_t rate_tick = sim_->tick(), rate_frames = 0;
    auto rate_time = Clock::now();
    auto next_tick = Clock::now(), next_frame = next_tick, next_publish = next_tick, last_frame = next_tick,
         idle_frame = next_tick;
    std::string failure;
    try {
        while (!quit_ && !interrupted) {
            for (auto& command : stats_->commands()) {
                dispatch(std::move(command));
                if (quit_)
                    break;
            }
            while (!quit_ && !control_.commands.empty()) {
                auto cmd = std::move(control_.commands.front());
                control_.commands.pop_front();
                dispatch(std::move(cmd));
            }
            if (quit_ || interrupted)
                break;
            if (reset_clock_) {
                next_tick = Clock::now() + RunClock::interval(sim_->speed());
                next_frame = Clock::now();
                last_frame = Clock::now();
                reset_clock_ = false;
            }
            const bool turbo = sim_->speed() == 0;
            const bool transport_busy = turbo && stats_->transport_status().value("queued", 0u) >= 128;
            for (unsigned count = 0; count < 8 && !transport_busy && (sim_->state() == "running" || step_remaining_) &&
                                     (step_remaining_ || turbo || Clock::now() >= next_tick);
                 ++count) {
                stats_->set_tick(sim_->tick() + 1);
                const auto begin = Clock::now();
                const bool stepping = step_remaining_ > 0;
                if (stepping)
                    sim_->command("resume", {});
                sim_->update();
                if (stepping) {
                    --step_remaining_;
                    if (sim_->state() == "running")
                        sim_->command("pause", {});
                    else
                        step_remaining_ = 0;
                }
                stats_->record_tick_cost(
                    std::chrono::duration_cast<std::chrono::microseconds>(Clock::now() - begin).count());
                if (sim_->tick() % 200 == 0 && sim_->state() != "finished")
                    stats_->sample(sim_->tick(), *agents_, *world_);
                finish_if_needed();
                next_tick += RunClock::interval(sim_->speed());
            }
            if (gui_) {
                renderer_->poll_event();
                if (!renderer_->window_open()) {
                    quit_ = true;
                    continue;
                }
            }
            const bool redraw = gui_ && (step_remaining_ || sim_->state() == "running" || gfx_->needs_redraw() ||
                                         Clock::now() >= idle_frame);
            if (redraw && Clock::now() >= next_frame) {
                const auto begin = Clock::now();
                renderer_->set_revision(sim_->tick());
                control_.status = status();
                renderer_->set_render_flags(flags_);
                if (!control_.selected_id.empty()) {
                    try {
                        // A single observer, sampled at 4 Hz, never accesses Pop on a worker.
                        static auto next_inspect = Clock::now();
                        if (Clock::now() >= next_inspect || control_.inspection.is_null()) {
                            control_.inspection = agents_->inspect(control_.selected_id);
                            control_.inspection["tick"] = sim_->tick();
                            next_inspect = Clock::now() + std::chrono::milliseconds(250);
                        }
                        auto pos = control_.inspection.at("position");
                        if (control_.follow)
                            gfx_->focus(pos[0], pos[1]);
                    } catch (...) {
                        control_.inspection = nullptr;
                        control_.selected_id.clear();
                        control_.follow = false;
                    }
                }
                renderer_->draw();
                if (!control_.inspection.is_null()) {
                    auto pos = control_.inspection.at("position");
                    const float x = pos[0].get<float>() * cell_render_size, y = pos[1].get<float>() * cell_render_size;
                    auto color = DesignSystem::color("accent");
                    gfx_->draw_line({x, y}, {x + cell_render_size, y}, color, 1);
                    gfx_->draw_line({x, y + cell_render_size}, {x + cell_render_size, y + cell_render_size}, color, 1);
                    gfx_->draw_line({x, y}, {x, y + cell_render_size}, color, 1);
                    gfx_->draw_line({x + cell_render_size, y}, {x + cell_render_size, y + cell_render_size}, color, 1);
                }
                const auto interval = std::chrono::duration_cast<std::chrono::microseconds>(begin - last_frame).count();
                gui_->update(std::max(0.001f, float(interval) / 1e6f));
                gui_->render();
                renderer_->present();
                const auto frame_us =
                    std::chrono::duration_cast<std::chrono::microseconds>(Clock::now() - begin).count();
                stats_->record_frame(frame_us, interval);
                ++rate_frames;
                idle_frame = begin + std::chrono::seconds(1);
                last_frame = begin;
                // Reserve wall time for the requested tick rate; render only from the remaining budget.
                // No biology is skipped. Paused windows redraw on input and otherwise idle at 1 Hz.
                const double tick_load = sim_->state() == "running"
                                             ? stats_->performance().value("avg_tick_us", 0.) * sim_->speed() / 10000.
                                             : 0.;
                const double render_budget = std::clamp(.85 - tick_load, .10, .50);
                next_frame =
                    begin + std::chrono::microseconds(std::max<int64_t>(
                                1000000 / config_->get_gui_fps(), static_cast<int64_t>(frame_us / render_budget)));
            }
            if (Clock::now() >= next_publish) {
                const auto now = Clock::now();
                effective_tps_ = sim_->tick() >= rate_tick ? (sim_->tick() - rate_tick) /
                                                                 std::chrono::duration<double>(now - rate_time).count()
                                                           : 0;
                render_fps_ = rate_frames / std::chrono::duration<double>(now - rate_time).count();
                rate_frames = 0;
                rate_tick = sim_->tick();
                rate_time = now;
                stats_->publish_status(status());
                next_publish = Clock::now() + std::chrono::seconds(1);
            }
            auto wake = Clock::now() + std::chrono::milliseconds(10);
            if (step_remaining_)
                wake = Clock::now();
            if (sim_->state() == "running" && !transport_busy)
                wake = turbo ? Clock::now() : std::min(wake, next_tick);
            if (redraw)
                wake = std::min(wake, next_frame);
            std::this_thread::sleep_until(wake);
        }
    } catch (const std::exception& error) {
        failure = error.what();
        logger_->error(failure);
    }
    const std::string exit_reason = !failure.empty() ? "error" : interrupted ? "signal" : "exit";
    if (!finished_recorded_)
        stats_->end_session(sim_->tick(), exit_reason, *agents_, *world_);
    auto final_status = status();
    final_status["state"] = "finished";
    if (!finished_recorded_)
        final_status["reason"] = exit_reason;
    if (!failure.empty())
        final_status["error"] = failure;
    stats_->publish_status(final_status);
    stats_->flush(1500);
    hide_gui();
    return failure.empty() ? 0 : 1;
}

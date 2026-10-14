#include "popx_app.h"

#include "console_logger.h"
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
    logger_ = std::make_shared<ConsoleLogger>();
    logger_->set_level(LogLevel::Error);
    stats_ = std::make_shared<Stats>(std::make_unique<UdpCommunicator>(options_.udp_port));
}
void PopXApp::start() {
    auto config = std::make_shared<Config>(options_.config_path, true);
    auto world = std::make_shared<GridWorld>(config->get_map_size(), config->get_map_size(), logger_);
    auto agents = std::make_shared<PopsManager>(world, logger_, config);
    auto sim = std::make_shared<Simulator>(world, agents, logger_);
    sim->set_duration(config->get_max_duration_minutes());
    sim->command("speed", {{"value", options_.speed}});
    sim->init(); // Prepare fully before replacing the old run.
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
    stats_->begin_session({{"kind", "evolution"},
                           {"config", config_->values()},
                           {"ini", raw},
                           {"config_path", config_->path()},
                           {"rng", "random_device and rand; no global deterministic seed"},
                           {"build", __VERSION__},
                           {"tick_seconds", 0.01},
                           {"model", "reserve_regulated_v1"}});
    agents_->set_stats(stats_);
    for (auto& p : agents_->telemetry(true))
        stats_->record_event({LifeEvent::Founder, std::move(p)});
    stats_->record_environment(*world_);
    stats_->sample(0, *agents_, *world_);
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
    auto renderer = std::make_unique<Renderer>(gfx, world_, config_);
    renderer->init();
    renderer->imgui_init();
    auto gui = std::make_unique<ImGuiLayer>(*gfx, *stats_, *agents_, control_, flags_);
    gui->init();
    gfx_ = std::move(gfx);
    renderer_ = std::move(renderer);
    gui_ = std::move(gui);
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
        if (name == "restart" || name == "start") {
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
            sim_->command(name, args);
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
    start();
    if (!options_.headless) {
        try {
            show_gui();
        } catch (const std::exception& e) {
            logger_->error(e.what());
        }
    }
    auto next_tick = Clock::now(), next_frame = next_tick, next_publish = next_tick, last_frame = next_tick,
         idle_frame = next_tick;
    std::string failure;
    try {
        while (!quit_ && !interrupted) {
            for (auto& command : stats_->commands())
                dispatch(std::move(command));
            while (!control_.commands.empty()) {
                auto cmd = std::move(control_.commands.front());
                control_.commands.pop_front();
                dispatch(std::move(cmd));
            }
            if (reset_clock_) {
                next_tick = Clock::now() + RunClock::interval(sim_->speed());
                next_frame = Clock::now();
                last_frame = Clock::now();
                reset_clock_ = false;
            }
            for (unsigned count = 0; count < 8 && sim_->state() == "running" && Clock::now() >= next_tick; ++count) {
                stats_->set_tick(sim_->tick() + 1);
                const auto begin = Clock::now();
                sim_->update();
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
            const bool redraw =
                gui_ && (sim_->state() == "running" || gfx_->needs_redraw() || Clock::now() >= idle_frame);
            if (redraw && Clock::now() >= next_frame) {
                const auto begin = Clock::now();
                const auto cpu_begin = std::clock();
                renderer_->set_revision(sim_->tick());
                control_.status = status();
                renderer_->set_render_flags(flags_);
                renderer_->draw();
                const auto interval = std::chrono::duration_cast<std::chrono::microseconds>(begin - last_frame).count();
                gui_->update(std::max(0.001f, float(interval) / 1e6f));
                gui_->render();
                renderer_->present();
                stats_->record_frame(
                    std::chrono::duration_cast<std::chrono::microseconds>(Clock::now() - begin).count(), interval);
                const auto cpu_us = static_cast<int64_t>(1e6 * double(std::clock() - cpu_begin) / CLOCKS_PER_SEC);
                idle_frame = begin + std::chrono::seconds(1);
                last_frame = begin;
                // gui_fps is a ceiling. On software/slow renderers, target at most 20% of one CPU
                // for drawing so bounded tick catch-up can retain the requested simulation rate.
                next_frame =
                    begin + std::chrono::microseconds(std::max<int64_t>(1000000 / config_->get_gui_fps(), cpu_us * 5));
            }
            if (Clock::now() >= next_publish) {
                stats_->publish_status(status());
                next_publish = Clock::now() + std::chrono::seconds(1);
            }
            auto wake = Clock::now() + std::chrono::milliseconds(10);
            if (sim_->state() == "running")
                wake = std::min(wake, next_tick);
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

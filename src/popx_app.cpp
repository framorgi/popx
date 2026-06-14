#include "popx_app.h"

#include <SFML/System.hpp>
#include <chrono>
#include <ctime>
#include <sstream>
#include <utility>

PopXApp::PopXApp(std::shared_ptr<ISimulator> sim, std::shared_ptr<IRenderer> renderer, std::shared_ptr<ILogger> logger,
                 std::shared_ptr<IStats> stats, std::shared_ptr<IAgentsManager> agents, std::shared_ptr<IWorld> world)
    : sim_(std::move(sim)), renderer_(std::move(renderer)), logger_(std::move(logger)), stats_(std::move(stats)),
      agents_(std::move(agents)), world_(std::move(world)) {}

void PopXApp::init() {
    logger_->info("POPx -- Initializing application");
    sim_->init();
    renderer_->init();       // creates SFML window
    renderer_->imgui_init(); // initialises Dear ImGui (must be after window creation)
    if (gui_)
        gui_->init();
    if (stats_)
        stats_->begin_session();
}

void PopXApp::run() {
    logger_->debug("Starting app loop");
    running_token_ = true;

    const sf::Time fixed_dt = sf::milliseconds(10);
    constexpr int stats_interval = 100; ///< record stats every N ticks

    sf::Clock clock;
    sf::Time accumulator = sf::Time::Zero;

    while (running_token_ && renderer_->window_open() && !sim_control_.terminate_requested) {
        // --- Event polling (also feeds ImGui::SFML::ProcessEvent) ---
        renderer_->poll_event();

        sf::Time frame_elapsed = clock.restart();
        auto frame_t0 = std::chrono::steady_clock::now();

        // --- Simulation update (skipped when paused or after end) ---
        if (!sim_control_.paused && !end_of_sim_) {
            accumulator += frame_elapsed;
            while (accumulator >= fixed_dt) {
                auto tick_t0 = std::chrono::steady_clock::now();
                sim_->update();
                auto tick_t1 = std::chrono::steady_clock::now();
                ++tick_count_;

                if (stats_ && tick_count_ % stats_interval == 0) {
                    auto dur = std::chrono::duration_cast<std::chrono::microseconds>(tick_t1 - tick_t0);
                    stats_->record_tick(tick_count_, agents_->get_generation_count(), agents_->get_alive_count(),
                                        agents_->get_dead_count(), agents_->get_total_births(),
                                        agents_->get_total_deaths(), world_->get_total_organics(), dur);
                }
                accumulator -= fixed_dt;
            }
        }

        // --- Manual NewGen trigger from GUI ---
        if (sim_control_.newgen_requested) {
            agents_->trigger_new_generation();
            sim_control_.newgen_requested = false;
        }

        // --- Restart trigger from GUI ---
        if (sim_control_.restart_requested) {
            agents_->reset_population_state();
            sim_->init();
            tick_count_ = 0;
            end_of_sim_ = false;
            sim_control_.paused = false;
            sim_control_.stop_requested = false;
            sim_control_.terminate_requested = false;
            sim_control_.restart_requested = false;
            sim_control_.stats_saved_recently = false;
            sim_control_.last_save_status = SimControl::SaveStatus::None;
            sim_control_.last_save_timestamp.clear();
            sim_control_.last_saved_json_path.clear();
            sim_control_.last_save_error.clear();
            accumulator = sf::Time::Zero;
            if (stats_) {
                stats_->begin_session();
            }
        }

        auto finish_simulation = [&]() {
            end_of_sim_ = true;
            sim_control_.paused = true;
            if (stats_) {
                stats_->end_session();
                (void)save_stats();
            }
            if (gui_)
                gui_->notify_end_of_simulation();
        };

        // --- Check end condition ---
        if (!end_of_sim_ && agents_->get_alive_count() == 0) {
            logger_->info("All pops have died — simulation ended.");
            finish_simulation();
        }

        if (!end_of_sim_ && sim_control_.stop_requested) {
            logger_->info("Simulation stopped from GUI.");
            finish_simulation();
        }

        // --- Handle save request from GUI ---
        if (sim_control_.save_stats_requested) {
            if (stats_) {
                (void)save_stats();
            }
            sim_control_.save_stats_requested = false;
        }

        // --- Apply render flags from GUI ---
        renderer_->set_render_flags(render_flags_);

        // --- Render simulation ---
        renderer_->draw();

        // --- Build & render ImGui (must happen after draw, before present) ---
        if (gui_) {
            gui_->update(frame_elapsed.asSeconds());
            gui_->render();
        }

        // --- Present final frame ---
        renderer_->present();

        // --- Record frame stats ---
        if (stats_) {
            auto frame_t1 = std::chrono::steady_clock::now();
            auto fdur = std::chrono::duration_cast<std::chrono::microseconds>(frame_t1 - frame_t0);
            stats_->record_frame(fdur);
        }

        // --- FPS cap (~60 FPS) ---
        auto frame_time = clock.getElapsedTime();
        if (frame_time < sf::milliseconds(16))
            sf::sleep(sf::milliseconds(16) - frame_time);
    }

    stop();
}

void PopXApp::stop() {
    logger_->info("POPx -- Stopping application");
    if (stats_ && !sim_control_.stats_saved_recently) {
        stats_->end_session();
        (void)save_stats();
    }

    if (gui_)
        gui_->shutdown();
    running_token_ = false;
}

bool PopXApp::save_stats() {
    // Build timestamped filename: data_out/stats_YYYYMMDD_HHMMSS.json
    auto now = std::chrono::system_clock::now();
    auto t = std::chrono::system_clock::to_time_t(now);
    std::tm* ltm = std::localtime(&t);
    char ts[32];
    std::strftime(ts, sizeof(ts), "%Y%m%d_%H%M%S", ltm);
    char ts_human[32];
    std::strftime(ts_human, sizeof(ts_human), "%Y-%m-%d %H:%M:%S", ltm);

    std::string path = "../data_out/stats_" + std::string(ts) + ".json";
    std::filesystem::create_directories("../data_out");
    if (!stats_->save_to_json(path)) {
        sim_control_.stats_saved_recently = false;
        sim_control_.last_save_status = SimControl::SaveStatus::Failed;
        sim_control_.last_save_timestamp = ts_human;
        sim_control_.last_saved_json_path.clear();
        sim_control_.last_save_error = "Unable to write stats file: " + path;
        logger_->error(sim_control_.last_save_error);
        return false;
    }

    sim_control_.last_saved_json_path = path;
    sim_control_.stats_saved_recently = true;
    sim_control_.last_save_status = SimControl::SaveStatus::Success;
    sim_control_.last_save_timestamp = ts_human;
    sim_control_.last_save_error.clear();
    logger_->info("Stats saved to " + path);
    return true;
}

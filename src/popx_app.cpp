#include "popx_app.h"

#include <thread>
#include <utility>

PopXApp::PopXApp(std::shared_ptr<ISimulator> sim, std::shared_ptr<IRenderer> renderer, std::shared_ptr<ILogger> logger)
    : sim_(std::move(sim)), renderer_(std::move(renderer)), logger_(std::move(logger)), running_token_(false) {}
void PopXApp::init() {
    logger_->info("POPx -- Initializing application");

    sim_->init();
    renderer_->init();
}
void PopXApp::run() {
    logger_->debug("Starting app loop  ");
    running_token_ = true;

    /// simulation timing (fixed time step) allow to decouple rendering frame rate from simulation update rate.
    // // Accumulator take trace of elapsed time between frames.
    // If too much time has elapsed, we may need to perform multiple simulation updates to catch up. If less time has
    // elapsed, we might skip simulation updates to maintain a consistent frame rate.
    const sf::Time dt = sf::milliseconds(10); // Fixed time step of 10 milliseconds (50 updates per second)

    sf::Clock clock;
    sf::Time accumulator = sf::Time::Zero;

    while (running_token_ && renderer_->window_open()) {
        renderer_->poll_event();

        sf::Time frame_start = clock.restart();
        accumulator += frame_start;
        while (accumulator >= dt) {
            logger_->info("App loop - simulation step ");
            sim_->update(); // <-- One simulation update per dt
            accumulator -= dt;
        }

        logger_->info("App loop - rendering step ");
        renderer_->draw();

        // fixed fps cap-- //TODO: adjust or remove- Vsync not working properly on WSL, maybe this can be removed once
        // enabled vsync
        auto frame_time = clock.getElapsedTime();
        if (frame_time < sf::milliseconds(5))
            sf::sleep(sf::milliseconds(5) - frame_time);

        // std::this_thread::sleep_for(std::chrono::milliseconds(1)); // Approx ~60 FPS
    }
}
void PopXApp::stop() {
    // TODO: Implement graceful shutdown (save state, cleanup resources, close window)
}

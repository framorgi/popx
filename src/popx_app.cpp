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
    while (running_token_) // TODO: while windows is open
    {
        sim_->update();

        renderer_->draw();
        // std::this_thread::sleep_for(std::chrono::milliseconds(1)); // Approx ~60 FPS
    }
}
void PopXApp::stop() {
    // TODO: Implement graceful shutdown (save state, cleanup resources, close window)
}

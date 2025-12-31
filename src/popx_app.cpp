#include "popx_app.h"

#include <utility>

PopXApp::PopXApp(std::shared_ptr<Simulator> sim, std::shared_ptr<Renderer> renderer,
                 std::shared_ptr<ConsoleLogger> logger)
    : sim_(std::move(sim)), renderer_(std::move(renderer)), running_token_(false), logger_(std::move(logger)) {}
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
    }
}
void PopXApp::stop() {
    // TODO: Implement graceful shutdown (save state, cleanup resources, close window)
}

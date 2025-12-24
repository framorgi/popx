#pragma once
#include "i_app.h"
#include "simulator.h"
#include "renderer.h"
#include  "console_logger.h"

class PopXApp : public IApp
{
public:
    PopXApp( std::shared_ptr<Simulator> sim, std::shared_ptr<Renderer> renderer,std::shared_ptr<ConsoleLogger> logger );
    void init() override;
    void run() override;
    void stop() override;

private:
    std::shared_ptr<Simulator> sim_;
    std::shared_ptr<Renderer> renderer_;
    std::shared_ptr<ConsoleLogger> logger_;
    bool running_token_;



};
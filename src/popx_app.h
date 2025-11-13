#pragma once
#include "i_app.h"
#include "simulator.h"
#include "renderer.h"
#include  "console_logger.h"

class popx_app : public i_app
{
public:
    popx_app( std::shared_ptr<simulator> sim, std::shared_ptr<renderer> renderer,std::shared_ptr<console_logger> logger );
    void init() override;
    void run() override;
    void stop() override;

private:
    std::shared_ptr<simulator> sim_;
    std::shared_ptr<renderer> renderer_;
    std::shared_ptr<console_logger> logger_;
    bool running_token_;



};
#pragma once
#include "i_app.h"
#include "simulator.h"
#include "renderer.h"
#include  "console_logger.h"


///--------------------------------------------------------------------------
/// @brief    Application class that implements the IApp interface  and its methods
///--------------------------------------------------------------------------
class PopXApp : public IApp
{
public:

///--------------------------------------------------------------------------
/// @brief    Constructor of PopXApp
/// @param    sim Shared pointer to the Simulator instance
/// @param    renderer Shared pointer to the Renderer instance
/// @param    logger Shared pointer to the Logger instance
///--------------------------------------------------------------------------

    PopXApp( std::shared_ptr<Simulator> sim, std::shared_ptr<Renderer> renderer,std::shared_ptr<ConsoleLogger> logger );
///--------------------------------------------------------------------------
/// @brief    Initializes the application
///--------------------------------------------------------------------------
    void init() override;
///--------------------------------------------------------------------------
/// @brief    Runs the application main loop
///--------------------------------------------------------------------------
    void run() override;
///--------------------------------------------------------------------------
/// @brief    Stops the application
///--------------------------------------------------------------------------
    void stop() override;

private:
///--------------------------------------------------------------------------
/// @brief    Shared pointer to the Simulator instance
///--------------------------------------------------------------------------
    std::shared_ptr<Simulator> sim_;
///--------------------------------------------------------------------------
/// @brief    Shared pointer to the Renderer instance
///--------------------------------------------------------------------------
    std::shared_ptr<Renderer> renderer_;
///--------------------------------------------------------------------------
/// @brief    Shared pointer to the Logger instance
///--------------------------------------------------------------------------
    std::shared_ptr<ConsoleLogger> logger_;

///--------------------------------------------------------------------------
/// @brief    Token to control the running state of the application
///--------------------------------------------------------------------------    
    bool running_token_;



};
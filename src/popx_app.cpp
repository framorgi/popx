#include "popx_app.h"

popx_app::popx_app(std::shared_ptr<simulator> sim, std::shared_ptr<renderer> renderer, std::shared_ptr<console_logger> logger)
    : sim_(sim), renderer_(renderer), running_token_(false), logger_(logger)
{
}
void popx_app::init()
{
    logger_->info("POPx -- Initializing application");


    sim_->init();
    renderer_->init();
}
void popx_app::run()
{   logger_->debug("Starting app loop  ");
    running_token_ = true;
    while (running_token_) //TODO: while windows is open
    {
      
        sim_->update(); 
        
       
      
        renderer_->draw( );
        

    }

} 
void popx_app::stop()
{
}

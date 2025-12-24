#pragma once
#include "i_renderer.h"
#include "sfml_graphic_engine.h"
#include "grid_world.h"
class Renderer : public IRenderer {
public:
    Renderer(std::shared_ptr<SfmlGraphicEngine> gfx,std::shared_ptr<GridWorld> world); 

    void init() override;
    void draw( ) override;
    void save_frame() override;

private:
    std::shared_ptr<SfmlGraphicEngine> gfx_;
    std::shared_ptr<GridWorld> world_;

    void draw_world();
    void draw_entity(   std::shared_ptr<IAgent> entity);
};

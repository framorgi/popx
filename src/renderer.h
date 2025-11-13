#pragma once
#include "i_renderer.h"
#include "sfml_graphic_engine.h"
#include "grid_world.h"
class renderer : public i_renderer {
public:
    renderer(std::shared_ptr<sfml_graphic_engine> gfx,std::shared_ptr<grid_world> world); 

    void init() override;
    void draw( ) override;
    void save_frame() override;

private:
    std::shared_ptr<sfml_graphic_engine> gfx_;
    std::shared_ptr<grid_world> world_;

    void draw_world();
    void draw_entity(   std::shared_ptr<i_agent> entity);
};

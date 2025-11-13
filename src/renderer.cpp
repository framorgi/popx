#include "renderer.h"

renderer::renderer(std::shared_ptr<sfml_graphic_engine> gfx,std::shared_ptr<grid_world> world) :gfx_(gfx), world_(world) {

}

void renderer::init() {
    // Inizializzazione se necessaria

    gfx_->create_window("PopX Simulation", 300, 300);
}   

void renderer::draw() {
    // Esempio di disegno del mondo
    gfx_->clear();

    Position center{50, 50};

    i_graphic_engine::Circle circle({i_graphic_engine::Vec2{static_cast<float>(center.x), static_cast<float>(center.y)}, 20.f});
    gfx_->draw_circle(circle, i_graphic_engine::Color(255, 0, 0));

    gfx_->display();
}   

void renderer::save_frame() {
    // Implementazione per salvare il frame corrente
}

void renderer::draw_world( ) {
    // Implementazione per disegnare lo stato del mondo
    for (const auto& slice : world_->get_slices()) {
        auto entity = slice->get_occupant();
        if (entity) {
            draw_entity(entity);
        }
    }
}

void renderer::draw_entity(std::shared_ptr<i_agent> entity) {
    // Implementazione per disegnare le entità
    Position pos = entity->get_position();
    i_graphic_engine::Circle circle({i_graphic_engine::Vec2{static_cast<float>(pos.x), static_cast<float>(pos.y)}, 5.f});
    gfx_->draw_circle(circle, i_graphic_engine::Color(0, 255, 0));
}
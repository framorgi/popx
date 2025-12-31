#include "renderer.h"

Renderer::Renderer(std::shared_ptr<SfmlGraphicEngine> gfx, std::shared_ptr<GridWorld> world)
    : gfx_(gfx), world_(std::move(world)) {}

void Renderer::init() {
    // TODO: Complete initialization (load resources, setup camera, configure rendering settings)

    gfx_->create_window("PopX Simulation", 300, 300);
}

void Renderer::draw() {
    // TODO: Replace test drawing with actual world rendering (call draw_world())
    gfx_->clear();

    Position center{50, 50};

    IGraphicEngine::Circle circle(
        {IGraphicEngine::Vec2{static_cast<float>(center.x), static_cast<float>(center.y)}, 20.f});
    gfx_->draw_circle(circle, IGraphicEngine::Color(255, 0, 0));

    gfx_->display();
}

void Renderer::save_frame() {
    // TODO: Implement frame saving to file (screenshot, video recording)
}

void Renderer::draw_world() {
    // TODO: Add grid rendering, background, UI elements
    for (const auto& slice : world_->get_slices()) {
        auto weak_entity = slice->get_occupant();
        auto entity = weak_entity.lock(); // Lock the weak_ptr to get shared_ptr
        if (entity) {
            draw_entity(entity.get()); // Pass raw pointer to draw function
        }
    }
}

void Renderer::draw_entity(IEntity* entity) {
    // TODO: Improve entity rendering (different colors/shapes per type, animations, health bars)

    Position pos = entity->get_position();
    IGraphicEngine::Circle circle({IGraphicEngine::Vec2{static_cast<float>(pos.x), static_cast<float>(pos.y)}, 5.f});
    gfx_->draw_circle(circle, IGraphicEngine::Color(0, 255, 0));
}
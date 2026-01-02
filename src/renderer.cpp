#include "renderer.h"

#include "i_graphic_engine.h"
#include "i_world.h"
#include "random_utility.h"

#include <cstdint>

Renderer::Renderer(std::shared_ptr<IGraphicEngine> gfx, std::shared_ptr<IWorld> world)
    : gfx_(std::move(gfx)), world_(std::move(world)) {}

void Renderer::init() {
    // TODO: Complete initialization (load resources, setup camera, configure rendering settings)

    gfx_->create_window("PopX Simulation", cell_render_size * world_->get_width(),
                        cell_render_size * world_->get_height());
}

void Renderer::draw() {
    // TODO: Replace test drawing with actual world rendering (call draw_world())
    gfx_->clear();

    // IGraphicEngine::Circle circle(
    //     {IGraphicEngine::Vec2{static_cast<float>(center.x), static_cast<float>(center.y)}, 10.f});
    // gfx_->draw_circle(circle, IGraphicEngine::Color(255, 150, 0));
    draw_world();

    gfx_->display();
}

void Renderer::save_frame() {
    // TODO: Implement frame saving to file (screenshot, video recording)
}

void Renderer::draw_world() {
    // TODO: Add grid rendering, background, UI elements

    int width = world_->get_width();
    int height = world_->get_height();
    for (int x = 0; x < width; ++x) {
        for (int y = 0; y < height; ++y) {
            auto cell = world_->get_cell(y * width + x);
            draw_cell(cell, x, y);
            if (auto entity = cell->get_occupant().lock()) {
                draw_entity(entity);
            }
        }
    }
}

void Renderer::draw_entity(const std::shared_ptr<IEntity>& entity) {
    // TODO: Improve entity rendering (different colors/shapes per type, animations, health bars)
    if (!entity) {
        return;
    }

    Position pos = entity->get_position();
    IGraphicEngine::Circle circle(
        {IGraphicEngine::Vec2{static_cast<float>(cell_render_size * pos.x + cell_render_size / 2),
                              static_cast<float>(cell_render_size * pos.y + cell_render_size / 2)},
         cell_render_size / 2.f});
    gfx_->draw_circle(circle, IGraphicEngine::Color(150, 150, 0));
}

void Renderer::draw_cell(const std::shared_ptr<ICell>& cell, int x, int y)

{
    IGraphicEngine::Rect rect{
        IGraphicEngine::Vec2{static_cast<float>(x * cell_render_size), static_cast<float>(y * cell_render_size)},
        IGraphicEngine::Vec2{cell_render_size, cell_render_size}};
    IGraphicEngine::Color temperature_color = evaluate_temperature_color(cell->get_temperature());
    double elevation = cell->get_elevation();
    IGraphicEngine::Color ground_color = evaluate_ground_color(elevation);

    gfx_->draw_rectangle(rect, apply_highlighting(blend_colors(ground_color, temperature_color, 0.1), elevation));
}

IGraphicEngine::Color Renderer::evaluate_temperature_color(double temperature) {
    // Define key intervals
    if (temperature <= 0) {
        return {0, 0, 255}; // Dark blue for low temperatures
    }
    if (temperature <= 10) {
        float factor = temperature / 10.0f;
        return {0, static_cast<int>(255 * factor), 255}; // From blue to cyan
    }
    if (temperature <= 20) {
        float factor = (temperature - 10) / 10.0f;
        return {0, 255, static_cast<int>(255 * (1 - factor))}; // From cyan to green
    }
    if (temperature <= 30) {
        float factor = (temperature - 20) / 10.0f;
        return {0, 255, static_cast<int>(255 * factor)}; // From green to yellow
    }
    if (temperature <= 40) {
        float factor = (temperature - 30) / 10.0f;
        return {255, static_cast<int>(255 * (1 - factor)), 0}; // From yellow to red
    } else {
        return {255, 0, 0}; // Dark red for high temperatures
    }
}

uint8_t Renderer::clamp_u8(int value) {
    return static_cast<uint8_t>(std::clamp(value, 0, 255));
}

IGraphicEngine::Color Renderer::color_interpolate(const IGraphicEngine::Color& a, const IGraphicEngine::Color& b,
                                                  double t) {
    return IGraphicEngine::Color{clamp_u8(static_cast<int>(a.r + t * (b.r - a.r))),
                                 clamp_u8(static_cast<int>(a.g + t * (b.g - a.g))),
                                 clamp_u8(static_cast<int>(a.b + t * (b.b - a.b)))};
}

IGraphicEngine::Color Renderer::evaluate_ground_color(double elevation) {
    IGraphicEngine::Color base;
    // RandomUtility rand_util;

    for (int i = 0; i < 9; ++i) {
        if (elevation >= ground_color_levels[i].height && elevation <= ground_color_levels[i + 1].height) {
            // float t = (elevation - ground_color_levels[i].height) /
            //(ground_color_levels[i + 1].height - ground_color_levels[i].height);
            // base = color_interpolate(ground_color_levels[i].color, ground_color_levels[i + 1].color, t);
            base = ground_color_levels[i].color;
            break;
        } else {
            base = ground_color_levels[9].color;
        }
    }

    const int noise = 5; // intensità controllata
    return base;
}

IGraphicEngine::Color Renderer::blend_colors(const IGraphicEngine::Color& baseColor,
                                             const IGraphicEngine::Color& blendColor, double factor) {
    // Clamp factor to [0.0, 1.0]
    factor = (factor < 0.0) ? 0.0 : (factor > 1.0) ? 1.0 : factor;

    int r = static_cast<int>(baseColor.r * (1.0 - factor) + blendColor.r * factor);
    int g = static_cast<int>(baseColor.g * (1.0 - factor) + blendColor.g * factor);
    int b = static_cast<int>(baseColor.b * (1.0 - factor) + blendColor.b * factor);
    int a = static_cast<int>(baseColor.a * (1.0 - factor) + blendColor.a * factor);

    // Clamp values to [0, 255]
    r = (r < 0) ? 0 : (r > 255) ? 255 : r;
    g = (g < 0) ? 0 : (g > 255) ? 255 : g;
    b = (b < 0) ? 0 : (b > 255) ? 255 : b;
    a = (a < 0) ? 0 : (a > 255) ? 255 : a;

    return IGraphicEngine::Color(r, g, b, a);
}
IGraphicEngine::Color Renderer::apply_highlighting(const IGraphicEngine::Color& color, double height) {
    double height_factor = std::min(1.0, height / 100.0);
    double lighting = 0.7 + 0.3 * height_factor;
    return IGraphicEngine::Color(static_cast<uint8_t>(std::clamp(color.r * lighting, 0.0, 255.0)),
                                 static_cast<uint8_t>(std::clamp(color.g * lighting, 0.0, 255.0)),
                                 static_cast<uint8_t>(std::clamp(color.b * lighting, 0.0, 255.0)));
}
#include "renderer.h"

#include <cmath>

Renderer::Renderer(std::shared_ptr<IGraphicEngine> gfx, std::shared_ptr<IWorld> world)
    : gfx_(std::move(gfx)), world_(std::move(world)) {}

void Renderer::init() {
    // TODO: Complete initialization (load resources, setup camera, configure rendering settings)

    gfx_->create_window("PopX Simulation", cell_render_size * world_->get_width(),
                        cell_render_size * world_->get_height());
    world_height_ = world_->get_height();
    world_width_ = world_->get_width();
    int total_size = world_width_ * world_height_;
    map_buffer_.resize(total_size); // Preallocate buffer for the entire map

    // Initialize map buffer with default quads
    for (int y = 0; y < world_height_; ++y) {
        for (int x = 0; x < world_width_; ++x) {
            int index = y * world_width_ + x;
            map_buffer_[index] = IGraphicEngine::Quad{
                IGraphicEngine::Vec2{static_cast<float>(x * cell_render_size),
                                     static_cast<float>(y * cell_render_size)},
                IGraphicEngine::Vec2{static_cast<float>(cell_render_size), static_cast<float>(cell_render_size)},
                IGraphicEngine::Color(0, 0, 0)};
        }
    }
}

void Renderer::draw() {
    // TODO: Replace test drawing with actual world rendering (call draw_world())
    gfx_->clear();
    // clear entity buffer every frame for now. Entity size can change very quickly so we need to redraw them all but
    // this section can be optimized later
    entity_buffer_.clear();
    entity_buffer_.reserve(MAX_ENTITIES * SEGMENTS * 3);
    update_world();
    gfx_->draw_quads(map_buffer_);
    gfx_->draw_triangles(entity_buffer_);
    gfx_->display();
}

void Renderer::save_frame() {
    // TODO: Implement frame saving to file (screenshot, video recording)
}

void Renderer::update_world() {
    for (int x = 0; x < world_width_; ++x) {
        for (int y = 0; y < world_height_; ++y) {
            auto cell = world_->get_cell(y * world_width_ + x);
            if (cell->need_rendering()) {
                update_cell(cell, x, y);
            }
            if (auto entity = cell->get_occupant().lock()) {
                update_entity(entity);
            }
        }
    }
}

void Renderer::update_entity(const std::shared_ptr<IEntity>& entity) {
    // TODO: Improve entity rendering (different colors/shapes per type, animations, health bars)
    if (!entity) {
        return;
    }

    constexpr float PI = 3.14159265359f;

    Position pos = entity->get_position();

    float cx = cell_render_size * pos.x + cell_render_size * 0.5f;
    float cy = cell_render_size * pos.y + cell_render_size * 0.5f;
    float radius = cell_render_size * 0.5f;

    IGraphicEngine::Color color(150, 150, 0);

    // foreach segment
    for (int i = 0; i < SEGMENTS; ++i) {
        float a0 = (i / static_cast<float>(SEGMENTS)) * 2.f * PI;
        float a1 = ((i + 1) / static_cast<float>(SEGMENTS)) * 2.f * PI;

        IGraphicEngine::Vec2 center{cx, cy};
        IGraphicEngine::Vec2 p0{cx + std::cos(a0) * radius, cy + std::sin(a0) * radius};
        IGraphicEngine::Vec2 p1{cx + std::cos(a1) * radius, cy + std::sin(a1) * radius};

        entity_buffer_.push_back({center, color});
        entity_buffer_.push_back({p0, color});
        entity_buffer_.push_back({p1, color});
    }
}

void Renderer::update_cell(const std::shared_ptr<ICell>& cell, int x, int y) {
    int idx = y * world_width_ + x;

    // get the relative quad from the preallocated buffer
    auto& q = map_buffer_[idx];

    IGraphicEngine::Rect rect{
        IGraphicEngine::Vec2{static_cast<float>(x * cell_render_size), static_cast<float>(y * cell_render_size)},
        IGraphicEngine::Vec2{static_cast<float>(cell_render_size), static_cast<float>(cell_render_size)}};
    // Determine cell color based on temperature
    IGraphicEngine::Color temperature_color = evaluate_temperature_color(cell->get_temperature());
    // Determine ground color based on elevation
    double elevation = cell->get_elevation();
    IGraphicEngine::Color ground_color = evaluate_ground_color(elevation);
    // Blend temperature and ground colors and finally apoply highlighting based on elevation
    IGraphicEngine::Color final_color =
        apply_highlighting(blend_colors(ground_color, temperature_color, 0.1), elevation);

    // Store the final color in the cell for future reference
    q.color = final_color;
    // Reset rendering flag
    cell->reset_need_rendering();
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
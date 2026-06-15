#include "renderer.h"

#include "common.h"

#include <algorithm>
#include <cmath>

Renderer::Renderer(std::shared_ptr<IGraphicEngine> gfx, std::shared_ptr<IWorld> world, std::shared_ptr<IConfig> config)
    : gfx_(std::move(gfx)), world_(std::move(world)), config_(std::move(config)) {}

void Renderer::init() {
    // TODO: Complete initialization (load resources, setup camera, configure rendering settings)

    gfx_->create_window("PopX Simulation", cell_render_size * world_->get_width(),
                        cell_render_size * world_->get_height());
    world_height_ = world_->get_height();
    world_width_ = world_->get_width();
    population_ = static_cast<int>(config_->get_start_population());
    int total_size = world_width_ * world_height_;
    map_layer_.resize(total_size); // Preallocate buffer for the entire map

    // Initialize map buffer with default quads
    for (int y = 0; y < world_height_; ++y) {
        for (int x = 0; x < world_width_; ++x) {
            int index = y * world_width_ + x;
            map_layer_[index] = IGraphicEngine::Quad{
                Vec2{static_cast<float>(x * cell_render_size), static_cast<float>(y * cell_render_size)},
                Vec2{static_cast<float>(cell_render_size), static_cast<float>(cell_render_size)}, Color(0, 0, 0)};
        }
    }
}

void Renderer::draw() {
    gfx_->clear();

    entity_layer_.clear();
    entity_layer_.reserve(static_cast<std::size_t>(population_) * SEGMENTS * 3);

    feromone_layer_.clear();
    feromone_layer_.reserve(world_width_ * world_height_ * SEGMENTS * 3);

    resource_layer_.clear();
    resource_layer_.reserve(world_width_ * world_height_ * GLUCOSE_SEGMENTS * 3);

    update_world();

    gfx_->draw_quads(map_layer_);

    if (flags_.show_glucose || flags_.active_overlay != ResourceOverlay::None) {
        gfx_->draw_triangles(resource_layer_);
    }

    gfx_->draw_triangles(feromone_layer_);
    gfx_->draw_triangles(entity_layer_);
}

void Renderer::save_frame() {
    // TODO: Implement frame saving to file (screenshot, video recording)
}

void Renderer::update_world() {
    for (int x = 0; x < world_width_; ++x) {
        for (int y = 0; y < world_height_; ++y) {
            auto cell = world_->get_cell(y * world_width_ + x);
            update_cell(cell, x, y);

            if (auto entity = cell->get_occupant().lock()) {
                update_entity(entity);
            }
        }
    }
}

void Renderer::update_entity(const std::shared_ptr<IEntity>& entity) {
    if (!entity || !flags_.show_entities) {
        return;
    }

    constexpr float PI = 3.14159265359f;
    // every entity defines a method for getting its render state

    RenderState rs = entity->get_render_state();
    PositionT pos = {static_cast<int>(rs.position.x), static_cast<int>(rs.position.y)};
    if (auto pop = std::get_if<PopVisualData>(&rs.payload)) {
        float cx = cell_render_size * pos.x + cell_render_size * rs.size / 2;
        float cy = cell_render_size * pos.y + cell_render_size * rs.size / 2;
        float radius = cell_render_size * rs.size / 2;

        if (pop->is_photosynthetic) {
            // draw simple triangle for photosynthetic entities
            for (int i = 0; i < 3; ++i) {
                float a0 = (i / static_cast<float>(3)) * 2.f * PI;
                float a1 = ((i + 1) / static_cast<float>(3)) * 2.f * PI;

                Vec2 center{cx, cy - radius};
                Vec2 p0{cx + radius, cy + radius};
                Vec2 p1{cx - radius, cy + radius};

                entity_layer_.push_back({center, rs.color});
                entity_layer_.push_back({p0, rs.color});
                entity_layer_.push_back({p1, rs.color});
            }
        } else {
            // foreach segment
            for (int i = 0; i < SEGMENTS; ++i) {
                float a0 = (i / static_cast<float>(SEGMENTS)) * 2.f * PI;
                float a1 = ((i + 1) / static_cast<float>(SEGMENTS)) * 2.f * PI;

                Vec2 center{cx, cy};
                Vec2 p0{cx + std::cos(a0) * radius, cy + std::sin(a0) * radius};
                Vec2 p1{cx + std::cos(a1) * radius, cy + std::sin(a1) * radius};

                entity_layer_.push_back({center, rs.color});
                entity_layer_.push_back({p0, rs.color});
                entity_layer_.push_back({p1, rs.color});
            }
        }
    }
}

void Renderer::update_cell(const std::shared_ptr<ICell>& cell, int x, int y) {
    constexpr float PI = 3.14159265359f;
    int idx = y * world_width_ + x;

    // get the render state from the cell
    auto rs = cell->get_render_state();
    // get payload data

    if (auto cell_data = std::get_if<CellVisualData>(&rs.payload)) {
        auto& q = map_layer_[idx];

        Color temperature_color = evaluate_temperature_color(cell->get_temperature());
        double elevation = cell_data->elevation;
        Color ground_color = evaluate_ground_color(elevation);

        q.color = flags_.show_temperature
                      ? temperature_color
                      : apply_highlighting(blend_colors(ground_color, temperature_color, 0.1), elevation);

        auto add_resource_blob = [&](double intensity, const Color& color) {
            if (intensity <= 0.0) {
                return;
            }
            float cx = cell_render_size * static_cast<float>(x) + cell_render_size;
            float cy = cell_render_size * static_cast<float>(y) + cell_render_size;
            float radius = std::max(0.8f, static_cast<float>(cell_render_size * intensity * 0.24));

            for (int i = 0; i < GLUCOSE_SEGMENTS; ++i) {
                float a0 = (i / static_cast<float>(GLUCOSE_SEGMENTS)) * 2.f * PI;
                float a1 = ((i + 1) / static_cast<float>(GLUCOSE_SEGMENTS)) * 2.f * PI;

                Vec2 center{cx, cy};
                Vec2 p0{cx + std::cos(a0) * radius, cy + std::sin(a0) * radius};
                Vec2 p1{cx + std::cos(a1) * radius, cy + std::sin(a1) * radius};

                resource_layer_.push_back({center, color});
                resource_layer_.push_back({p0, color});
                resource_layer_.push_back({p1, color});
            }
        };

        const auto organics = cell->get_organics();
        const auto overlay = flags_.active_overlay;
        switch (overlay) {
            case ResourceOverlay::Temperature:
                if (flags_.filter_temperature)
                    add_resource_blob(std::clamp(cell->get_temperature() / 100.0, 0.0, 1.0), Color(255, 80, 30, 140));
                break;
            case ResourceOverlay::Glucose:
                if (flags_.filter_glucose)
                    add_resource_blob(static_cast<double>(organics.c6h12o6) / MaxC6h12o6, Color(20, 20, 20, 140));
                break;
            case ResourceOverlay::Water:
                if (flags_.filter_water)
                    add_resource_blob(static_cast<double>(organics.h2o) / MaxH2o, Color(40, 120, 255, 140));
                break;
            case ResourceOverlay::Oxygen:
                if (flags_.filter_oxygen)
                    add_resource_blob(static_cast<double>(organics.o2) / MaxO2, Color(120, 235, 255, 140));
                break;
            case ResourceOverlay::CarbonDioxide:
                if (flags_.filter_co2)
                    add_resource_blob(static_cast<double>(organics.co2) / MaxCo2, Color(180, 120, 120, 140));
                break;
            case ResourceOverlay::Lipids:
                if (flags_.filter_lipids)
                    add_resource_blob(static_cast<double>(organics.lipids) / MaxLipids, Color(255, 200, 30, 140));
                break;
            case ResourceOverlay::Nitrogen:
                if (flags_.filter_nitrogen)
                    add_resource_blob(static_cast<double>(organics.n2) / MaxN2, Color(180, 255, 180, 140));
                break;
            case ResourceOverlay::CalciumCarbonate:
                if (flags_.filter_caco3)
                    add_resource_blob(static_cast<double>(organics.caco3) / MaxCaco3, Color(220, 220, 220, 140));
                break;
            case ResourceOverlay::None:
                if (flags_.show_glucose && flags_.filter_glucose) {
                    add_resource_blob(static_cast<double>(organics.c6h12o6) / MaxC6h12o6, Color(20, 20, 20, 140));
                }
                break;
            default:
                break;
        }

        auto add_feromone_layer = [&](double intensity, const Color& feromone_color) {
            if (intensity == 0.0) {
                return;
            }
            float cx = cell_render_size * static_cast<float>(x) + cell_render_size;
            float cy = cell_render_size * static_cast<float>(y) + cell_render_size;
            float radius = cell_render_size * static_cast<float>(intensity) / 1.7f;

            for (int i = 0; i < FEROMONE_SEGMENTS; ++i) {
                float a0 = (i / static_cast<float>(FEROMONE_SEGMENTS)) * 2.f * PI;
                float a1 = ((i + 1) / static_cast<float>(FEROMONE_SEGMENTS)) * 2.f * PI;

                Vec2 center{cx, cy};
                Vec2 p0{cx + std::cos(a0) * radius, cy + std::sin(a0) * radius};
                Vec2 p1{cx + std::cos(a1) * radius, cy + std::sin(a1) * radius};

                feromone_layer_.push_back({center, feromone_color});
                feromone_layer_.push_back({p0, feromone_color});
                feromone_layer_.push_back({p1, feromone_color});
            }
        };

        if (flags_.show_feromone_food)
            add_feromone_layer(cell_data->feromones_a,
                               Color(255, 0, 100, static_cast<uint8_t>(cell_data->feromones_a * 200)));
        if (flags_.show_feromone_danger)
            add_feromone_layer(cell_data->feromones_b,
                               Color(55, 240, 200, static_cast<uint8_t>(cell_data->feromones_b * 200)));
        if (flags_.show_feromone_mate)
            add_feromone_layer(cell_data->feromones_c,
                               Color(55, 220, 0, static_cast<uint8_t>(cell_data->feromones_c * 200)));
        if (flags_.show_feromone_home)
            add_feromone_layer(cell_data->feromones_d,
                               Color(0, 10, 100, static_cast<uint8_t>(cell_data->feromones_d * 200)));
    }
}

bool Renderer::window_open() const {
    return gfx_->is_open();
}

void Renderer::poll_event() {
    gfx_->poll_event();
}

void Renderer::set_render_flags(const RenderFlags& flags) {
    flags_ = flags;
}

void Renderer::imgui_init() {
    gfx_->imgui_init();
}

void Renderer::present() {
    gfx_->display();
}

Color Renderer::evaluate_temperature_color(double temperature) {
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

Color Renderer::color_interpolate(const Color& a, const Color& b, double t) {
    return Color{clamp_u8(static_cast<int>(a.r + t * (b.r - a.r))), clamp_u8(static_cast<int>(a.g + t * (b.g - a.g))),
                 clamp_u8(static_cast<int>(a.b + t * (b.b - a.b)))};
}

Color Renderer::evaluate_ground_color(double elevation) {
    Color base;
    // RandomUtility rand_util;

    for (int i = 0; i < ground_color_levels.size() - 1; ++i) {
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

Color Renderer::blend_colors(const Color& base_color, const Color& blend_color, double factor) {
    // Clamp factor to [0.0, 1.0]
    factor = (factor < 0.0) ? 0.0 : (factor > 1.0) ? 1.0 : factor;

    int r = static_cast<int>(base_color.r * (1.0 - factor) + blend_color.r * factor);
    int g = static_cast<int>(base_color.g * (1.0 - factor) + blend_color.g * factor);
    int b = static_cast<int>(base_color.b * (1.0 - factor) + blend_color.b * factor);
    int a = static_cast<int>(base_color.a * (1.0 - factor) + blend_color.a * factor);

    // Clamp values to [0, 255]
    r = (r < 0) ? 0 : (r > 255) ? 255 : r;
    g = (g < 0) ? 0 : (g > 255) ? 255 : g;
    b = (b < 0) ? 0 : (b > 255) ? 255 : b;
    a = (a < 0) ? 0 : (a > 255) ? 255 : a;

    return {r, g, b, a};
}
Color Renderer::apply_highlighting(const Color& color, double height) {
    double height_factor = std::min(1.0, height / 100.0);
    double lighting = 0.7 + 0.3 * height_factor;
    return Color(static_cast<uint8_t>(std::clamp(color.r * lighting, 0.0, 255.0)),
                 static_cast<uint8_t>(std::clamp(color.g * lighting, 0.0, 255.0)),
                 static_cast<uint8_t>(std::clamp(color.b * lighting, 0.0, 255.0)));
}
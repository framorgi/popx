#include "renderer.h"

#include "common.h"
#include "design_system.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace {
template <int N>
const std::array<Vec2, N + 1>& unit_circle() {
    static const auto points = [] {
        std::array<Vec2, N + 1> result{};
        for (int i = 0; i <= N; ++i) {
            const float angle = 6.28318530718f * i / N;
            result[i] = {std::cos(angle), std::sin(angle)};
        }
        return result;
    }();
    return points;
}
} // namespace

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

    if (dirty_) {
        entity_layer_.clear();
        entity_layer_.reserve(static_cast<std::size_t>(population_) * SEGMENTS * 3);

        feromone_layer_.clear();
        feromone_layer_.reserve(world_width_ * world_height_ * SEGMENTS * 3);

        resource_layer_.clear();
        resource_layer_.reserve(world_width_ * world_height_ * GLUCOSE_SEGMENTS * 3);

        update_world();

        dirty_ = false;
    }

    gfx_->draw_quads(map_layer_);

    if (flags_.show_glucose || (flags_.active_overlay >= ResourceOverlay::Glucose &&
                                flags_.active_overlay <= ResourceOverlay::CalciumCarbonate)) {
        gfx_->draw_triangles(resource_layer_);
    }

    gfx_->draw_triangles(feromone_layer_);
    gfx_->draw_triangles(entity_layer_);
    auto sfml = std::dynamic_pointer_cast<SfmlGraphicEngine>(gfx_);
    if (DesignSystem::settings.value("grid", false) && sfml && sfml->zoom() >= 4) {
        auto line = DesignSystem::color("line");
        line.a = 90;
        for (int x = 0; x <= world_width_; ++x)
            gfx_->draw_line({float(x * cell_render_size), 0},
                            {float(x * cell_render_size), float(world_height_ * cell_render_size)}, line, .3f);
        for (int y = 0; y <= world_height_; ++y)
            gfx_->draw_line({0, float(y * cell_render_size)},
                            {float(world_width_ * cell_render_size), float(y * cell_render_size)}, line, .3f);
    }
}

void Renderer::save_frame() {
    // TODO: Implement frame saving to file (screenshot, video recording)
}

void Renderer::update_world() {
    background_ = DesignSystem::color("bg");
    muted_ = DesignSystem::color("muted");
    accent_ = DesignSystem::color("accent");
    cold_ = DesignSystem::color("cold");
    warm_ = DesignSystem::color("warm");
    hot_ = DesignSystem::color("hot");
    const char* names[] = {"glucose", "water", "oxygen", "co2", "lipids", "nitrogen", "calcium"};
    for (unsigned i = 0; i < 7; ++i)
        resource_colors_[i] = DesignSystem::hex(DesignSystem::tokens.at("resources").at(names[i]));
    pheromone_colors_ = {warm_, DesignSystem::color("red"), accent_, cold_};
    if (flags_.active_overlay == ResourceOverlay::Population) {
        const unsigned stride = world_width_ + 1;
        density_prefix_.assign(stride * (world_height_ + 1), 0);
        for (int y = 0; y < world_height_; ++y)
            for (int x = 0; x < world_width_; ++x) {
                auto occupant = world_->get_cell(y * world_width_ + x)->get_occupant().lock();
                unsigned value =
                    occupant && std::holds_alternative<PopVisualData>(occupant->get_render_state().payload);
                density_prefix_[(y + 1) * stride + x + 1] = value + density_prefix_[y * stride + x + 1] +
                                                            density_prefix_[(y + 1) * stride + x] -
                                                            density_prefix_[y * stride + x];
            }
    }
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
    if (std::holds_alternative<ReaperVisualData>(rs.payload)) {
        const float cx = cell_render_size * (pos.x + .5f), cy = cell_render_size * (pos.y + .5f);
        auto octagon = [&](float radius, Color color) {
            for (int i = 0; i < 8; ++i) {
                const float a = (i + .5f) * PI / 4, b = (i + 1.5f) * PI / 4;
                for (Vec2 p : {Vec2{cx, cy}, Vec2{cx + radius * std::cos(a), cy + radius * std::sin(a)},
                               Vec2{cx + radius * std::cos(b), cy + radius * std::sin(b)}})
                    entity_layer_.push_back({p, color});
            }
        };
        octagon(cell_render_size * .67f, Color(252, 190, 168));
        octagon(cell_render_size * .53f, Color(83, 17, 33));
        return;
    }
    if (std::holds_alternative<SourceVisualData>(rs.payload)) {
        // A bordered black diamond distinguishes Source from circular/triangular Pops.
        const float cx = cell_render_size * (pos.x + 0.5f);
        const float cy = cell_render_size * (pos.y + 0.5f);
        auto diamond = [&](float radius, Color color) {
            const Vec2 top{cx, cy - radius}, right{cx + radius, cy};
            const Vec2 bottom{cx, cy + radius}, left{cx - radius, cy};
            for (const auto& triangle :
                 {std::array<Vec2, 3>{top, right, bottom}, std::array<Vec2, 3>{top, bottom, left}})
                for (const auto& vertex : triangle)
                    entity_layer_.push_back({vertex, color});
        };
        diamond(cell_render_size * 0.58f, Color(235, 235, 235));
        diamond(cell_render_size * 0.48f, Color(0, 0, 0));
        return;
    }
    if (auto pop = std::get_if<PopVisualData>(&rs.payload)) {
        float cx = cell_render_size * (pos.x + .5f);
        float cy = cell_render_size * (pos.y + .5f);
        float radius = cell_render_size * rs.size / 2 * DesignSystem::settings.value("agent_scale", .85f);

        if (pop->is_photosynthetic) {
            // draw simple triangle for photosynthetic entities
            for (int i = 0; i < 1; ++i) {
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
                const auto& circle = unit_circle<SEGMENTS>();

                Vec2 center{cx, cy};
                Vec2 p0{cx + circle[i].x * radius, cy + circle[i].y * radius};
                Vec2 p1{cx + circle[i + 1].x * radius, cy + circle[i + 1].y * radius};

                entity_layer_.push_back({center, rs.color});
                entity_layer_.push_back({p0, rs.color});
                entity_layer_.push_back({p1, rs.color});
            }
        }
    }
}

void Renderer::update_cell(const std::shared_ptr<ICell>& cell, int x, int y) {
    auto& quad = map_layer_[y * world_width_ + x];
    const auto organics = cell->get_organics();
    const auto elevation = std::clamp(cell->get_elevation() / 100., 0., 1.);
    const auto base = blend_colors(background_, muted_, .08 + .75 * elevation);
    quad.color = base;
    auto overlay = flags_.active_overlay;
    if (overlay == ResourceOverlay::None && flags_.show_temperature)
        overlay = ResourceOverlay::Temperature;
    if (overlay == ResourceOverlay::None && flags_.show_glucose)
        overlay = ResourceOverlay::Glucose;
    double intensity = 0;
    int resource = -1;
    switch (overlay) {
        case ResourceOverlay::Terrain:
            quad.color = evaluate_ground_color(cell->get_elevation());
            break;
        case ResourceOverlay::Temperature:
            quad.color = evaluate_temperature_color(cell->get_temperature());
            break;
        case ResourceOverlay::Glucose:
            intensity = double(organics.c6h12o6) / MaxC6h12o6;
            resource = 0;
            break;
        case ResourceOverlay::Water:
            intensity = double(organics.h2o) / MaxH2o;
            resource = 1;
            break;
        case ResourceOverlay::Oxygen:
            intensity = double(organics.o2) / MaxO2;
            resource = 2;
            break;
        case ResourceOverlay::CarbonDioxide:
            intensity = double(organics.co2) / MaxCo2;
            resource = 3;
            break;
        case ResourceOverlay::Lipids:
            intensity = double(organics.lipids) / MaxLipids;
            resource = 4;
            break;
        case ResourceOverlay::Nitrogen:
            intensity = double(organics.n2) / MaxN2;
            resource = 5;
            break;
        case ResourceOverlay::CalciumCarbonate:
            intensity = double(organics.caco3) / MaxCaco3;
            resource = 6;
            break;
        case ResourceOverlay::Population: {
            const unsigned stride = world_width_ + 1;
            const int x0 = std::max(0, x - 2), y0 = std::max(0, y - 2), x1 = std::min(world_width_, x + 3),
                      y1 = std::min(world_height_, y + 3);
            const auto occupied = density_prefix_[y1 * stride + x1] + density_prefix_[y0 * stride + x0] -
                                  density_prefix_[y1 * stride + x0] - density_prefix_[y0 * stride + x1];
            quad.color = blend_colors(base, accent_, double(occupied) / ((x1 - x0) * (y1 - y0)));
            break;
        }
        default:
            break;
    }
    auto add_quad = [&](std::vector<IGraphicEngine::EntityVertex>& vertices, float x0, float y0, float x1,
                        float y1, Color color) {
        const Vec2 a{x0, y0}, b{x1, y0}, c{x1, y1}, d{x0, y1};
        for (const Vec2 point : {a, b, c, a, c, d}) vertices.push_back({point, color});
    };
    if (resource >= 0 && intensity > 0) {
        const float side = cell_render_size * .5f;
        const float x0 = cell_render_size * (x + .5f) - side / 2;
        const float y0 = cell_render_size * (y + .5f) - side / 2;
        auto color = resource_colors_[resource];
        color.a = clamp_u8(40 + int(215 * std::clamp(intensity, 0., 1.)));
        add_quad(resource_layer_, x0, y0, x0 + side, y0 + side, color);
    }
    if (!flags_.show_feromone_food && !flags_.show_feromone_danger && !flags_.show_feromone_mate &&
        !flags_.show_feromone_home)
        return;
    const auto pheromones = cell->get_feromone_map();
    const bool enabled[] = {flags_.show_feromone_food, flags_.show_feromone_danger, flags_.show_feromone_mate,
                            flags_.show_feromone_home};
    for (unsigned i = 0; i < 4; ++i) {
        if (!enabled[i]) continue;
        const double strength = std::clamp(double(pheromones.at(static_cast<FeromoneT>(i))) / MaxFeromones, 0., 1.);
        if (strength <= .001) continue;
        // Eight vertices give each pheromone marker softly clipped, rounded corners.
        const float cx = cell_render_size * (x + (i % 2 ? .73f : .27f));
        const float cy = cell_render_size * (y + (i / 2 ? .73f : .27f));
        const float r = cell_render_size * .20f, cut = r * .38f;
        const std::array<Vec2, 8> ring{{{cx-r+cut,cy-r},{cx+r-cut,cy-r},{cx+r,cy-r+cut},
                                        {cx+r,cy+r-cut},{cx+r-cut,cy+r},{cx-r+cut,cy+r},
                                        {cx-r,cy+r-cut},{cx-r,cy-r+cut}}};
        auto color = pheromone_colors_[i];
        color.a = clamp_u8(50 + int(205 * strength));
        for (unsigned k = 0; k < ring.size(); ++k)
            for (Vec2 point : {Vec2{cx,cy}, ring[k], ring[(k+1)%ring.size()]})
                feromone_layer_.push_back({point, color});
    }
}

bool Renderer::window_open() const {
    return gfx_->is_open();
}

void Renderer::poll_event() {
    gfx_->poll_event();
}

void Renderer::set_render_flags(const RenderFlags& flags) {
    if (flags_ != flags)
        dirty_ = true;
    flags_ = flags;
}

void Renderer::imgui_init() {
    gfx_->imgui_init();
}

void Renderer::present() {
    gfx_->display();
}

Color Renderer::evaluate_temperature_color(double temperature) const {
    if (temperature <= 30)
        return color_interpolate(cold_, warm_, std::clamp(temperature / 30., 0., 1.));
    return color_interpolate(warm_, hot_, std::clamp((temperature - 30) / 30., 0., 1.));
}

Color Renderer::color_interpolate(const Color& a, const Color& b, double t) {
    return Color{clamp_u8(static_cast<int>(a.r + t * (b.r - a.r))), clamp_u8(static_cast<int>(a.g + t * (b.g - a.g))),
                 clamp_u8(static_cast<int>(a.b + t * (b.b - a.b)))};
}

Color Renderer::evaluate_ground_color(double elevation) {
    const double clamped = std::clamp(elevation, ground_color_levels.front().height,
                                      ground_color_levels.back().height);
    for (size_t i = 1; i < ground_color_levels.size(); ++i)
        if (clamped <= ground_color_levels[i].height) {
            const auto& low = ground_color_levels[i - 1];
            const auto& high = ground_color_levels[i];
            return color_interpolate(low.color, high.color, (clamped - low.height) / (high.height - low.height));
        }
    return ground_color_levels.back().color;
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
uint8_t Renderer::clamp_u8(int value) {
    return static_cast<uint8_t>(std::clamp(value, 0, 255));
}

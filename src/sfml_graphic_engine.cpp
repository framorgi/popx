#include "sfml_graphic_engine.h"

#include "common.h"
#include "design_system.h"
#include "i_renderer.h"

#include <X11/Xlib.h>
#include <imgui-SFML.h>
#include <imgui.h>
#include <unistd.h>

#include <algorithm>
#include <cmath>
#include <filesystem>

namespace {
void request_maximize(sf::RenderWindow& window) {
    Display* display = XOpenDisplay(nullptr);
    if (!display)
        return;
    const auto state = XInternAtom(display, "_NET_WM_STATE", False);
    const auto horizontal = XInternAtom(display, "_NET_WM_STATE_MAXIMIZED_HORZ", False);
    const auto vertical = XInternAtom(display, "_NET_WM_STATE_MAXIMIZED_VERT", False);
    XEvent event{};
    event.xclient.type = ClientMessage;
    event.xclient.window = window.getSystemHandle();
    event.xclient.message_type = state;
    event.xclient.format = 32;
    event.xclient.data.l[0] = 1; // _NET_WM_STATE_ADD
    event.xclient.data.l[1] = static_cast<long>(horizontal);
    event.xclient.data.l[2] = static_cast<long>(vertical);
    XSendEvent(display, DefaultRootWindow(display), False, SubstructureRedirectMask | SubstructureNotifyMask, &event);
    XFlush(display);
    XCloseDisplay(display);
}
} // namespace

SfmlGraphicEngine::SfmlGraphicEngine(std::shared_ptr<ILogger> logger) : logger_(std::move(logger)) {}

SfmlGraphicEngine::~SfmlGraphicEngine() {
    // SFML must release its active cursor while ImGui-owned cursors are still alive.
    window_.close();
    if (imgui_initialized_)
        ImGui::SFML::Shutdown();
}

void SfmlGraphicEngine::create_window(std::string title, int width, int height) {
    Display* display = XOpenDisplay(nullptr);
    const auto screen = display ? DefaultScreen(display) : 0;
    const auto screen_width = display ? DisplayWidth(display, screen) : sf::VideoMode::getDesktopMode().width;
    const auto screen_height = display ? DisplayHeight(display, screen) : sf::VideoMode::getDesktopMode().height;
    Window root, child;
    int root_x = 0, root_y = 0, window_x, window_y;
    unsigned mask;
    if (display)
        XQueryPointer(display, DefaultRootWindow(display), &root, &child, &root_x, &root_y, &window_x, &window_y,
                      &mask);
    if (display)
        XCloseDisplay(display);

    const auto initial_width = std::min(1280u, screen_width);
    const auto initial_height = std::min(720u, screen_height);
    window_.create(sf::VideoMode(initial_width, initial_height), title + " [" + std::to_string(getpid()) + "]",
                   sf::Style::Default);
    window_.setPosition(
        {std::clamp(root_x - static_cast<int>(initial_width / 2), 0, static_cast<int>(screen_width - initial_width)),
         std::clamp(root_y - static_cast<int>(initial_height / 2), 0,
                    static_cast<int>(screen_height - initial_height))});
    request_maximize(window_);
    world_size_ = {float(width), float(height)};
    world_view_.reset({0, 0, float(width), float(height)});
    const auto client_size = window_.getSize();
    update_world_viewport(client_size.x, client_size.y);
}

float SfmlGraphicEngine::sidebar_width() const {
    return std::min(float(window_.getSize().x) * .45f, float(DesignSystem::tokens.at("sidebar")));
}
void SfmlGraphicEngine::update_world_viewport(unsigned width, unsigned height) {
    if (!width || !height)
        return;
    const float usable = std::max(1.f, float(width) - sidebar_width());
    const float aspect = usable / height;
    fit_size_ = world_size_;
    if (aspect > world_size_.x / world_size_.y)
        fit_size_.x = world_size_.y * aspect;
    else
        fit_size_.y = world_size_.x / aspect;
    world_view_.setSize(fit_size_ / zoom_);
    world_view_.setViewport({0, 0, usable / width, 1});
    constrain_camera();
}
void SfmlGraphicEngine::constrain_camera() {
    auto center = world_view_.getCenter(), size = world_view_.getSize();
    center.x =
        size.x >= world_size_.x ? world_size_.x / 2 : std::clamp(center.x, size.x / 2, world_size_.x - size.x / 2);
    center.y =
        size.y >= world_size_.y ? world_size_.y / 2 : std::clamp(center.y, size.y / 2, world_size_.y - size.y / 2);
    world_view_.setCenter(center);
}
void SfmlGraphicEngine::reset_camera() {
    zoom_ = 1;
    world_view_.setSize(fit_size_);
    world_view_.setCenter(world_size_ / 2.f);
    if (controls_)
        controls_->follow = false;
    event_pending_ = true;
}
void SfmlGraphicEngine::focus(float x, float y) {
    world_view_.setCenter((x + .5f) * cell_render_size, (y + .5f) * cell_render_size);
    constrain_camera();
}
void SfmlGraphicEngine::capture(const std::string& path) {
    sf::Texture texture;
    if (!texture.create(window_.getSize().x, window_.getSize().y))
        throw std::runtime_error("Screenshot allocation failed");
    texture.update(window_);
    if (!texture.copyToImage().saveToFile(path))
        throw std::runtime_error("Screenshot write failed");
}

void SfmlGraphicEngine::clear(const Color& color) {
    const auto bg = DesignSystem::color("bg");
    window_.clear(sf::Color(bg.r, bg.g, bg.b));
    window_.setView(world_view_);
}

void SfmlGraphicEngine::display() {
    window_.display();
    event_pending_ = false;
}
bool SfmlGraphicEngine::is_open() const {
    return window_.isOpen();
}
void SfmlGraphicEngine::poll_event() {
    auto paint = [&](sf::Vector2i mouse, int button) {
        if (!controls_ || controls_->status.value("state", "") != "paused" ||
            controls_->status.value("step_remaining", 0u) != 0) return;
        const auto world = window_.mapPixelToCoords(mouse, world_view_);
        if (world.x < 0 || world.y < 0 || world.x >= world_size_.x || world.y >= world_size_.y) return;
        const sf::Vector2i target{int(world.x / cell_render_size), int(world.y / cell_render_size)};
        auto emit = [&](int x, int y) {
            const auto& field = controls_->brush_field;
            nlohmann::json args = {{"x", x}, {"y", y}, {"erase", button == sf::Mouse::Right}, {"brush", true}};
            if (field == "pop" || field == "source") {
                args["kind"] = field;
                controls_->request(button == sf::Mouse::Right ? "sandbox_remove" : "sandbox_spawn", args);
            } else {
                args["field"] = field;
                args["value"] = controls_->brush_value;
                controls_->request("sandbox_cell", args);
            }
            controls_->cell_x = x;
            controls_->cell_y = y;
        };
        if (target == last_paint_cell_) return;
        const auto start = last_paint_cell_.x < 0 ? target : last_paint_cell_;
        int x = start.x, y = start.y;
        const int dx = std::abs(target.x - x), sx = x < target.x ? 1 : -1;
        const int dy = -std::abs(target.y - y), sy = y < target.y ? 1 : -1;
        int error = dx + dy;
        while (true) {
            if (x != start.x || y != start.y || last_paint_cell_.x < 0) emit(x, y);
            if (x == target.x && y == target.y) break;
            const int twice = 2 * error;
            if (twice >= dy) { error += dy; x += sx; }
            if (twice <= dx) { error += dx; y += sy; }
        }
        last_paint_cell_ = target;
    };
    sf::Event event;
    while (window_.pollEvent(event)) {
        event_pending_ = true;
        ImGui::SFML::ProcessEvent(window_, event);
        if (event.type == sf::Event::Resized)
            update_world_viewport(event.size.width, event.size.height);
        if (event.type == sf::Event::Closed) {
            window_.close();
            logger_->info("Window closed by user.");
        }
        auto mouse = sf::Mouse::getPosition(window_);
        if (event.type == sf::Event::MouseButtonPressed || event.type == sf::Event::MouseButtonReleased)
            mouse = {event.mouseButton.x, event.mouseButton.y};
        else if (event.type == sf::Event::MouseWheelScrolled)
            mouse = {event.mouseWheelScroll.x, event.mouseWheelScroll.y};
        else if (event.type == sf::Event::MouseMoved)
            mouse = {event.mouseMove.x, event.mouseMove.y};
        const bool on_map = mouse.x >= 0 && mouse.y >= 0 && mouse.x < int(window_.getSize().x - sidebar_width()) &&
                            mouse.y < int(window_.getSize().y);
        if (event.type == sf::Event::MouseWheelScrolled && on_map && !ImGui::GetIO().WantCaptureMouse) {
            auto anchor = window_.mapPixelToCoords(mouse, world_view_);
            zoom_ = std::clamp(zoom_ * std::pow(1.2f, event.mouseWheelScroll.delta), 1.f, 24.f);
            world_view_.setSize(fit_size_ / zoom_);
            world_view_.move(anchor - window_.mapPixelToCoords(mouse, world_view_));
            constrain_camera();
        }
        const bool brush = controls_ && controls_->brush_enabled && controls_->status.value("state", "") == "paused" &&
                           controls_->status.value("step_remaining", 0u) == 0;
        if (event.type == sf::Event::MouseButtonPressed && on_map && !ImGui::GetIO().WantCaptureMouse) {
            if (brush && (event.mouseButton.button == sf::Mouse::Left || event.mouseButton.button == sf::Mouse::Right)) {
                paint_button_ = event.mouseButton.button;
                last_paint_cell_ = {-1, -1};
                paint(mouse, paint_button_);
            } else if (event.mouseButton.button == sf::Mouse::Right || event.mouseButton.button == sf::Mouse::Middle) {
                dragging_ = true;
                drag_pixel_ = mouse;
                if (controls_)
                    controls_->follow = false;
            }
            if (!brush && event.mouseButton.button == sf::Mouse::Left && controls_) {
                auto p = window_.mapPixelToCoords(mouse, world_view_);
                if (p.x >= 0 && p.y >= 0 && p.x < world_size_.x && p.y < world_size_.y)
                    controls_->request("select_cell",
                                       {{"x", int(p.x / cell_render_size)}, {"y", int(p.y / cell_render_size)}});
            }
        }
        if (event.type == sf::Event::MouseMoved && paint_button_ >= 0 && on_map)
            paint(mouse, paint_button_);
        if (event.type == sf::Event::MouseMoved && dragging_) {
            world_view_.move(window_.mapPixelToCoords(drag_pixel_, world_view_) -
                             window_.mapPixelToCoords(mouse, world_view_));
            drag_pixel_ = mouse;
            constrain_camera();
        }
        if (event.type == sf::Event::MouseButtonReleased || event.type == sf::Event::LostFocus) {
            dragging_ = false;
            paint_button_ = -1;
            last_paint_cell_ = {-1, -1};
        }
        if (event.type == sf::Event::KeyPressed && controls_ && !ImGui::GetIO().WantCaptureKeyboard) {
            if (event.key.code >= sf::Keyboard::F1 && event.key.code <= sf::Keyboard::F3)
                controls_->panel_request = event.key.code - sf::Keyboard::F1;
            if (event.key.code == sf::Keyboard::F4)
                controls_->open_monitor = true;
            if (event.key.code == sf::Keyboard::F)
                controls_->follow = !controls_->follow;
            if (event.key.code == sf::Keyboard::Home)
                reset_camera();
            if (event.key.code == sf::Keyboard::Space)
                controls_->request(controls_->status.value("state", "") == "paused" ? "resume" : "pause");
            if (event.key.code == sf::Keyboard::Period)
                controls_->request("step", {{"ticks", 1}});
        }
    }
}
void SfmlGraphicEngine::draw_circle(const Circle& circle, const Color& color) {
    sf::CircleShape shape(circle.radius);
    shape.setPosition(circle.center.x - circle.radius, circle.center.y - circle.radius);
    shape.setFillColor(sf::Color(color.r, color.g, color.b, color.a));
    window_.draw(shape);
}
void SfmlGraphicEngine::draw_rectangle(const Rect& rect, const Color& color) {
    sf::RectangleShape shape(sf::Vector2f(rect.size.x, rect.size.y));
    shape.setPosition(rect.pos.x, rect.pos.y);
    shape.setFillColor(sf::Color(color.r, color.g, color.b, color.a));
    window_.draw(shape);
}
void SfmlGraphicEngine::draw_line(const Vec2& from, const Vec2& to, const Color& color, float thickness) {
    sf::RectangleShape shape({std::hypot(to.x - from.x, to.y - from.y), thickness});
    shape.setPosition(from.x, from.y);
    shape.setRotation(std::atan2(to.y - from.y, to.x - from.x) * 180.f / 3.14159265f);
    shape.setFillColor(sf::Color(color.r, color.g, color.b, color.a));
    window_.draw(shape);
}

void SfmlGraphicEngine::draw_point(const Vec2& point, const Color& color) {
    sf::Vertex vertex(sf::Vector2f(point.x, point.y), sf::Color(color.r, color.g, color.b, color.a));
    window_.draw(&vertex, 1, sf::Points);
}

void SfmlGraphicEngine::draw_polygon(const std::vector<Vec2>& vertices, const Color& color) {
    sf::ConvexShape shape;
    shape.setPointCount(vertices.size());
    for (size_t i = 0; i < vertices.size(); ++i) {
        shape.setPoint(i, sf::Vector2f(vertices[i].x, vertices[i].y));
    }
    shape.setFillColor(sf::Color(color.r, color.g, color.b, color.a));
    window_.draw(shape);
}

void SfmlGraphicEngine::draw_text(const std::string& text, const Vec2& pos, int size, const Color& color,
                                  const std::string& fontName) {
    static sf::Font font;
    static bool font_loaded = false;
    if (!font_loaded) {
        if (!font.loadFromFile(fontName.empty() ? "arial.ttf" : fontName)) {
            // Handle error
        }
        font_loaded = true;
    }

    sf::Text sf_text;
    sf_text.setFont(font);
    sf_text.setString(text);
    sf_text.setCharacterSize(size);
    sf_text.setFillColor(sf::Color(color.r, color.g, color.b, color.a));
    sf_text.setPosition(pos.x, pos.y);

    window_.draw(sf_text);
}
void SfmlGraphicEngine::draw_triangles(const std::vector<EntityVertex>& vertices) {
    if (vertices.empty()) {
        return;
    }
    triangle_vertices_.setPrimitiveType(sf::Triangles);
    triangle_vertices_.resize(vertices.size());

    for (std::size_t i = 0; i < vertices.size(); ++i) {
        const EntityVertex& ev = vertices[i];
        triangle_vertices_[i].position = sf::Vector2f(ev.pos.x, ev.pos.y);
        triangle_vertices_[i].color = sf::Color(ev.color.r, ev.color.g, ev.color.b, ev.color.a);
    }

    window_.draw(triangle_vertices_);
}
void SfmlGraphicEngine::draw_quads(const std::vector<Quad>& quads) {
    if (quads.empty()) {
        return;
    }
    if (!initialized_) {
        quad_vertices_.setPrimitiveType(sf::Quads);
        quad_vertices_.resize(quads.size() * 4);

        for (std::size_t i = 0; i < quads.size(); ++i) {
            const Quad& q = quads[i];
            std::size_t idx = i * 4;

            float x = q.pos.x;
            float y = q.pos.y;
            float w = q.size.x;
            float h = q.size.y;

            quad_vertices_[idx + 0].position = {x, y};
            quad_vertices_[idx + 1].position = {x + w, y};
            quad_vertices_[idx + 2].position = {x + w, y + h};
            quad_vertices_[idx + 3].position = {x, y + h};
        }

        initialized_ = true;
    }

    // aggiorna SOLO i colori
    for (std::size_t i = 0; i < quads.size(); ++i) {
        const sf::Color c(static_cast<sf::Uint8>(quads[i].color.r), static_cast<sf::Uint8>(quads[i].color.g),
                          static_cast<sf::Uint8>(quads[i].color.b), static_cast<sf::Uint8>(quads[i].color.a));

        std::size_t idx = i * 4;
        quad_vertices_[idx + 0].color = c;
        quad_vertices_[idx + 1].color = c;
        quad_vertices_[idx + 2].color = c;
        quad_vertices_[idx + 3].color = c;
    }

    window_.draw(quad_vertices_);
}

void SfmlGraphicEngine::imgui_init() {
    imgui_initialized_ = ImGui::SFML::Init(window_);
    if (!imgui_initialized_) {
        window_.close();
        ImGui::SFML::Shutdown();
        throw std::runtime_error("ImGui initialization failed");
    }
    const std::string font = "/usr/share/fonts/truetype/lato/Lato-Regular.ttf";
    if (imgui_initialized_ && std::filesystem::exists(font)) {
        ImGui::GetIO().Fonts->Clear();
        ImGui::GetIO().Fonts->AddFontFromFileTTF(font.c_str(), 17.f);
        (void)ImGui::SFML::UpdateFontTexture();
    }
}

void SfmlGraphicEngine::imgui_new_frame(float dt_seconds) {
    window_.setView(window_.getDefaultView());
    ImGui::SFML::Update(window_, sf::seconds(dt_seconds));
}

void SfmlGraphicEngine::imgui_render() {
    ImGui::SFML::Render(window_);
}

#include "sfml_graphic_engine.h"

#include <X11/Xlib.h>
#include <imgui-SFML.h>
#include <imgui.h>
#include <unistd.h>

#include <algorithm>
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
    world_view_.reset({0, 0, float(width), float(height)});
    const auto client_size = window_.getSize();
    update_world_viewport(client_size.x, client_size.y);
}

void SfmlGraphicEngine::update_world_viewport(unsigned client_width, unsigned client_height) {
    const float side = std::min(float(client_height), std::max(1.f, float(client_width) - 320.f));
    world_view_.setViewport({0, 0, side / client_width, side / client_height});
}

void SfmlGraphicEngine::clear(const Color& color) {
    window_.clear(sf::Color(14, 19, 28));
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
        if (event.type == sf::Event::KeyPressed) {
            logger_->info("Key pressed: " + std::to_string(event.key.code));
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
void SfmlGraphicEngine::draw_line(const Vec2& from, const Vec2& to, const Color& color, float thickness) {}

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

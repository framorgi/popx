#include "sfml_graphic_engine.h"

SfmlGraphicEngine::SfmlGraphicEngine(std::shared_ptr<ILogger> logger) : logger_(std::move(logger)) {}

void SfmlGraphicEngine::create_window(std::string title, int width, int height) {
    window_.create(sf::VideoMode(width, height), title);
    window_.setVerticalSyncEnabled(true);
}

void SfmlGraphicEngine::clear(const Color& color) {
    window_.clear(sf::Color(color.r, color.g, color.b, color.a));
}

void SfmlGraphicEngine::display() {
    window_.display();
    sf::sleep(sf::milliseconds(1));
}
bool SfmlGraphicEngine::is_open() const {
    return window_.isOpen();
}
void SfmlGraphicEngine::poll_event() {
    sf::Event event;
    while (window_.pollEvent(event)) {
        if (event.type == sf::Event::Closed) {
            window_.close();
            logger_->info("Window closed by user.");
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
    sf::VertexArray vertex_array(sf::Triangles, vertices.size());

    for (std::size_t i = 0; i < vertices.size(); ++i) {
        const EntityVertex& ev = vertices[i];
        vertex_array[i].position = sf::Vector2f(ev.pos.x, ev.pos.y);
        vertex_array[i].color = sf::Color(ev.color.r, ev.color.g, ev.color.b, ev.color.a);
    }

    window_.draw(vertex_array);
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
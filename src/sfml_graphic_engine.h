#pragma once
#include "i_graphic_engine.h"
#include "i_logger.h"

#include <SFML/Graphics.hpp>

class SfmlGraphicEngine : public IGraphicEngine {
  public:
    SfmlGraphicEngine(std::shared_ptr<ILogger> logger);
    ~SfmlGraphicEngine() override;

    // --------------------------
    // Gestione finestra / frame
    // --------------------------
    void clear(const Color& color = Color(0, 0, 0)) override;
    void create_window(std::string title, int width, int height) override;
    void display() override;
    bool is_open() const override;
    void poll_event() override;

    // --------------------------
    // Primitive di disegno
    // --------------------------
    void draw_circle(const Circle& circle, const Color& color) override;
    void draw_rectangle(const Rect& rect, const Color& color) override;
    void draw_line(const Vec2& from, const Vec2& to, const Color& color, float thickness = 1.f) override;
    void draw_point(const Vec2& point, const Color& color) override;
    void draw_polygon(const std::vector<Vec2>& vertices, const Color& color) override;

    // --------------------------
    // Testo
    // --------------------------
    void draw_text(const std::string& text, const Vec2& pos, int size, const Color& color = Color(255, 255, 255),
                   const std::string& fontName = "") override;

    void draw_quads(const std::vector<Quad>& quads) override;
    void draw_triangles(const std::vector<EntityVertex>& vertices) override;

    // --------------------------
    // ImGui integration
    // --------------------------
    void imgui_init() override;
    void imgui_new_frame(float dt_seconds) override;
    void imgui_render() override;

  private:
    std::shared_ptr<ILogger> logger_;
    sf::RenderWindow window_;
    sf::VertexArray quad_vertices_;
    sf::VertexArray triangle_vertices_;
    bool initialized_ = false;
};

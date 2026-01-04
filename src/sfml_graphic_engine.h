#pragma once
#include "i_graphic_engine.h"

#include <SFML/Graphics.hpp>

class SfmlGraphicEngine : public IGraphicEngine {
  public:
    SfmlGraphicEngine();

    // --------------------------
    // Gestione finestra / frame
    // --------------------------
    void clear(const Color& color = Color(0, 0, 0)) override;
    void create_window(std::string title, int width, int height) override;
    void display() override;
    bool is_open() const override;
    void refresh() override;

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

    ///--------------------------------------------------------------------------
    /// @brief    Draws a buffer of quads
    /// @param    quads Vector of quads to draw
    ///--------------------------------------------------------------------------
    void draw_quads(const std::vector<Quad>& quads) override;

    ///--------------------------------------------------------------------------
    /// @brief    Draws a buffer of triangles representing entities
    /// @param    vertices Vector of entity vertices to draw
    ///--------------------------------------------------------------------------
    void draw_triangles(const std::vector<EntityVertex>& vertices) override;

  private:
    sf::RenderWindow window_;

    ///--------------------------------------------------------------------------
    /// @brief    Vertex array for drawing map quads
    ///--------------------------------------------------------------------------
    sf::VertexArray quad_vertices_;

    ///--------------------------------------------------------------------------
    /// @brief    Flag indicating if the engine has been initialized and the map buffer created
    ///--------------------------------------------------------------------------
    bool initialized_ = false;
};

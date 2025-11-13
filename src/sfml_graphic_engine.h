#pragma once
#include "i_graphic_engine.h"
#include <SFML/Graphics.hpp>

class sfml_graphic_engine : public i_graphic_engine {
public:
    sfml_graphic_engine( );

    // --------------------------
    // Gestione finestra / frame
    // --------------------------
    void clear(const Color& color = Color(0,0,0)) override;
    void create_window(std::string title, int width, int height) override;
    void display() override;
    bool is_open() const override;
    void refresh() override;

    // --------------------------
    // Primitive di disegno
    // --------------------------
    void draw_circle(const Circle& circle, const Color& color) override ;
    void draw_rectangle(const Rect& rect, const Color& color) override;
    void draw_line(const Vec2& from, const Vec2& to, const Color& color, float thickness = 1.f) override;
    void draw_point(const Vec2& point, const Color& color) override;
    void draw_polygon(const std::vector<Vec2>& vertices, const Color& color) override;

    // --------------------------
    // Testo
    // --------------------------
    void draw_text(const std::string& text, const Vec2& pos, int size,
                  const Color& color = Color(255,255,255), const std::string& fontName = "") override;

private:
    sf::RenderWindow window_;

    
};

#pragma once    
#include <memory>
#include <string>
#include <vector>

class i_graphic_engine {

public:
    virtual ~i_graphic_engine() = default;
    // --------------------------
    // Tipi di supporto integrati
    // --------------------------
    struct Color {
        int r = 0, g = 0, b = 0, a = 255;
        Color() = default;
        Color(int red, int green, int blue, int alpha = 255)
            : r(red), g(green), b(blue), a(alpha) {}
    };

    struct Vec2 {
        float x = 0.f, y = 0.f;
        Vec2() = default;
        Vec2(float x_, float y_) : x(x_), y(y_) {}
    };

    struct Rect {
        Vec2 pos;
        Vec2 size;
        Rect() = default;
        Rect(const Vec2& p, const Vec2& s) : pos(p), size(s) {}
    };

    struct Circle {
        Vec2 center;
        float radius = 0.f;
        Circle() = default;
        Circle(const Vec2& c, float r) : center(c), radius(r) {}
    };

    // --------------------------
    // Gestione finestra / frame
    // --------------------------
    virtual void clear(const Color& color = Color(0,0,0)) = 0;
    virtual void create_window(std::string title, int width, int height) = 0;
    virtual void display() = 0;
    virtual bool is_open() const = 0;
    virtual void refresh() = 0;

    // --------------------------
    // Primitive di disegno
    // --------------------------
    virtual void draw_circle(const Circle& circle, const Color& color) = 0;
    virtual void draw_rectangle(const Rect& rect, const Color& color) = 0;
    virtual void draw_line(const Vec2& from, const Vec2& to, const Color& color, float thickness = 1.f) = 0;
    virtual void draw_point(const Vec2& point, const Color& color) = 0;
    virtual void draw_polygon(const std::vector<Vec2>& vertices, const Color& color) = 0;

    // --------------------------
    // Testo
    // --------------------------
    virtual void draw_text(const std::string& text, const Vec2& pos, int size,
                          const Color& color = Color(255,255,255), const std::string& fontName = "") = 0;
    };
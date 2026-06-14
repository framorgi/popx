#pragma once
#include "color.h"
#include "geometry.h"

#include <memory>
#include <string>
#include <vector>
class IGraphicEngine {
  public:
    virtual ~IGraphicEngine() = default;
    // --------------------------
    // Tipi di supporto integrati
    // --------------------------

    struct Quad {
        Vec2 pos;
        Vec2 size;
        Color color;
    };
    struct EntityVertex {
        Vec2 pos;
        Color color;
    };
    // --------------------------
    // Gestione finestra / frame
    // --------------------------
    virtual void clear(const Color& color = Color(0, 0, 0)) = 0;
    virtual void create_window(std::string title, int width, int height) = 0;
    virtual void display() = 0;
    virtual void poll_event() = 0;
    [[nodiscard]] virtual bool is_open() const = 0;

    // --------------------------
    // Primitive di disegno
    // --------------------------
    virtual void draw_circle(const Circle& circle, const Color& color) = 0;
    virtual void draw_rectangle(const Rect& rect, const Color& color) = 0;
    virtual void draw_line(const Vec2& from, const Vec2& to, const Color& color, float thickness = 1.f) = 0;
    virtual void draw_point(const Vec2& point, const Color& color) = 0;
    virtual void draw_polygon(const std::vector<Vec2>& vertices, const Color& color) = 0;

    //  --------------------------
    // Draw quad buffer
    //  --------------------------
    virtual void draw_quads(const std::vector<Quad>& quads) = 0;
    // --------------------------

    ///----------------
    /// Draw entity triangles buffer
    ///----------------
    virtual void draw_triangles(const std::vector<EntityVertex>& vertices) = 0;
    // Testo
    // --------------------------
    virtual void draw_text(const std::string& text, const Vec2& pos, int size,
                           const Color& color = Color(255, 255, 255), const std::string& fontName = "") = 0;

    // --------------------------
    // ImGui integration
    // --------------------------
    /// @brief Initialise Dear ImGui (call once after window creation).
    virtual void imgui_init() = 0;
    /// @brief Start a new ImGui frame.  @param dt_seconds elapsed time since last frame.
    virtual void imgui_new_frame(float dt_seconds) = 0;
    /// @brief Render ImGui draw commands into the window (call before present/display).
    virtual void imgui_render() = 0;
};
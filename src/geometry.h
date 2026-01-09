#pragma once

#include <variant>

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
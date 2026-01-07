#pragma once

#include <variant> 
#include "color.h"
#include "geometry.h"

enum class RenderShape {
    Circle,
    Segment,
    Square,
    Triangle
};

struct PopVisualData {
    float energy;
    float age;
};

struct RenderState {
    RenderShape shape;
    Vec2 position;
    float size;
    Color color;

    std::variant<std::monostate, PopVisualData> payload;
};

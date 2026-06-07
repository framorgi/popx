#pragma once

#include "color.h"
#include "geometry.h"

#include <variant>

enum class RenderShape { Circle, Segment, Square, Triangle };

struct PopVisualData {
    float energy;
    float age;
};
struct CellVisualData {
    double temperature;
    double elevation;
    double humidity;
    bool water;
    double glucose;
    double feromones_a;
    double feromones_b;
    double feromones_c;
    double feromones_d;
};

struct RenderState {
    RenderShape shape;
    Vec2 position;
    float size;
    Color color;

    std::variant<CellVisualData, PopVisualData> payload;
    bool dirty;
};

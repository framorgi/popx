#pragma once

struct Color {
    int r = 0, g = 0, b = 0, a = 255;
    Color() = default;
    Color(int red, int green, int blue, int alpha = 255) : r(red), g(green), b(blue), a(alpha) {}
};

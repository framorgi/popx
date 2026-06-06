
#pragma once
#include <map>

constexpr int MapSize = 160; // Number of cells along one side of the square map

constexpr unsigned MaxAge = 1000;
using PositionT = struct {
    int x;
    int y;
};

constexpr unsigned WaterThreshold = 100;
constexpr unsigned MaxH2o = 1000;
constexpr unsigned MaxC6h12o6 = 50;
constexpr unsigned MaxLipids = 300;
constexpr unsigned MaxO2 = 1000;
constexpr unsigned MaxCo2 = 1000;
constexpr unsigned MaxN2 = 2000;
constexpr unsigned MaxCaco3 = 500;
constexpr unsigned MaxFeromones = 100;

using OrganicsT = struct {
    unsigned c6h12o6;
    unsigned lipids; // can be transformed into energy and reduce temperature exchange factor [0.01 - 0.5]
    unsigned o2;
    unsigned co2;
    unsigned h2o;
    unsigned n2;
    unsigned caco3;
};

using FeromoneT = enum { FOOD_FEROMONE, DANGER_FEROMONE, MATE_FEROMONE, HOME_FEROMONE };
using FeromoneMapT = std::map<FeromoneT, int>;

constexpr double FeromoneDecayRate = 0.99; // Decay rate per update

enum class Direction { N, NE, E, SE, S, SW, W, NW };

constexpr double MaxClimbableSlope = 3.0; // Maximum elevation difference that can be climbed in one move

#pragma once

using Position = struct {
    int x;
    int y;
};

using Organics = struct {
    unsigned c6h12o6;
    unsigned lipids; // can be transformed into energy and reduce temperature exchange factor [0.01 - 0.5]
    unsigned o2;
    unsigned co2;
    unsigned h2o;
    unsigned n2;
    unsigned caco3;
};

using Feromone_t = enum { FOOD_FEROMONE, DANGER_FEROMONE, MATE_FEROMONE, HOME_FEROMONE };

enum class Direction { N, NE, E, SE, S, SW, W, NW };
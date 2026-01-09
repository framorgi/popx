
#pragma once
#include <map>
const int MAX_ENTITIES = 40;
using Position = struct {
    int x;
    int y;
};

constexpr unsigned water_threshold = 100;
constexpr unsigned max_h2o = 1000;
constexpr unsigned max_c6h12o6 = 500;
constexpr unsigned max_lipids = 300;
constexpr unsigned max_o2 = 1000;
constexpr unsigned max_co2 = 1000;
constexpr unsigned max_n2 = 2000;
constexpr unsigned max_caco3 = 500;
constexpr unsigned max_feromones = 100;

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
using FeromoneMap = std::map<Feromone_t, int>;

enum class Direction { N, NE, E, SE, S, SW, W, NW };
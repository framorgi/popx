#pragma once
#include "color.h"
#include "common.h"
#include "i_brain.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

///--------------------------------------------------------------------------
/// @brief Read-only snapshot of a Pop's state used by the GUI layer.
///        Copied from the live Pop so the GUI never touches simulation data
///        while it is rendering.
///--------------------------------------------------------------------------
struct PopSnapshot {
    std::string pop_id = {};
    uint32_t age = 0;
    float energy = 0.f;
    PositionT pos = {0, 0};
    unsigned mitochondrions = 0;
    unsigned chloroplasts = 0;
    unsigned sensitiveness = 0;
    unsigned adipose_stock_max = 0;
    double body_temperature = 0.0;
    unsigned glucose = 0;
    unsigned water = 0;
    unsigned o2 = 0;
    unsigned co2 = 0;
    unsigned calcium = 0;
    unsigned lipids = 0;
    float learning_score = 0.0f;
    std::size_t total_connections = 0;
    std::size_t useful_connections = 0;
    std::vector<BrainConnectionActivity> top_active_connections;
    Color genetic_color = {128, 128, 128, 255};
    int offspring = 0;
    bool is_photosynthetic = false;
};

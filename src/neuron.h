

#pragma once

enum class Sensor {
    // ── 0-7: position / movement ──────────────────────────────────────────
    LOC_X,           // 0  distance from left edge
    LOC_Y,           // 1  distance from bottom
    BOUNDARY_DIST_X, // 2  X distance to nearest world edge
    BOUNDARY_DIST,   // 3  distance to nearest world edge
    BOUNDARY_DIST_Y, // 4  Y distance to nearest world edge
    GENETIC_SIM_FWD, // 5  genetic similarity of entity ahead
    LAST_MOVE_DIR_X, // 6  last X displacement
    LAST_MOVE_DIR_Y, // 7  last Y displacement

    // ── 8-11: population density (directional scan) ───────────────────────
    POPULATION_DENSITY_N, // 8
    POPULATION_DENSITY_W, // 9
    POPULATION_DENSITY_E, // 10
    POPULATION_DENSITY_S, // 11

    // ── 12-19: temperature ────────────────────────────────────────────────
    TEMP_AVG_N, // 12
    TEMP_AVG_W, // 13
    TEMP_AVG_E, // 14
    TEMP_AVG_S, // 15
    TEMP_DRV_N, // 16  temperature gradient N
    TEMP_DRV_W, // 17
    TEMP_DRV_E, // 18
    TEMP_DRV_S, // 19

    // ── 20-27: feromones ──────────────────────────────────────────────────
    SENSE_SIGNAL_FOOD,   // 20  FOOD feromone at current cell
    SENSE_SIGNAL_DANGER, // 21  DANGER feromone at current cell
    SENSE_SIGNAL_MATE,   // 22  MATE feromone at current cell
    SENSE_SIGNAL_HOME,   // 23  HOME feromone at current cell
    SENSE_SIGNAL_DRV_N,  // 24  FOOD feromone gradient N
    SENSE_SIGNAL_DRV_W,  // 25
    SENSE_SIGNAL_DRV_E,  // 26
    SENSE_SIGNAL_DRV_S,  // 27

    // ── 28-31: intrinsic / environmental ──────────────────────────────────
    // Placed here so they fall within the 5-bit source_num address space
    // (source_num max = 31 with current gene encoding).
    OSC1,   // 28  oscillator ±value
    AGE,    // 29  normalised age
    TEMP,   // 30  temperature at current cell
    RANDOM, // 31  uniform random

    NUM_SENSES, // 32  ← brain input layer size (brain_size_s = 32)

    // ── Extended sensors: beyond 5-bit address space (gene source_num ≤ 31)
    // Computed by sense() but unreachable by current gene encoding.
    // Reserve for future 6-bit num field expansion.
    GLUCOSE_DENSITY_N, // 33
    GLUCOSE_DENSITY_W, // 34
    GLUCOSE_DENSITY_E, // 35
    GLUCOSE_DENSITY_S, // 36
};

enum class Action {
    // ── 0-7: movement ─────────────────────────────────────────────────────
    MOVE_FORWARD, // 0   continue last direction
    MOVE_LEFT,    // 1   rotate 90° CCW and move
    MOVE_RIGHT,   // 2   rotate 90° CW and move
    MOVE_RANDOM,  // 3   random adjacent cell
    MOVE_EAST,    // 4
    MOVE_WEST,    // 5
    MOVE_NORTH,   // 6
    MOVE_SOUTH,   // 7

    // ── 8-11: feromone signalling ─────────────────────────────────────────
    EMIT_SIGNAL_FOOD,   // 8   emit FOOD feromone
    EMIT_SIGNAL_DANGER, // 9   emit DANGER feromone
    EMIT_SIGNAL_MATE,   // 10  emit MATE feromone
    EMIT_SIGNAL_HOME,   // 11  emit HOME feromone

    // ── 12-14: resource acquisition ──────────────────────────────────────
    GET_GLUCOSE, // 12  absorb 2 units glucose from current cell
    GET_H2O,     // 13  absorb 2 units water from current cell
    GET_CALCIUM, // 14  absorb 2 units calcium from current cell

    // ── 15-16: internal modulation ────────────────────────────────────────
    SET_OSCILLATOR_PERIOD, // 15  scale osc_period_ × 1.1
    SET_RESPONSIVENESS,    // 16  increase responsiveness_ by 0.1

    NUM_ACTIONS,  // 17  ← brain output layer size (brain_size_y = 17)
    KILL_FORWARD, // 18  lethal: kill entity in the forward cell
};

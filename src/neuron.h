

#pragma once

enum class Sensor {
    // ── 0-7: position / movement ──────────────────────────────────────────
    LOC_X,           // 0  distance from left edge
    LOC_Y,           // 1  normalized coordinate from the top edge (y increases south)
    BOUNDARY_DIST_X, // 2  X distance to nearest world edge
    BOUNDARY_DIST,   // 3  distance to nearest world edge
    BOUNDARY_DIST_Y, // 4  Y distance to nearest world edge
    GENETIC_SIM_FWD, // 5: birth-genome similarity of the adjacent living Pop ahead; empty/outside = 0.
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
    SENSE_SIGNAL_DENSITY_FOOD,   // 24  mean FOOD concentration around the Pop
    SENSE_SIGNAL_DENSITY_DANGER, // 25  mean DANGER concentration
    SENSE_SIGNAL_DENSITY_MATE,   // 26  mean MATE concentration
    SENSE_SIGNAL_DENSITY_HOME,   // 27  mean HOME concentration

    // ── 28-31: intrinsic / environmental ──────────────────────────────────
    // Keep historical indices stable when expanding the input domain.
    OSC1,   // 28  oscillator ±value
    AGE,    // 29  normalised age
    TEMP,   // 30  temperature at current cell
    RANDOM, // 31  uniform random

    LAST_MOVE_FAILED = 32, // 1 after a failed movement attempt, 0 after a successful one or at birth.

    // ── Resource sensors: configured input width must include these slots.
    GLUCOSE_DENSITY_N = 33,
    GLUCOSE_DENSITY_W, // 34
    GLUCOSE_DENSITY_E, // 35
    GLUCOSE_DENSITY_S, // 36
    SENSE_DANGER_DRV_N = 37,
    SENSE_DANGER_DRV_W,
    SENSE_DANGER_DRV_E,
    SENSE_DANGER_DRV_S,
    SENSE_MATE_DRV_N,
    SENSE_MATE_DRV_W,
    SENSE_MATE_DRV_E,
    SENSE_MATE_DRV_S,
    SENSE_HOME_DRV_N,
    SENSE_HOME_DRV_W,
    SENSE_HOME_DRV_E,
    SENSE_HOME_DRV_S,
    MOVE_OPEN_N, // 49: exact destination passability at current movement stride.
    MOVE_OPEN_NE,
    MOVE_OPEN_E,
    MOVE_OPEN_SE,
    MOVE_OPEN_S,
    MOVE_OPEN_SW,
    MOVE_OPEN_W,
    MOVE_OPEN_NW,
    SENSOR_COUNT, // 57 input slots; sentinel is not a sensor.
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

    // ── 12-17: resource acquisition ──────────────────────────────────────
    GET_GLUCOSE,   // 12  absorb 2 units glucose from current cell
    LEAVE_GLUCOSE, // 13  release 2 units glucose to current cell
    GET_H2O,       // 14  absorb 2 units water from current cell
    LEAVE_H2O,     // 15  release 2 units water to current cell
    GET_CALCIUM,   // 16  absorb 2 units calcium from current cell
    LEAVE_CALCIUM, // 17  release 2 units calcium to current cell

    // ── 18-20: internal modulation ────────────────────────────────────────
    BURN_CALORIES,         // 18  burn energy to produce extra heat
    SET_OSCILLATOR_PERIOD, // 19  scale osc_period_ × 1.1
    SET_RESPONSIVENESS,    // 20  increase movement responsiveness and effective sensing radius

    DO_NOTHING = 21,   // Explicit wait and fallback when outputs do not select a unique action.
    KILL_FORWARD = 22, // Lethal: kill entity in the forward cell.
    NUM_ACTIONS,       // 23 output slots; brain_size_y may limit the enabled prefix.
};

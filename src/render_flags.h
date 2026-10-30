#pragma once

enum class ResourceOverlay {
    None = 0,
    Terrain,
    Temperature,
    Glucose,
    Water,
    Oxygen,
    CarbonDioxide,
    Lipids,
    Nitrogen,
    CalciumCarbonate,
    Population
};

///--------------------------------------------------------------------------
/// @brief Flags controlling which render layers are visible.
///        Shared between the GUI layer and the Renderer.
///--------------------------------------------------------------------------
struct RenderFlags {
    bool operator==(const RenderFlags&) const = default;
    bool show_entities = true;
    bool show_glucose = false;
    bool show_temperature = false;
    bool show_feromone_food = false;
    bool show_feromone_danger = false;
    bool show_feromone_mate = false;
    bool show_feromone_home = false;

    // Active map overlay selected from GUI combobox.
    ResourceOverlay active_overlay = ResourceOverlay::None;

    // Per-resource filters (configured via GUI checkboxes).
    bool filter_temperature = true;
    bool filter_glucose = true;
    bool filter_water = true;
    bool filter_oxygen = true;
    bool filter_co2 = true;
    bool filter_lipids = true;
    bool filter_nitrogen = true;
    bool filter_caco3 = true;
};

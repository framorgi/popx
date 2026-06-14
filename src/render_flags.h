#pragma once

enum class ResourceOverlay {
    None = 0,
    Temperature,
    Glucose,
    Water,
    Oxygen,
    CarbonDioxide,
    Lipids,
    Nitrogen,
    CalciumCarbonate
};

///--------------------------------------------------------------------------
/// @brief Flags controlling which render layers are visible.
///        Shared between the GUI layer and the Renderer.
///--------------------------------------------------------------------------
struct RenderFlags {
    bool show_entities = true;
    bool show_glucose = true;
    bool show_temperature = false;
    bool show_feromone_food = true;
    bool show_feromone_danger = true;
    bool show_feromone_mate = true;
    bool show_feromone_home = true;

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

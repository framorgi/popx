#pragma once
#include <deque>
#include <nlohmann/json.hpp>
#include <string>

/// @brief Commands queued by GUI; the application dispatches the same contract as UDP.
struct SimControl {
    std::deque<nlohmann::json> commands;
    nlohmann::json status;
    std::string error, selected_id;
    nlohmann::json inspection;
    int cell_x = 0, cell_y = 0;
    std::string brush_field = "glucose";
    double brush_value = 10;
    bool brush_enabled = false;
    bool follow = false, open_monitor = false;
    int panel_request = -1;
    void request(const std::string& command, nlohmann::json arguments = nlohmann::json::object()) {
        commands.push_back({{"command", command}, {"arguments", std::move(arguments)}});
    }
};

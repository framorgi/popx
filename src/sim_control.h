#pragma once
#include <deque>
#include <nlohmann/json.hpp>
#include <string>

/// @brief Commands queued by GUI; the application dispatches the same contract as UDP.
struct SimControl {
    std::deque<nlohmann::json> commands;
    nlohmann::json status;
    std::string error;
    void request(const std::string& command, nlohmann::json arguments = nlohmann::json::object()) {
        commands.push_back({{"command", command}, {"arguments", std::move(arguments)}});
    }
};

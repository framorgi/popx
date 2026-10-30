#pragma once
#include "color.h"

#include <cmath>
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>

/// @brief Shared frontend tokens and atomically persisted, simulation-independent preferences.
class DesignSystem {
  public:
    inline static nlohmann::json tokens, settings;
    inline static std::filesystem::path settings_path;
    static void load(const std::filesystem::path& config) {
        auto root = std::filesystem::absolute(config).parent_path().parent_path();
        auto path = root / "dashboard/static/design.json";
        if (!std::filesystem::exists(path))
            path = std::filesystem::path(__FILE__).parent_path().parent_path() / "dashboard/static/design.json";
        std::ifstream input(path);
        input >> tokens;
        settings = tokens.at("defaults");
        settings_path = std::filesystem::absolute(config).parent_path() / "appearance.json";
        if (std::filesystem::exists(settings_path)) {
            try {
                nlohmann::json saved;
                std::ifstream(settings_path) >> saved;
                validate(saved);
                settings.update(saved);
            } catch (...) {} // A corrupt preference file never prevents running a simulation.
        }
    }
    static void validate(const nlohmann::json& patch) {
        if (!patch.is_object())
            throw std::invalid_argument("Appearance must be an object");
        for (const auto& [key, value] : patch.items()) {
            if (key == "theme") {
                if (!value.is_string() || !tokens.at("themes").contains(value.get<std::string>()))
                    throw std::invalid_argument("Unknown theme");
            } else if (key == "grid" || key == "labels") {
                if (!value.is_boolean())
                    throw std::invalid_argument("Boolean expected");
            } else if (key == "agent_scale" || key == "ui_scale") {
                if (!value.is_number() || !std::isfinite(value.get<double>()) || value < .5 || value > 1.5)
                    throw std::invalid_argument("Scale must be 0.5..1.5");
            } else
                throw std::invalid_argument("Unknown appearance setting");
        }
    }
    static void apply(const nlohmann::json& patch) {
        validate(patch);
        auto candidate = settings;
        candidate.update(patch);
        auto temporary = settings_path.string() + ".tmp";
        std::ofstream output(temporary);
        output << candidate.dump(2);
        output.close();
        if (!output)
            throw std::runtime_error("Cannot save appearance");
        std::filesystem::rename(temporary, settings_path);
        settings = std::move(candidate);
    }
    static std::string neuron_label(unsigned layer, unsigned index, unsigned last, unsigned inputs) {
        if (layer == 0) {
            if (index == 32 && inputs < 57)
                return "Ingresso riservato (sempre zero)";
            if (inputs < 57 && index >= 24 && index <= 27) {
                const char* directions[] = {"N", "W", "E", "S"};
                return std::string("Grad. FOOD ") + directions[index - 24] + " (precedente)";
            }
            const auto key = std::to_string(index);
            if (tokens.at("sensors").contains(key))
                return tokens.at("sensors").at(key);
            return "Ingresso " + key + " (non implementato)";
        }
        if (layer == last) {
            const auto key = std::to_string(index);
            if (tokens.at("actions").contains(key))
                return tokens.at("actions").at(key);
            return "Uscita " + key + " (non implementata)";
        }
        return "Interno " + std::to_string(layer) + ":" + std::to_string(index);
    }
    static Color hex(const std::string& value) {
        unsigned rgb = std::stoul(value.substr(1), nullptr, 16);
        return Color((rgb >> 16) & 255, (rgb >> 8) & 255, rgb & 255);
    }
    static Color color(const std::string& key) {
        if (tokens.at("signals").contains(key))
            return hex(tokens.at("signals").at(key));
        return hex(tokens.at("themes").at(settings.at("theme").get<std::string>()).at(key));
    }
};

#include "imgui_layer.h"

#include "neuron.h"
#include "stats.h"

#include <imgui-SFML.h>
#include <imgui.h>

#include <cfloat>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <string>
#include <unordered_map>
#include <unordered_set>

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
namespace {

void draw_badge(const char* id, const char* label, const ImVec4& color) {
    ImGui::PushStyleColor(ImGuiCol_Button, color);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, color);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, color);
    ImGui::Button(label, ImVec2(170.f, 0.f));
    ImGui::PopStyleColor(3);
    (void)id;
}

/// Format a duration given in microseconds as a human-readable string.
std::string fmt_us(int64_t us) {
    char buf[32];
    if (us < 1000)
        std::snprintf(buf, sizeof(buf), "%lld µs", static_cast<long long>(us));
    else
        std::snprintf(buf, sizeof(buf), "%.2f ms", us / 1000.0);
    return buf;
}

/// Convert a StatsTimePoint to a local-time string like "2026-06-13 14:05:22".
std::string fmt_time(StatsTimePoint tp) {
    auto t = std::chrono::system_clock::now() +
             std::chrono::duration_cast<std::chrono::system_clock::duration>(tp - StatsClock::now());
    std::time_t tt = std::chrono::system_clock::to_time_t(t);
    std::tm* ltm = std::localtime(&tt);
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", ltm);
    return buf;
}

const char* sensor_name(unsigned idx) {
    switch (static_cast<Sensor>(idx)) {
        case Sensor::LOC_X:
            return "LOC_X";
        case Sensor::LOC_Y:
            return "LOC_Y";
        case Sensor::BOUNDARY_DIST_X:
            return "BOUNDARY_DIST_X";
        case Sensor::BOUNDARY_DIST:
            return "BOUNDARY_DIST";
        case Sensor::BOUNDARY_DIST_Y:
            return "BOUNDARY_DIST_Y";
        case Sensor::GENETIC_SIM_FWD:
            return "GENETIC_SIM_FWD";
        case Sensor::LAST_MOVE_DIR_X:
            return "LAST_MOVE_DIR_X";
        case Sensor::LAST_MOVE_DIR_Y:
            return "LAST_MOVE_DIR_Y";
        case Sensor::POPULATION_DENSITY_N:
            return "POP_DENSITY_N";
        case Sensor::POPULATION_DENSITY_W:
            return "POP_DENSITY_W";
        case Sensor::POPULATION_DENSITY_E:
            return "POP_DENSITY_E";
        case Sensor::POPULATION_DENSITY_S:
            return "POP_DENSITY_S";
        case Sensor::TEMP_AVG_N:
            return "TEMP_AVG_N";
        case Sensor::TEMP_AVG_W:
            return "TEMP_AVG_W";
        case Sensor::TEMP_AVG_E:
            return "TEMP_AVG_E";
        case Sensor::TEMP_AVG_S:
            return "TEMP_AVG_S";
        case Sensor::TEMP_DRV_N:
            return "TEMP_DRV_N";
        case Sensor::TEMP_DRV_W:
            return "TEMP_DRV_W";
        case Sensor::TEMP_DRV_E:
            return "TEMP_DRV_E";
        case Sensor::TEMP_DRV_S:
            return "TEMP_DRV_S";
        case Sensor::SENSE_SIGNAL_FOOD:
            return "SIGNAL_FOOD";
        case Sensor::SENSE_SIGNAL_DANGER:
            return "SIGNAL_DANGER";
        case Sensor::SENSE_SIGNAL_MATE:
            return "SIGNAL_MATE";
        case Sensor::SENSE_SIGNAL_HOME:
            return "SIGNAL_HOME";
        case Sensor::SENSE_SIGNAL_DRV_N:
            return "SIGNAL_DRV_N";
        case Sensor::SENSE_SIGNAL_DRV_W:
            return "SIGNAL_DRV_W";
        case Sensor::SENSE_SIGNAL_DRV_E:
            return "SIGNAL_DRV_E";
        case Sensor::SENSE_SIGNAL_DRV_S:
            return "SIGNAL_DRV_S";
        case Sensor::OSC1:
            return "OSC1";
        case Sensor::AGE:
            return "AGE";
        case Sensor::TEMP:
            return "TEMP";
        case Sensor::RANDOM:
            return "RANDOM";
        default:
            return "SENSOR";
    }
}

const char* action_name(unsigned idx) {
    switch (static_cast<Action>(idx)) {
        case Action::MOVE_FORWARD:
            return "MOVE_FORWARD";
        case Action::MOVE_LEFT:
            return "MOVE_LEFT";
        case Action::MOVE_RIGHT:
            return "MOVE_RIGHT";
        case Action::MOVE_RANDOM:
            return "MOVE_RANDOM";
        case Action::MOVE_EAST:
            return "MOVE_EAST";
        case Action::MOVE_WEST:
            return "MOVE_WEST";
        case Action::MOVE_NORTH:
            return "MOVE_NORTH";
        case Action::MOVE_SOUTH:
            return "MOVE_SOUTH";
        case Action::EMIT_SIGNAL_FOOD:
            return "EMIT_SIGNAL_FOOD";
        case Action::EMIT_SIGNAL_DANGER:
            return "EMIT_SIGNAL_DANGER";
        case Action::EMIT_SIGNAL_MATE:
            return "EMIT_SIGNAL_MATE";
        case Action::EMIT_SIGNAL_HOME:
            return "EMIT_SIGNAL_HOME";
        case Action::GET_GLUCOSE:
            return "GET_GLUCOSE";
        case Action::LEAVE_GLUCOSE:
            return "LEAVE_GLUCOSE";
        case Action::GET_H2O:
            return "GET_H2O";
        case Action::LEAVE_H2O:
            return "LEAVE_H2O";
        case Action::GET_CALCIUM:
            return "GET_CALCIUM";
        case Action::LEAVE_CALCIUM:
            return "LEAVE_CALCIUM";
        case Action::BURN_CALORIES:
            return "BURN_CALORIES";
        case Action::SET_OSCILLATOR_PERIOD:
            return "SET_OSC_PERIOD";
        case Action::SET_RESPONSIVENESS:
            return "SET_RESPONSIVENESS";
        case Action::KILL_FORWARD:
            return "KILL_FORWARD";
        default:
            return "ACTION";
    }
}

std::string node_label(unsigned layer, unsigned neuron, unsigned output_layer) {
    if (layer == 0) {
        return std::string(sensor_name(neuron));
    }
    if (layer == output_layer) {
        return std::string(action_name(neuron));
    }
    return "H" + std::to_string(layer) + "N" + std::to_string(neuron);
}

} // namespace

// ---------------------------------------------------------------------------
// Construction / lifecycle
// ---------------------------------------------------------------------------

ImGuiLayer::ImGuiLayer(IGraphicEngine& gfx, IStats& stats, IAgentsManager& agents, SimControl& sim_control,
                       RenderFlags& render_flags)
    : gfx_(gfx), stats_(stats), agents_(agents), sim_control_(sim_control), render_flags_(render_flags) {}

void ImGuiLayer::init() {
    // ImGui::SFML::Init was already called by the app via gfx_.imgui_init().
    // Nothing else to set up here for now.
}

void ImGuiLayer::update(float dt) {
    gfx_.imgui_new_frame(dt);
    build_control_panel();
    if (sim_control_.show_stats_overlay)
        build_stats_overlay();
    if (show_pop_inspector_)
        build_pop_inspector();
    if (show_charts_)
        build_charts();
    if (end_of_sim_)
        build_end_modal();
}

void ImGuiLayer::render() {
    gfx_.imgui_render();
}

void ImGuiLayer::shutdown() {}

void ImGuiLayer::notify_end_of_simulation() {
    end_of_sim_ = true;
}

// ---------------------------------------------------------------------------
// Main Control Panel
// ---------------------------------------------------------------------------
void ImGuiLayer::build_control_panel() {
    ImGui::SetNextWindowSize(ImVec2(340, 0), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowPos(ImVec2(8, 8), ImGuiCond_FirstUseEver);
    ImGui::Begin("Simulation Control");

    // --- Simulation flow ---
    if (sim_control_.paused) {
        if (ImGui::Button("  Resume  "))
            sim_control_.paused = false;
    } else {
        if (ImGui::Button("  Pause   "))
            sim_control_.paused = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("  Stop    "))
        sim_control_.stop_requested = true;

    if (ImGui::Button("Restart From Zero")) {
        sim_control_.restart_requested = true;
        sim_control_.stop_requested = false;
        sim_control_.terminate_requested = false;
        end_of_sim_ = false;
    }
    ImGui::SameLine();
    if (ImGui::Button("NewGen")) {
        sim_control_.newgen_requested = true;
    }
    ImGui::Text("Generation: %u", agents_.get_generation_count());

    ImGui::Separator();

    // --- Render layers ---
    ImGui::Text("Render layers");
    ImGui::Checkbox("Entities", &render_flags_.show_entities);
    ImGui::Checkbox("Temperature map", &render_flags_.show_temperature);
    ImGui::Separator();

    const char* overlay_items[] = {"None", "Temperature", "Glucose",  "Water",           "Oxygen",
                                   "CO2",  "Lipids",      "Nitrogen", "CalciumCarbonate"};
    int overlay_idx = static_cast<int>(render_flags_.active_overlay);
    if (ImGui::Combo("Map Resource Overlay", &overlay_idx, overlay_items,
                     static_cast<int>(sizeof(overlay_items) / sizeof(overlay_items[0])))) {
        render_flags_.active_overlay = static_cast<ResourceOverlay>(overlay_idx);
    }

    ImGui::Text("Resource filters");
    ImGui::Checkbox("Temperature", &render_flags_.filter_temperature);
    ImGui::Checkbox("Glucose", &render_flags_.filter_glucose);
    ImGui::Checkbox("Water", &render_flags_.filter_water);
    ImGui::Checkbox("Oxygen", &render_flags_.filter_oxygen);
    ImGui::Checkbox("CO2", &render_flags_.filter_co2);
    ImGui::Checkbox("Lipids", &render_flags_.filter_lipids);
    ImGui::Checkbox("Nitrogen", &render_flags_.filter_nitrogen);
    ImGui::Checkbox("CaCO3", &render_flags_.filter_caco3);

    ImGui::Separator();
    ImGui::Text("Feromones");
    ImGui::Checkbox("Food (red)", &render_flags_.show_feromone_food);
    ImGui::Checkbox("Danger (teal)", &render_flags_.show_feromone_danger);
    ImGui::Checkbox("Mate (green)", &render_flags_.show_feromone_mate);
    ImGui::Checkbox("Home (blue)", &render_flags_.show_feromone_home);
    ImGui::Separator();

    // --- Stats ---
    ImGui::Checkbox("Stats overlay", &sim_control_.show_stats_overlay);
    if (ImGui::Button("Save stats JSON"))
        sim_control_.save_stats_requested = true;

    int rate_window = static_cast<int>(stats_.get_rate_window_samples());
    if (ImGui::SliderInt("Rate window (samples)", &rate_window, 2, 120)) {
        stats_.set_rate_window_samples(static_cast<std::size_t>(rate_window));
    }

    if (sim_control_.stats_saved_recently) {
        ImGui::TextWrapped("Last save: %s", sim_control_.last_saved_json_path.c_str());
    } else if (!sim_control_.last_save_error.empty()) {
        ImGui::TextColored(ImVec4(1.f, 0.35f, 0.35f, 1.f), "Save error: %s", sim_control_.last_save_error.c_str());
    }

    if (sim_control_.last_save_status == SimControl::SaveStatus::Success) {
        draw_badge("##save_ok", "SAVE SUCCESS", ImVec4(0.10f, 0.65f, 0.20f, 1.0f));
        if (!sim_control_.last_save_timestamp.empty()) {
            ImGui::Text("Timestamp: %s", sim_control_.last_save_timestamp.c_str());
        }
    } else if (sim_control_.last_save_status == SimControl::SaveStatus::Failed) {
        draw_badge("##save_fail", "SAVE FAILED", ImVec4(0.80f, 0.15f, 0.15f, 1.0f));
        if (!sim_control_.last_save_timestamp.empty()) {
            ImGui::Text("Timestamp: %s", sim_control_.last_save_timestamp.c_str());
        }
    }

    if (agents_.get_forced_respawn_active()) {
        draw_badge("##forced_respawn", "FORCED RESPAWN ACTIVE", ImVec4(0.90f, 0.55f, 0.10f, 1.0f));
        ImGui::Text("Alive %d / Min %d", agents_.get_alive_count(), agents_.get_forced_respawn_min_alive());
    }
    ImGui::Separator();

    // --- Sub-windows ---
    ImGui::Text("Analysis windows");
    if (ImGui::Button("Pop Inspector")) {
        show_pop_inspector_ = !show_pop_inspector_;
        if (show_pop_inspector_)
            snap_dirty_ = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("Charts"))
        show_charts_ = !show_charts_;

    ImGui::End();
}

// ---------------------------------------------------------------------------
// Stats Overlay (top-right corner, translucent, no decoration)
// ---------------------------------------------------------------------------
void ImGuiLayer::build_stats_overlay() {
    const auto& history = stats_.get_history();

    // Choose viewport size for corner anchoring
    ImGuiIO& io = ImGui::GetIO();
    ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x - 8.f, 8.f), ImGuiCond_Always, ImVec2(1.f, 0.f));
    ImGui::SetNextWindowBgAlpha(0.75f);
    ImGui::SetNextWindowSize(ImVec2(230, 0), ImGuiCond_Always);

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoNav |
                             ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
                             ImGuiWindowFlags_NoBringToFrontOnFocus;

    ImGui::Begin("##overlay", nullptr, flags);
    ImGui::Text("Gen:    %u", agents_.get_generation_count());
    ImGui::Text("Alive:  %d", agents_.get_alive_count());
    ImGui::Text("Dead:   %d", agents_.get_dead_count());

    if (!history.empty()) {
        const auto& r = history.back();
        ImGui::Text("Tick:   %llu", static_cast<unsigned long long>(r.tick));
        ImGui::Text("FPS:    %.1f", r.fps);
        ImGui::Separator();
        ImGui::Separator();
        ImGui::Text("Tick dur: %s", fmt_us(r.tick_duration_us).c_str());
        ImGui::Text("Frame dur:%s", fmt_us(r.frame_duration_us).c_str());
    }
    ImGui::End();
}

// ---------------------------------------------------------------------------
// Pop Inspector
// ---------------------------------------------------------------------------
void ImGuiLayer::build_pop_inspector() {
    ImGui::SetNextWindowSize(ImVec2(430, 560), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowPos(ImVec2(296, 8), ImGuiCond_FirstUseEver);

    if (!ImGui::Begin("Pop Inspector", &show_pop_inspector_)) {
        ImGui::End();
        return;
    }

    // Refresh snapshot if dirty (e.g. opened, or user navigated)
    if (snap_dirty_) {
        std::string prev_selected_id;
        if (pop_idx_ >= 0 && pop_idx_ < static_cast<int>(snap_cache_.size())) {
            prev_selected_id = snap_cache_[pop_idx_].pop_id;
        } else if (!selected_pop_id_.empty()) {
            prev_selected_id = selected_pop_id_;
        }

        snap_cache_ = agents_.get_pops_snapshot();
        snap_dirty_ = false;
        if (!snap_cache_.empty()) {
            if (!prev_selected_id.empty()) {
                int remapped_idx = -1;
                for (int i = 0; i < static_cast<int>(snap_cache_.size()); ++i) {
                    if (snap_cache_[i].pop_id == prev_selected_id) {
                        remapped_idx = i;
                        break;
                    }
                }
                pop_idx_ = remapped_idx >= 0 ? remapped_idx : 0;
            } else if (pop_idx_ >= static_cast<int>(snap_cache_.size()) || pop_idx_ < 0) {
                pop_idx_ = 0;
            }
            selected_pop_id_ = snap_cache_[pop_idx_].pop_id;
        } else {
            pop_idx_ = 0;
            selected_pop_id_.clear();
        }
    }

    if (snap_cache_.empty()) {
        ImGui::Text("No alive pops.");
        ImGui::End();
        return;
    }

    // Navigation
    ImGui::Text("Pop %d / %d", pop_idx_ + 1, static_cast<int>(snap_cache_.size()));
    ImGui::SameLine();
    if (ImGui::ArrowButton("##prev", ImGuiDir_Left)) {
        if (pop_idx_ > 0)
            --pop_idx_;
        if (pop_idx_ >= 0 && pop_idx_ < static_cast<int>(snap_cache_.size()))
            selected_pop_id_ = snap_cache_[pop_idx_].pop_id;
        snap_dirty_ = true;
    }
    ImGui::SameLine();
    if (ImGui::ArrowButton("##next", ImGuiDir_Right)) {
        if (pop_idx_ < static_cast<int>(snap_cache_.size()) - 1)
            ++pop_idx_;
        if (pop_idx_ >= 0 && pop_idx_ < static_cast<int>(snap_cache_.size()))
            selected_pop_id_ = snap_cache_[pop_idx_].pop_id;
        snap_dirty_ = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("Refresh"))
        snap_dirty_ = true;

    ImGui::Separator();

    const PopSnapshot& s = snap_cache_[pop_idx_];
    selected_pop_id_ = s.pop_id;

    // Identity
    ImGui::Text("ID:          %s", s.pop_id.c_str());
    ImGui::Text("Position:    (%d, %d)", s.pos.x, s.pos.y);
    ImGui::Text("Type:        %s", s.is_photosynthetic ? "Photosynthetic" : "Heterotroph");

    ImGui::Separator();

    // Life stats
    ImGui::Text("Age:         %u", s.age);
    ImGui::Text("Energy:");
    ImGui::SameLine();
    ImGui::ProgressBar(s.energy / 400.f, ImVec2(-1.f, 0.f));

    ImGui::Separator();

    // Physical traits
    ImGui::Text("Mitochondria: %u", s.mitochondrions);
    ImGui::Text("Chloroplasts: %u", s.chloroplasts);
    ImGui::Text("Sensitiveness:%u", s.sensitiveness);
    ImGui::Text("Lipid cap:    %u", s.adipose_stock_max);

    ImGui::Separator();

    // Physiology state
    ImGui::Text("Body Temp:    %.2f", s.body_temperature);
    ImGui::Text("Glucose:      %u", s.glucose);
    ImGui::Text("Water:        %u", s.water);
    ImGui::Text("O2:           %u", s.o2);
    ImGui::Text("CO2:          %u", s.co2);
    ImGui::Text("Calcium:      %u", s.calcium);
    ImGui::Text("Lipids:       %u", s.lipids);

    ImGui::Separator();

    ImGui::Text("Offspring:   %d", s.offspring);
    ImGui::Text("Learning score (last reward): %.4f", s.learning_score);
    ImGui::Text("Brain connections: %zu", s.total_connections);
    ImGui::Text("Useful connections (eps=0.05): %zu", s.useful_connections);
    ImGui::Separator();
    ImGui::Text("Lifetime movement loss:      %.4f", s.total_movement_energy_loss);
    ImGui::Text("Lifetime metabolism loss:    %.4f", s.total_metabolism_energy_loss);
    ImGui::Text("Lifetime reproduction loss:  %.4f", s.total_reproduction_energy_loss);
    ImGui::Text("Lifetime respiration gain:   %.4f", s.total_respiration_energy_gain);
    ImGui::Text("Lifetime thermoreg loss:     %.4f", s.total_thermoregolation_energy_loss);

    // Genetic colour marker (circle for heterotroph, triangle for photosynthetic)
    ImGui::Text("Genetic marker:");
    ImGui::SameLine();
    ImVec4 col4(s.genetic_color.r / 255.f, s.genetic_color.g / 255.f, s.genetic_color.b / 255.f, 1.f);
    const ImVec2 marker_size(36.f, 24.f);
    ImGui::InvisibleButton("##gencolshape", marker_size);
    const ImVec2 pmin = ImGui::GetItemRectMin();
    const ImVec2 pmax = ImGui::GetItemRectMax();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImU32 col = ImGui::ColorConvertFloat4ToU32(col4);
    if (s.is_photosynthetic) {
        const ImVec2 top((pmin.x + pmax.x) * 0.5f, pmin.y + 2.f);
        const ImVec2 left(pmin.x + 3.f, pmax.y - 3.f);
        const ImVec2 right(pmax.x - 3.f, pmax.y - 3.f);
        dl->AddTriangleFilled(top, left, right, col);
    } else {
        const ImVec2 c((pmin.x + pmax.x) * 0.5f, (pmin.y + pmax.y) * 0.5f);
        const float radius = std::min(marker_size.x, marker_size.y) * 0.40f;
        dl->AddCircleFilled(c, radius, col, 24);
    }

    ImGui::Separator();
    ImGui::Text("Useful active paths (input -> hidden -> output)");
    if (s.top_active_connections.empty()) {
        ImGui::TextDisabled("No active paths above threshold in latest activation.");
    } else {
        struct NodeKey {
            unsigned layer;
            unsigned neuron;
            bool operator==(const NodeKey& other) const {
                return layer == other.layer && neuron == other.neuron;
            }
        };
        struct NodeKeyHash {
            std::size_t operator()(const NodeKey& k) const {
                return (static_cast<std::size_t>(k.layer) << 32U) ^ static_cast<std::size_t>(k.neuron);
            }
        };

        unsigned output_layer = 0;
        for (const auto& e : s.top_active_connections) {
            output_layer = std::max(output_layer, e.to_layer);
        }

        std::unordered_map<NodeKey, BrainConnectionActivity, NodeKeyHash> best_incoming;
        for (const auto& e : s.top_active_connections) {
            const NodeKey key{e.to_layer, e.to_neuron};
            auto it = best_incoming.find(key);
            if (it == best_incoming.end() || e.activity_score > it->second.activity_score) {
                best_incoming[key] = e;
            }
        }

        std::vector<std::pair<std::string, float>> path_rows;
        std::unordered_set<std::string> seen_paths;
        for (const auto& edge_to_out : s.top_active_connections) {
            if (edge_to_out.to_layer != output_layer) {
                continue;
            }
            std::vector<std::string> rev_nodes;
            rev_nodes.push_back(node_label(edge_to_out.to_layer, edge_to_out.to_neuron, output_layer));

            unsigned cur_layer = edge_to_out.from_layer;
            unsigned cur_neuron = edge_to_out.from_neuron;
            int guard = 0;
            bool reached_input = false;
            while (guard++ < 16) {
                rev_nodes.push_back(node_label(cur_layer, cur_neuron, output_layer));
                if (cur_layer == 0) {
                    reached_input = true;
                    break;
                }
                const NodeKey k{cur_layer, cur_neuron};
                auto it = best_incoming.find(k);
                if (it == best_incoming.end()) {
                    break;
                }
                cur_layer = it->second.from_layer;
                cur_neuron = it->second.from_neuron;
            }

            if (!reached_input) {
                continue;
            }

            std::reverse(rev_nodes.begin(), rev_nodes.end());
            std::string path;
            for (std::size_t i = 0; i < rev_nodes.size(); ++i) {
                if (i > 0) {
                    path += " -> ";
                }
                path += rev_nodes[i];
            }
            if (seen_paths.insert(path).second) {
                path_rows.emplace_back(path, edge_to_out.activity_score);
            }
        }

        std::sort(path_rows.begin(), path_rows.end(), [](const auto& a, const auto& b) { return a.second > b.second; });
        if (path_rows.size() > 10) {
            path_rows.resize(10);
        }

        if (path_rows.empty()) {
            ImGui::TextDisabled("No complete input->...->output active path above threshold.");
        } else {
            for (std::size_t i = 0; i < path_rows.size(); ++i) {
                ImGui::Text("%zu) %s | score=%.4f", i + 1, path_rows[i].first.c_str(), path_rows[i].second);
            }
        }
    }

    ImGui::End();
}

// ---------------------------------------------------------------------------
// Charts
// ---------------------------------------------------------------------------
void ImGuiLayer::build_charts() {
    ImGui::SetNextWindowSize(ImVec2(620, 720), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowPos(ImVec2(8, 380), ImGuiCond_FirstUseEver);

    if (!ImGui::Begin("Charts", &show_charts_)) {
        ImGui::End();
        return;
    }

    const auto& history = stats_.get_history();
    if (history.empty()) {
        ImGui::Text("No data yet.");
        ImGui::End();
        return;
    }

    // Build float arrays from history (capped for performance)
    constexpr int kMaxPlot = 512;
    const int n = static_cast<int>(history.size());
    const int step = (n <= kMaxPlot) ? 1 : n / kMaxPlot;
    const int count = (n + step - 1) / step;

    static std::vector<float> alive_arr, dead_arr, glucose_arr, o2_arr, co2_arr, h2o_arr, lipids_arr, n2_arr, caco3_arr,
        births_arr, deaths_arr, brate_arr, drate_arr;
    alive_arr.resize(count);
    dead_arr.resize(count);
    glucose_arr.resize(count);
    o2_arr.resize(count);
    co2_arr.resize(count);
    h2o_arr.resize(count);
    lipids_arr.resize(count);
    n2_arr.resize(count);
    caco3_arr.resize(count);
    births_arr.resize(count);
    deaths_arr.resize(count);
    brate_arr.resize(count);
    drate_arr.resize(count);

    for (int i = 0; i < count; ++i) {
        const auto& r = history[static_cast<std::size_t>(i * step)];
        alive_arr[i] = static_cast<float>(r.alive);
        dead_arr[i] = static_cast<float>(r.dead);
        glucose_arr[i] = static_cast<float>(r.world_organics.c6h12o6);
        o2_arr[i] = static_cast<float>(r.world_organics.o2);
        co2_arr[i] = static_cast<float>(r.world_organics.co2);
        h2o_arr[i] = static_cast<float>(r.world_organics.h2o);
        lipids_arr[i] = static_cast<float>(r.world_organics.lipids);
        n2_arr[i] = static_cast<float>(r.world_organics.n2);
        caco3_arr[i] = static_cast<float>(r.world_organics.caco3);
        births_arr[i] = static_cast<float>(r.total_births);
        deaths_arr[i] = static_cast<float>(r.total_deaths);
        brate_arr[i] = static_cast<float>(r.birth_rate_per_sec);
        drate_arr[i] = static_cast<float>(r.death_rate_per_sec);
    }

    ImGui::Text("Alive pop over time");
    ImGui::PlotLines("##alive", alive_arr.data(), count, 0, nullptr, 0.f, FLT_MAX, ImVec2(-1.f, 60.f));

    ImGui::Text("Current dead");
    ImGui::PlotLines("##dead", dead_arr.data(), count, 0, nullptr, 0.f, FLT_MAX, ImVec2(-1.f, 60.f));

    ImGui::Text("World glucose total");
    ImGui::PlotLines("##glucose", glucose_arr.data(), count, 0, nullptr, 0.f, FLT_MAX, ImVec2(-1.f, 60.f));

    ImGui::Text("World O2 total");
    ImGui::PlotLines("##o2", o2_arr.data(), count, 0, nullptr, 0.f, FLT_MAX, ImVec2(-1.f, 60.f));

    ImGui::Text("World CO2 total");
    ImGui::PlotLines("##co2", co2_arr.data(), count, 0, nullptr, 0.f, FLT_MAX, ImVec2(-1.f, 60.f));

    ImGui::Text("World H2O total");
    ImGui::PlotLines("##h2o", h2o_arr.data(), count, 0, nullptr, 0.f, FLT_MAX, ImVec2(-1.f, 60.f));

    ImGui::Text("World Lipids total");
    ImGui::PlotLines("##lipids", lipids_arr.data(), count, 0, nullptr, 0.f, FLT_MAX, ImVec2(-1.f, 60.f));

    ImGui::Text("World N2 total");
    ImGui::PlotLines("##n2", n2_arr.data(), count, 0, nullptr, 0.f, FLT_MAX, ImVec2(-1.f, 60.f));

    ImGui::Text("World CaCO3 total");
    ImGui::PlotLines("##caco3", caco3_arr.data(), count, 0, nullptr, 0.f, FLT_MAX, ImVec2(-1.f, 60.f));

    ImGui::Text("Total births");
    ImGui::PlotLines("##births", births_arr.data(), count, 0, nullptr, 0.f, FLT_MAX, ImVec2(-1.f, 60.f));

    ImGui::Text("Total deaths");
    ImGui::PlotLines("##deaths", deaths_arr.data(), count, 0, nullptr, 0.f, FLT_MAX, ImVec2(-1.f, 60.f));

    ImGui::Text("Birth rate (per sec)");
    ImGui::PlotLines("##brate", brate_arr.data(), count, 0, nullptr, 0.f, FLT_MAX, ImVec2(-1.f, 60.f));

    ImGui::Text("Death rate (per sec)");
    ImGui::PlotLines("##drate", drate_arr.data(), count, 0, nullptr, 0.f, FLT_MAX, ImVec2(-1.f, 60.f));

    const auto& latest = history.back();
    ImGui::Separator();
    ImGui::Text("Current generation: %u", latest.generation);
    ImGui::Text("Last window delta births=%d deaths=%d", latest.births_window, latest.deaths_window);
    ImGui::Text("Last rates birth=%.4f death=%.4f", latest.birth_rate_per_sec, latest.death_rate_per_sec);

    ImGui::End();
}

// ---------------------------------------------------------------------------
// End-of-simulation modal
// ---------------------------------------------------------------------------
void ImGuiLayer::build_end_modal() {
    if (!ImGui::IsPopupOpen("Simulation Ended"))
        ImGui::OpenPopup("Simulation Ended");

    ImGui::SetNextWindowSize(ImVec2(420, 0), ImGuiCond_Always);
    if (ImGui::BeginPopupModal("Simulation Ended", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextColored(ImVec4(1, 0.8f, 0.2f, 1), "Simulation has ended.");
        ImGui::Separator();

        SessionSummary s = stats_.get_summary();
        ImGui::Text("Start:          %s", fmt_time(s.start_time).c_str());
        ImGui::Text("End:            %s", fmt_time(s.end_time).c_str());
        ImGui::Text("Total ticks:    %llu", static_cast<unsigned long long>(s.total_ticks));
        ImGui::Text("Generation:     %u", s.current_generation);
        ImGui::Text("Peak alive:     %d", s.peak_alive);
        ImGui::Text("Current dead:   %d", s.total_dead);
        ImGui::Text("Total births:   %d", s.total_births);
        ImGui::Text("Cumulative deaths: %d", s.total_deaths);
        ImGui::Text("Avg tick dur:   %s", fmt_us(s.avg_tick_us).c_str());
        ImGui::Text("Avg frame dur:  %s", fmt_us(s.avg_frame_us).c_str());
        ImGui::Text("Avg FPS:        %.1f", s.avg_fps);
        ImGui::Text("Avg birth rate: %.3f /sec", s.avg_birth_rate_per_sec);
        ImGui::Text("Avg death rate: %.3f /sec", s.avg_death_rate_per_sec);
        ImGui::Text("Avg movement loss (active): %.4f", s.avg_movement_energy_loss_active);
        ImGui::Text("Avg metabolism loss (active): %.4f", s.avg_metabolism_energy_loss_active);
        ImGui::Text("Avg reproduction loss (active): %.4f", s.avg_reproduction_energy_loss_active);
        ImGui::Text("Avg respiration gain (active): %.4f", s.avg_respiration_energy_gain_active);
        ImGui::Text("Avg thermoreg loss (active): %.4f", s.avg_thermoregolation_energy_loss_active);

        ImGui::Separator();
        ImGui::Text("Last organics:");
        ImGui::Text("  Glucose %u  |  O2 %u  |  CO2 %u", s.last_organics.c6h12o6, s.last_organics.o2,
                    s.last_organics.co2);
        ImGui::Text("  H2O %u  |  Lipids %u  |  N2 %u  |  CaCO3 %u", s.last_organics.h2o, s.last_organics.lipids,
                    s.last_organics.n2, s.last_organics.caco3);

        if (sim_control_.stats_saved_recently) {
            ImGui::Separator();
            ImGui::TextColored(ImVec4(0.4f, 1.f, 0.4f, 1.f), "Stats saved to: %s",
                               sim_control_.last_saved_json_path.c_str());
        }

        ImGui::Separator();
        if (ImGui::Button("Close Window")) {
            sim_control_.terminate_requested = true;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

#include "imgui_layer.h"

#include <imgui.h>

#include <algorithm>

ImGuiLayer::ImGuiLayer(IGraphicEngine& gfx, Stats& stats, IAgentsManager& agents, SimControl& controls,
                       RenderFlags& flags)
    : gfx_(gfx), stats_(stats), agents_(agents), controls_(controls), flags_(flags) {}
void ImGuiLayer::add_widget(std::string title, std::function<void()> draw) {
    widgets_.emplace_back(std::move(title), std::move(draw));
}
void ImGuiLayer::init() {
    auto& style = ImGui::GetStyle();
    ImGui::StyleColorsDark();
    style.WindowPadding = {24, 20};
    style.FramePadding = {12, 8};
    style.ItemSpacing = {10, 10};
    style.FrameRounding = 6;
    style.ChildRounding = 8;
    style.WindowRounding = 0;
    style.Colors[ImGuiCol_WindowBg] = {0.055f, 0.075f, 0.11f, 1};
    style.Colors[ImGuiCol_Button] = {0.10f, 0.23f, 0.28f, 1};
    style.Colors[ImGuiCol_ButtonHovered] = {0.13f, 0.39f, 0.43f, 1};
    style.Colors[ImGuiCol_Header] = {.105f, .16f, .23f, 1};
    style.Colors[ImGuiCol_HeaderHovered] = {.10f, .23f, .28f, 1};
    style.Colors[ImGuiCol_CheckMark] = {0.28f, 0.85f, 0.72f, 1};
    ImGui::GetIO().IniFilename = nullptr; // Keep the user's old layout file untouched.
    add_widget("ESECUZIONE", [this] {
        const auto& s = controls_.status;
        ImGui::Text("Stato: %s", s.value("state", std::string("starting")).c_str());
        if (!s.value("reason", std::string{}).empty())
            ImGui::Text("Conclusione: %s", s["reason"].get<std::string>().c_str());
        ImGui::Text("Tick: %llu", static_cast<unsigned long long>(s.value("tick", uint64_t(0))));
        ImGui::Text("Tempo simulato: %.1f min", s.value("simulated_seconds", 0.0) / 60);
        ImGui::Text("Agenti vivi: %d", agents_.get_alive_count());
        ImGui::Text("NewGen: %u", agents_.get_generation_count());
        const auto latest = stats_.performance();
        ImGui::Text("Tick medio: %.0f us", latest.value("avg_tick_us", 0.0));
        ImGui::Text("Frame medi: %.1f FPS", latest.value("fps", 0.0));
        ImGui::Text("CPU processo: %.2f s", latest.value("process_cpu_seconds", 0.0));
        auto transport = stats_.transport_status();
        ImGui::Text("Dashboard: %s", transport.value("connected", false) ? "connessa" : "non disponibile");
        ImGui::Text("Messaggi persi: %llu",
                    static_cast<unsigned long long>(transport.value("dropped_messages", uint64_t(0))));
    });
    add_widget("CONTROLLI", [this] {
        auto button = [this](const char* label, const char* name) {
            if (ImGui::Button(label, {-1, 0}))
                controls_.request(name);
        };
        const auto state = controls_.status.value("state", std::string{});
        if (state == "finished")
            button("Nuova run", "start");
        else
            button(state == "paused" ? "Riprendi" : "Pausa", state == "paused" ? "resume" : "pause");
        if (state != "finished")
            button("Stop", "stop");
        button("Riavvia e rileggi INI", "restart");
        const auto speed = controls_.status.value("speed", 1);
        if (ImGui::Button(speed == 1 ? "Velocita: 1x  >  2x" : "Velocita: 2x  >  1x", {-1, 0}))
            controls_.request("speed", {{"value", speed == 1 ? 2 : 1}});
        if (state != "finished")
            button("Applica NewGen", "newgen");
        button("Headless - continua senza GUI", "headless");
        ImGui::TextWrapped("Riapri questa finestra dalla dashboard web.");
        button("Esci", "exit");
        if (!controls_.error.empty()) {
            ImGui::TextWrapped("%s", controls_.error.c_str());
            if (ImGui::Button("Chiudi messaggio"))
                controls_.error.clear();
        }
    });
    add_widget("MAPPA", [this] {
        ImGui::Checkbox("Agenti", &flags_.show_entities);
        ImGui::Checkbox("Temperatura terreno", &flags_.show_temperature);
        const char* layers[] = {"Nessuno", "Temperatura", "Glucosio", "Acqua", "Ossigeno",
                                "CO2",     "Lipidi",      "Azoto",    "Calcio"};
        int layer = static_cast<int>(flags_.active_overlay);
        if (ImGui::Combo("Risorsa", &layer, layers, 9))
            flags_.active_overlay = static_cast<ResourceOverlay>(layer);
        ImGui::Checkbox("Glucosio base", &flags_.show_glucose);
        ImGui::Checkbox("FOOD", &flags_.show_feromone_food);
        ImGui::SameLine();
        ImGui::Checkbox("DANGER", &flags_.show_feromone_danger);
        ImGui::Checkbox("MATE", &flags_.show_feromone_mate);
        ImGui::SameLine();
        ImGui::Checkbox("HOME", &flags_.show_feromone_home);
    });
}
void ImGuiLayer::update(float dt) {
    gfx_.imgui_new_frame(dt);
    const auto size = ImGui::GetIO().DisplaySize;
    const float side = std::min(size.y, std::max(1.f, size.x - 320.f));
    ImGui::SetNextWindowPos({side, 0});
    ImGui::SetNextWindowSize({size.x - side, size.y});
    ImGui::Begin("PopX", nullptr,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings);
    ImGui::TextColored({.28f, .85f, .72f, 1}, "POPX / SIMULATION LAB");
    ImGui::TextDisabled("Eredita / ecologia / adattamento");
    ImGui::Separator();
    for (auto& [title, draw] : widgets_)
        if (ImGui::CollapsingHeader(title.c_str(), ImGuiTreeNodeFlags_DefaultOpen))
            draw();
    ImGui::End();
}
void ImGuiLayer::render() {
    gfx_.imgui_render();
}

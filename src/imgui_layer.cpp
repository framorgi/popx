#include "imgui_layer.h"

#include "config.h"
#include "design_system.h"
#include "sfml_graphic_engine.h"

#include <imgui.h>

#include <algorithm>
#include <cmath>

namespace {
ImVec4 rgba(Color c, float alpha = 1) {
    return {c.r / 255.f, c.g / 255.f, c.b / 255.f, alpha};
}
bool slider(const char* label, float* value, float low, float high, const char* format = "%.3f") {
    ImGui::TextUnformatted(label);
    ImGui::PushID(label);
    ImGui::SetNextItemWidth(-1);
    bool changed = ImGui::SliderFloat("##value", value, low, high, format);
    ImGui::PopID();
    return changed;
}
ImU32 ink(const std::string& key, float alpha = 1) {
    return ImGui::ColorConvertFloat4ToU32(rgba(DesignSystem::color(key), alpha));
}
void json_rows(const nlohmann::json& value) {
    if (!value.is_object())
        return;
    for (const auto& [key, item] : value.items()) {
        if (key == "brain") {
            ImGui::TextUnformatted("Rete neurale: apri Brain Monitor per nodi, attivazioni e pesi.");
        } else if (key == "sensors" && item.is_array() && ImGui::TreeNode("Sensori")) {
            for (unsigned i = 0; i < item.size(); ++i)
                ImGui::Text("%s: %.5f", DesignSystem::neuron_label(0, i, 1, item.size()).c_str(),
                            item[i].get<float>());
            ImGui::TreePop();
        } else if (key == "action_requests" && item.is_array() && ImGui::TreeNode("Azioni richieste")) {
            for (unsigned i = 0; i < item.size(); ++i)
                ImGui::Text("%s: %llu", DesignSystem::neuron_label(1, i, 1, 57).c_str(),
                            static_cast<unsigned long long>(item[i].get<uint64_t>()));
            ImGui::TreePop();
        } else if ((key == "last_action" || key == "last_executed_action") && item.is_number_integer()) {
            const int action = item.get<int>();
            ImGui::TextWrapped("%s: %s", key == "last_executed_action" ? "Ultima azione eseguita" : "Ultima azione scelta",
                               action >= 0 ? DesignSystem::neuron_label(1, action, 1, 57).c_str() : "nessuna");
        } else if (item.is_primitive())
            ImGui::TextWrapped("%s: %s", key.c_str(), item.dump().c_str());
        else if (ImGui::TreeNode(key.c_str())) {
            if (item.is_object())
                json_rows(item);
            else
                ImGui::TextWrapped("%s", item.dump().c_str());
            ImGui::TreePop();
        }
    }
}
} // namespace
ImGuiLayer::ImGuiLayer(IGraphicEngine& gfx, Stats& stats, IAgentsManager& agents, SimControl& controls,
                       RenderFlags& flags)
    : gfx_(gfx), stats_(stats), agents_(agents), controls_(controls), flags_(flags) {}
void ImGuiLayer::add_widget(std::string title, std::function<void()> draw) {
    widgets_.emplace_back(std::move(title), std::move(draw));
}
void ImGuiLayer::init() {
    ImGui::GetIO().IniFilename = nullptr;
}

void ImGuiLayer::draw_world() {
    auto& engine = dynamic_cast<SfmlGraphicEngine&>(gfx_);
    ImGui::TextWrapped("Rotellina: zoom  |  trascina destro: pan\nClic: seleziona  |  Home: ricentra\nF1/F2/F3: "
                       "pannelli  |  F4: rete  |  F: segui");
    if (ImGui::Button("Ricentra mappa"))
        engine.reset_camera();
    ImGui::SameLine();
    ImGui::Text("%.1fx", engine.zoom());
    const char* layers[] = {"Elevazione", "Terreno", "Temperatura", "Glucosio", "Acqua", "Ossigeno",
                            "CO2", "Lipidi", "Azoto", "Calcio", "Densita Pop (5x5)"};
    int layer = int(flags_.active_overlay);
    if (ImGui::Combo("Layer", &layer, layers, 11)) {
        flags_.active_overlay = ResourceOverlay(layer);
        flags_.show_temperature = false;
    }
    ImGui::TextWrapped(layer == 2    ? "Scala fissa: blu <= 0 C, oro 30 C, corallo >= 60 C."
                       : layer == 10 ? "Intensita = celle occupate da Pop / celle nella finestra 5x5."
                       : layer >= 3  ? "Risorse: quadrato centrale, intensita = quantita / capacita (0..100%%)."
                                     : "Elevazione: scala neutra. Terreno: colori naturali per quota.");
    ImGui::Checkbox("Agenti", &flags_.show_entities);
    ImGui::Checkbox("FOOD", &flags_.show_feromone_food);
    ImGui::SameLine();
    ImGui::Checkbox("DANGER", &flags_.show_feromone_danger);
    ImGui::Checkbox("MATE", &flags_.show_feromone_mate);
    ImGui::SameLine();
    ImGui::Checkbox("HOME", &flags_.show_feromone_home);
    ImGui::TextWrapped("Triangolo: cloroplasti > mitocondri. Cerchio: altri Pop. Diamante nero: Source. "
                       "Ottagono rosso: Reaper. Colore Pop: firma genetica.");
    if (ImGui::CollapsingHeader("Aspetto e grafica", ImGuiTreeNodeFlags_DefaultOpen)) {
        auto settings = DesignSystem::settings;
        if (settings != last_appearance_) {
            appearance_draft_ = settings;
            last_appearance_ = settings;
        }
        int preset = settings["theme"] == "slate" ? 1 : 0;
        if (ImGui::Combo("Tema", &preset, "Lagoon\0Slate\0"))
            controls_.request("appearance", {{"theme", preset ? "slate" : "lagoon"}});
        float scale = appearance_draft_.value("agent_scale", .85f);
        slider("Dimensione Pop", &scale, .5f, 1.5f);
        appearance_draft_["agent_scale"] = scale;
        if (ImGui::IsItemDeactivatedAfterEdit())
            controls_.request("appearance", {{"agent_scale", scale}});
        bool grid = settings.value("grid", false);
        if (ImGui::Checkbox("Griglia (zoom >= 4x)", &grid))
            controls_.request("appearance", {{"grid", grid}});
        bool labels = settings.value("labels", true);
        if (ImGui::Checkbox("Etichette del monitor", &labels))
            controls_.request("appearance", {{"labels", labels}});
        float ui_scale = appearance_draft_.value("ui_scale", 1.f);
        slider("Scala UI", &ui_scale, .75f, 1.25f);
        appearance_draft_["ui_scale"] = ui_scale;
        if (ImGui::IsItemDeactivatedAfterEdit())
            controls_.request("appearance", {{"ui_scale", ui_scale}});
        ImGui::TextWrapped("Preferenze condivise con la dashboard e salvate in config/appearance.json.");
    }
}

void ImGuiLayer::draw_inspector() {
    const auto& p = controls_.inspection;
    if (p.is_null()) {
        ImGui::TextWrapped("Seleziona un Pop sulla mappa. Un agente morto viene deselezionato automaticamente.");
        return;
    }
    ImGui::TextWrapped("%s", controls_.selected_id.c_str());
    ImGui::Text("Campione tick %llu (max 4 Hz)", static_cast<unsigned long long>(p.value("tick", uint64_t(0))));
    const int executed = p.value("last_executed_action", -1);
    ImGui::Text("Ultima azione eseguita: %s",
                executed >= 0 ? DesignSystem::neuron_label(1, executed, 1, 57).c_str() : "nessuna");
    ImGui::Checkbox("Segui agente", &controls_.follow);
    auto avatar = ImGui::GetCursorScreenPos();
    avatar.x += 60;
    avatar.y += 48;
    auto* painter = ImGui::GetWindowDrawList();
    const auto rgb = p.at("genetics").at("color");
    auto color = ImGui::ColorConvertFloat4ToU32(rgba(Color(rgb[0], rgb[1], rgb[2])));
    if (p.at("traits").at("chloroplasts").get<int>() > p.at("traits").at("mitochondria").get<int>())
        painter->AddTriangleFilled({avatar.x, avatar.y - 20}, {avatar.x + 20, avatar.y + 20},
                                   {avatar.x - 20, avatar.y + 20}, color);
    else
        painter->AddCircleFilled(avatar, 20, color, 11);
    painter->AddCircle(avatar, 40, ink("line"), 24);
    painter->AddText({avatar.x - 4, avatar.y - 48}, ink("muted"), "N");
    painter->AddText({avatar.x + 43, avatar.y - 7}, ink("muted"), "E");
    const auto direction = p.at("direction");
    float dx = direction[0], dy = direction[1], length = std::hypot(dx, dy);
    if (length > 0)
        painter->AddLine(avatar, {avatar.x + 36 * dx / length, avatar.y + 36 * dy / length}, ink("accent"), 2);
    ImGui::Dummy({130, 100});
    ImGui::TextWrapped("Avatar: firma genetica. Vettore: ultima direzione; nord in alto.");
    const auto& total = p.at("totals");
    ImGui::Text("Eta: %u tick  |  Figli: %u", p.at("age").get<unsigned>(), total.at("offspring").get<unsigned>());
    ImGui::Text("Posizione: %d, %d", p["position"][0].get<int>(), p["position"][1].get<int>());
    ImGui::Text("Energia: %.2f  |  Temperatura: %.2f C", total.at("energy").get<double>(),
                total.at("body_temperature").get<double>());
    ImGui::Text("Distanza: %.2f celle", total.at("distance").get<double>());
    const float energy = std::clamp(total.at("energy").get<float>() / MaxEnergy, 0.f, 1.f);
    ImGui::ProgressBar(energy, {-1, 0}, "Energia / massimo");
    if (ImGui::Button("Apri Brain Monitor", {-1, 0}))
        monitor_open_ = true;
    if (ImGui::CollapsingHeader("Genoma ereditato", ImGuiTreeNodeFlags_DefaultOpen)) {
        const auto& genes = p.at("genes");
        ImGui::Text("%zu geni | colore = firma genetica", genes.size());
        const float width = std::max(80.f, ImGui::GetContentRegionAvail().x);
        const int columns = std::max(1, int(width / 9));
        auto start = ImGui::GetCursorScreenPos();
        auto* draw = ImGui::GetWindowDrawList();
        for (size_t i = 0; i < genes.size(); ++i) {
            const auto& gene = genes[i];
            float x = start.x + (i % columns) * 9, y = start.y + (i / columns) * 16;
            const int weight = gene[4];
            draw->AddRectFilled({x, y}, {x + 7, y + 13},
                                ink(weight > 0   ? "accent"
                                    : weight < 0 ? "red"
                                                 : "muted",
                                    .25f + .75f * std::min(1.f, std::abs(weight) / 32768.f)),
                                2);
            if (ImGui::IsMouseHoveringRect({x, y}, {x + 8, y + 15})) {
                ImGui::SetTooltip("Gene %zu: %s", i, gene.dump().c_str());
                if (ImGui::IsMouseClicked(0))
                    selected_gene_ = i;
            }
        }
        ImGui::Dummy({width, float((genes.size() + columns - 1) / columns * 16)});
        if (selected_gene_ >= 0 && size_t(selected_gene_) < genes.size())
            ImGui::TextWrapped("Gene %d [tipo, indice, tipo, indice, quanto]: %s", selected_gene_,
                               genes[selected_gene_].dump().c_str());
        ImGui::TextWrapped("Verde positivo, corallo negativo, grigio zero. Peso = quanto / 8192. I pesi appresi sono "
                           "nel Brain Monitor.");
    }
    if (ImGui::CollapsingHeader("Tratti ereditari"))
        json_rows(p.at("traits"));
    if (ImGui::CollapsingHeader("Learning e contributi"))
        json_rows(p.at("learning"));
    if (ImGui::CollapsingHeader("Stato completo / contatori"))
        json_rows(p);
}

void ImGuiLayer::draw_sandbox() {
    ImGui::TextWrapped("Interventi esterni registrati. Configurazione INI bloccata nei checkpoint; questi override "
                       "espliciti sono inclusi nei nuovi salvataggi.");
    const auto runtime = controls_.status.value("runtime", nlohmann::json::object());
    if (runtime != last_runtime_) {
        draft_ = runtime;
        last_runtime_ = runtime;
    }
    if (draft_.empty())
        return;
    if (ImGui::CollapsingHeader("Learning Mixer", ImGuiTreeNodeFlags_DefaultOpen)) {
        double sum = 0;
        for (const auto& factor : DesignSystem::tokens.at("learning"))
            sum += draft_.value("reward_" + factor.at("key").get<std::string>() + "_weight", 0.);
        for (const auto& factor : DesignSystem::tokens.at("learning")) {
            auto key = "reward_" + factor.at("key").get<std::string>() + "_weight";
            float value = draft_.value(key, 0.f);
            ImGui::PushStyleColor(ImGuiCol_SliderGrab, rgba(DesignSystem::hex(factor.at("color"))));
            slider(factor.at("label").get<std::string>().c_str(), &value, 0, 1, "%.3f");
            draft_[key] = value;
            if (ImGui::IsItemDeactivatedAfterEdit())
                controls_.request("runtime_apply", {{key, value}});
            ImGui::PopStyleColor();
            ImGui::TextDisabled("Quota effettiva %.1f%%", sum ? 100 * value / sum : 0);
        }
        if (ImGui::Button("Preset ecologico / Reset")) {
            nlohmann::json patch;
            for (const auto& spec : Config::runtime_schema())
                if (spec.at("name").get<std::string>().starts_with("reward_"))
                    patch[spec.at("name").get<std::string>()] = spec.at("default");
            controls_.request("runtime_apply", patch);
        }
        if (ImGui::Button("Solo energia + temperatura"))
            controls_.request("runtime_apply", {{"reward_age_weight", 0},
                                                {"reward_energy_weight", .3},
                                                {"reward_delta_weight", .35},
                                                {"reward_offspring_weight", 0},
                                                {"reward_temperature_weight", .2}});
        ImGui::TextWrapped(
            "Pesi normalizzati per la loro somma. Tutti zero: reward zero. Reward = somma dei contributi in [-1,1]. "
            "Segnale = reward - baseline precedente. Il cambio azzera il credito temporale.");
        const auto latest = stats_.latest();
        if (latest.contains("learning")) {
            const auto& learning = latest["learning"];
            ImGui::Text("Campione tick %llu", static_cast<unsigned long long>(latest.value("tick", uint64_t(0))));
            ImGui::Text("Reward %.5f | segnale %.5f", learning.value("reward", 0.), learning.value("signal", 0.));
            for (const auto& f : DesignSystem::tokens.at("learning")) {
                auto key = f.at("key").get<std::string>() + "_component";
                const float value = learning.value(key, 0.f);
                ImGui::PushStyleColor(ImGuiCol_PlotHistogram, rgba(DesignSystem::hex(f.at("color"))));
                ImGui::ProgressBar(std::min(1.f, std::abs(value)), {-1, 10}, "");
                ImGui::PopStyleColor();
                ImGui::Text("%s: %+.5f", f.at("label").get<std::string>().c_str(), value);
            }
            ImGui::TextWrapped("Barre: modulo dei contributi medi dei vivi nell'ultimo campione; valore firmato sotto. "
                               "Somma = reward media.");
        }
    }
    if (ImGui::CollapsingHeader("Parametri runtime")) {
        for (const auto& spec : Config::runtime_schema()) {
            const std::string key = spec.at("name");
            if (key.starts_with("reward_"))
                continue;
            ImGui::PushID(key.c_str());
            ImGui::TextWrapped("%s", key == "climate_rotation_speed" ? "Velocita rotazione clima" : key.c_str());
            ImGui::SetNextItemWidth(-1);
            bool changed;
            if (spec.at("type") == "integer") {
                int value = draft_.at(key);
                changed = ImGui::InputInt("##value", &value);
                draft_[key] = value;
            } else {
                double value = draft_.at(key);
                changed = ImGui::InputDouble("##value", &value, 0, 0, "%.6f");
                draft_[key] = value;
            }
            if (changed) {} // Edits are staged until explicit application.
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("%s", spec.at("description").get<std::string>().c_str());
            if (ImGui::Button("Applica"))
                controls_.request("runtime_apply", {{key, draft_[key]}});
            ImGui::PopID();
        }
    }
    if (ImGui::CollapsingHeader("Strumenti sulla cella", ImGuiTreeNodeFlags_DefaultOpen)) {
        static const char* labels[] = {"Glucosio", "Acqua", "Ossigeno", "CO2", "Lipidi", "Azoto", "Calcio",
                                       "Feromone FOOD", "Feromone DANGER", "Feromone MATE", "Feromone HOME",
                                       "Temperatura", "Pop", "Source"};
        static const char* keys[] = {"glucose", "water", "oxygen", "co2", "lipids", "nitrogen", "calcium",
                                     "food", "danger", "mate", "home", "temperature", "pop", "source"};
        int chosen = 0;
        for (int i = 0; i < 14; ++i)
            if (controls_.brush_field == keys[i]) chosen = i;
        if (ImGui::Combo("Elemento", &chosen, labels, 14)) controls_.brush_field = keys[chosen];
        float amount = static_cast<float>(controls_.brush_value);
        if (chosen < 12) {
            if (chosen == 11) {
                ImGui::SliderFloat("Temperatura C", &amount, -50, 100);
            } else {
                ImGui::InputFloat("Quantita per cella", &amount, 1, 10, "%.0f");
                amount = std::clamp(amount, 0.f, chosen == 5 ? float(MaxN2) : chosen >= 7 ? float(MaxFeromones)
                        : chosen == 0 ? float(MaxC6h12o6) : chosen == 4 ? float(MaxLipids)
                        : chosen == 6 ? float(MaxCaco3) : float(MaxH2o));
            }
            controls_.brush_value = amount;
        }
        ImGui::InputInt("X", &controls_.cell_x);
        ImGui::InputInt("Y", &controls_.cell_y);
        const bool paused =
            controls_.status.value("state", "") == "paused" && !controls_.status.value("step_remaining", 0u);
        ImGui::BeginDisabled(!paused);
        auto act = [&](bool erase) {
            nlohmann::json args = {{"x", controls_.cell_x}, {"y", controls_.cell_y}, {"erase", erase}};
            if (chosen >= 12) { args["kind"] = keys[chosen]; controls_.request(erase ? "sandbox_remove" : "sandbox_spawn", args); }
            else { args["field"] = keys[chosen]; args["value"] = controls_.brush_value; controls_.request("sandbox_cell", args); }
        };
        if (ImGui::Button("Aggiungi / applica")) act(false);
        ImGui::SameLine();
        if (ImGui::Button(chosen == 11 ? "Annulla ultima modifica" : "Elimina")) act(true);
        ImGui::EndDisabled();
        ImGui::TextWrapped("In pausa: sinistro dipinge, destro cancella l'elemento scelto; trascina sulla mappa. "
                           "Rotellina zoom, centrale trascina la camera. Temperatura: destro ripristina il valore precedente per cella.");
    }
}

void ImGuiLayer::draw_brain() {
    if (controls_.open_monitor) {
        monitor_open_ = true;
        controls_.open_monitor = false;
    }
    if (!monitor_open_)
        return;
    ImGui::SetNextWindowSize({1500, 800}, ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Brain Monitor", &monitor_open_)) {
        ImGui::End();
        return;
    }
    const auto& p = controls_.inspection;
    if (p.is_null()) {
        ImGui::TextUnformatted("Agente non disponibile");
        ImGui::End();
        return;
    }
    const auto& brain = p.at("brain");
    const auto tick = p.value("tick", uint64_t(0));
    const auto activations = brain.at("activations").get<std::vector<std::vector<float>>>();
    if (observed_id_ != controls_.selected_id || average_.size() != activations.size()) {
        observed_id_ = controls_.selected_id;
        average_ = activations;
        peaks_ = activations;
        edge_ = nullptr;
        observed_tick_ = tick;
    } else if (tick != observed_tick_) {
        for (size_t l = 0; l < activations.size(); ++l)
            for (size_t n = 0; n < activations[l].size(); ++n) {
                average_[l][n] = .8f * average_[l][n] + .2f * activations[l][n];
                peaks_[l][n] = std::max(peaks_[l][n], std::abs(activations[l][n]));
            }
        observed_tick_ = tick;
    }
    ImGui::Checkbox("Aggregated Activity (EMA 0.2)", &aggregate_);
    slider("|peso| minimo", &min_weight_, 0, 1, "%.4f");
    const int executed = p.value("last_executed_action", -1);
    ImGui::Text("Ultima azione eseguita: %s",
                executed >= 0 ? DesignSystem::neuron_label(1, executed, 1, 57).c_str() : "nessuna");
    ImGui::TextWrapped(
        "Live Activity: ultimo campione. Aggregata: EMA dei campioni osservati; picco dal cambio agente. Colore nodo = "
        "attivazione; linea verde/corallo = segno, spessore = |peso|. Seleziona un arco per modificarlo in pausa.");
    ImGui::BeginChild("##brain_canvas", {0, std::min(650.f, ImGui::GetContentRegionAvail().y - 110.f)}, true,
                      ImGuiWindowFlags_HorizontalScrollbar);
    auto origin = ImGui::GetCursorScreenPos();
    auto width = std::max(1150.f, ImGui::GetContentRegionAvail().x);
    const unsigned layers = brain.at("num_hidden_layers").get<unsigned>() + 2;
    std::vector<unsigned> sizes(layers, brain.at("sizeN"));
    sizes.front() = brain.at("sizeS");
    sizes.back() = brain.at("sizeY");
    const float height = std::max(690.f, 40.f + 21.f * sizes.front());
    auto point = [&](unsigned layer, unsigned index) {
        const float left = origin.x + 185.f, right = origin.x + width - 185.f;
        return ImVec2(left + layer * (right - left) / (layers - 1),
                      origin.y + 28.f + (index + .5f) * (height - 45.f) / sizes[layer]);
    };
    auto* draw = ImGui::GetWindowDrawList();
    for (unsigned l = 0; l < layers; ++l) {
        const std::string title = l == 0 ? "Input" : l == layers - 1 ? "Output" : "Hidden " + std::to_string(l);
        draw->AddText({point(l, 0).x - 15, origin.y}, ink("muted"), title.c_str());
    }
    size_t shown = 0;
    for (const auto& block : brain.at("connections"))
        for (const auto& edge : block.at("entries")) {
            float weight = edge.at("value");
            if (std::abs(weight) < min_weight_ || weight == 0)
                continue;
            if (++shown > 3000)
                continue;
            const unsigned from = block.at("from_layer"), to = block.at("to_layer");
            auto a = point(from, edge.at("col")), b = point(to, edge.at("row"));
            draw->AddLine(a, b, ink(weight > 0 ? "positive" : "negative", .45f),
                          .5f + 2 * std::min(4.f, std::abs(weight)));
            auto m = ImGui::GetMousePos();
            float dx = b.x - a.x, dy = b.y - a.y;
            float t = std::clamp(((m.x - a.x) * dx + (m.y - a.y) * dy) / (dx * dx + dy * dy), 0.f, 1.f);
            if (std::hypot(m.x - a.x - t * dx, m.y - a.y - t * dy) < 3 && ImGui::IsWindowHovered()) {
                ImGui::SetTooltip("%s -> %s | peso %.6f",
                                  DesignSystem::neuron_label(from, edge.at("col"), layers - 1, sizes.front()).c_str(),
                                  DesignSystem::neuron_label(to, edge.at("row"), layers - 1, sizes.front()).c_str(), weight);
                if (ImGui::IsMouseClicked(0)) {
                    edge_ = edge;
                    edge_["from_layer"] = from;
                    edge_["to_layer"] = to;
                }
            }
        }
    const auto& shown_activity = aggregate_ ? average_ : activations;
    for (unsigned l = 0; l < layers; ++l)
        for (unsigned n = 0; n < sizes[l]; ++n) {
            auto v = point(l, n);
            float value = l < shown_activity.size() && n < shown_activity[l].size() ? shown_activity[l][n] : 0;
            draw->AddCircleFilled(
                v, 5, ink(value >= 0 ? "positive" : "negative", .2f + .8f * std::min(1.f, std::abs(value))), 16);
            if (l == 0 || l == layers - 1) {
                const auto label = DesignSystem::neuron_label(l, n, layers - 1, sizes.front());
                draw->AddText({l == 0 ? origin.x + 8.f : v.x + 11.f, v.y - 7.f}, ink("text"), label.c_str());
            } else if (DesignSystem::settings.value("labels", true))
                draw->AddText({v.x + 8.f, v.y - 7.f}, ink("muted"), std::to_string(n).c_str());
            if (std::hypot(ImGui::GetMousePos().x - v.x, ImGui::GetMousePos().y - v.y) < 6)
                ImGui::SetTooltip("%s: %.6f | picco %.6f",
                                  DesignSystem::neuron_label(l, n, layers - 1, sizes.front()).c_str(), value,
                                  l < peaks_.size() && n < peaks_[l].size() ? peaks_[l][n] : 0);
        }
    ImGui::Dummy({width, height});
    ImGui::EndChild();
    if (shown > 3000)
        ImGui::TextWrapped("Vista limitata ai primi 3000 archi sopra soglia; aumenta il filtro.");
    if (!edge_.is_null()) {
        const unsigned from = edge_.at("from_layer"), to = edge_.at("to_layer");
        ImGui::TextWrapped("Arco selezionato: %s -> %s",
                           DesignSystem::neuron_label(from, edge_.at("col"), layers - 1, sizes.front()).c_str(),
                           DesignSystem::neuron_label(to, edge_.at("row"), layers - 1, sizes.front()).c_str());
        float value = edge_.at("value");
        if (ImGui::InputFloat("Peso appreso", &value, .01f, .1f, "%.6f"))
            edge_["value"] = value;
        ImGui::BeginDisabled(controls_.status.value("state", "") != "paused");
        if (ImGui::Button("Applica peso [-4,4]")) {
            auto args = edge_;
            args["pop_id"] = controls_.selected_id;
            controls_.request("sandbox_weight", args);
        }
        ImGui::EndDisabled();
        ImGui::TextWrapped("Modifica il peso corrente; ereditabile se abilitato. Topologia e genoma alla nascita "
                           "restano visibili separatamente.");
    }
    ImGui::End();
}

void ImGuiLayer::update(float dt) {
    gfx_.imgui_new_frame(dt);
    const auto theme = DesignSystem::settings.at("theme").get<std::string>();
    if (theme_ != theme) {
        theme_ = theme;
        ImGui::StyleColorsDark();
        auto& style = ImGui::GetStyle();
        const float gap = DesignSystem::tokens.at("spacing"), radius = DesignSystem::tokens.at("radius");
        style.WindowPadding = {gap, gap};
        style.FramePadding = {9, 6};
        style.ItemSpacing = {8, 8};
        style.FrameRounding = radius;
        style.ChildRounding = radius;
        style.Colors[ImGuiCol_WindowBg] = rgba(DesignSystem::color("panel"));
        style.Colors[ImGuiCol_Text] = rgba(DesignSystem::color("text"));
        style.Colors[ImGuiCol_TextDisabled] = rgba(DesignSystem::color("muted"));
        style.Colors[ImGuiCol_Button] = rgba(DesignSystem::color("line"));
        style.Colors[ImGuiCol_Header] = rgba(DesignSystem::color("line"));
        style.Colors[ImGuiCol_CheckMark] = rgba(DesignSystem::color("accent"));
        for (auto slot : {ImGuiCol_FrameBg, ImGuiCol_Tab, ImGuiCol_TitleBg, ImGuiCol_TitleBgActive})
            style.Colors[slot] = rgba(DesignSystem::color("line"));
        for (auto slot : {ImGuiCol_ButtonHovered, ImGuiCol_HeaderHovered, ImGuiCol_TabHovered, ImGuiCol_TabActive,
                          ImGuiCol_FrameBgHovered})
            style.Colors[slot] = rgba(DesignSystem::color("accent"), .35f);
        style.Colors[ImGuiCol_SliderGrab] = rgba(DesignSystem::color("accent"));
        style.Colors[ImGuiCol_PlotHistogram] = rgba(DesignSystem::color("accent"));
    }
    ImGui::GetIO().FontGlobalScale = DesignSystem::settings.value("ui_scale", 1.f);
    const auto size = ImGui::GetIO().DisplaySize;
    const float side = dynamic_cast<SfmlGraphicEngine&>(gfx_).sidebar_width();
    ImGui::SetNextWindowPos({size.x - side, 0});
    ImGui::SetNextWindowSize({side, size.y});
    ImGui::Begin("POPX / SANDBOX", nullptr,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings);
    ImGui::TextColored(rgba(DesignSystem::color("accent")), "POPX / EVOLUTION SANDBOX");
    const auto state = controls_.status.value("state", std::string{});
    ImGui::Text("%s  |  %d vivi",
                state == "paused"    ? "In pausa"
                : state == "running" ? "In esecuzione"
                                     : "Conclusa",
                agents_.get_alive_count());
    ImGui::Text("Tick %llu  |  %.1f min", static_cast<unsigned long long>(controls_.status.value("tick", uint64_t(0))),
                controls_.status.value("simulated_seconds", 0.) / 60);
    ImGui::Text("%.1f tick/s  |  %.1f FPS", controls_.status.value("effective_tps", 0.),
                controls_.status.value("render_fps", 0.));
    if (ImGui::Button(state == "paused" ? "Riprendi" : "Pausa"))
        controls_.request(state == "paused" ? "resume" : "pause");
    ImGui::SameLine();
    ImGui::BeginDisabled(state != "paused");
    if (ImGui::Button("+1 tick"))
        controls_.request("step", {{"ticks", 1}});
    ImGui::SameLine();
    if (ImGui::Button("+100"))
        controls_.request("step", {{"ticks", 100}});
    ImGui::EndDisabled();
    if (ImGui::Button("1x"))
        controls_.request("speed", {{"value", 1}});
    ImGui::SameLine();
    if (ImGui::Button("2x"))
        controls_.request("speed", {{"value", 2}});
    ImGui::SameLine();
    if (ImGui::Button("Turbo headless"))
        controls_.request("speed", {{"value", 0}});
    static int steps = 100;
    ImGui::SetNextItemWidth(130);
    ImGui::InputInt("Tick", &steps);
    ImGui::SameLine();
    ImGui::BeginDisabled(state != "paused");
    if (ImGui::Button("Avanza N"))
        controls_.request("step", {{"ticks", steps}});
    ImGui::EndDisabled();
    controls_.brush_enabled = false;
    if (ImGui::BeginTabBar("panels")) {
        if (ImGui::BeginTabItem("Mondo", nullptr, controls_.panel_request == 0 ? ImGuiTabItemFlags_SetSelected : 0)) {
            draw_world();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Pop", nullptr, controls_.panel_request == 1 ? ImGuiTabItemFlags_SetSelected : 0)) {
            draw_inspector();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Sandbox", nullptr, controls_.panel_request == 2 ? ImGuiTabItemFlags_SetSelected : 0)) {
            controls_.brush_enabled = true;
            draw_sandbox();
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
        controls_.panel_request = -1;
    }
    ImGui::Separator();
    if (ImGui::CollapsingHeader("Sessione e processo")) {
        if (ImGui::Button("Applica NewGen"))
            ImGui::OpenPopup("Conferma NewGen");
        if (ImGui::BeginPopupModal("Conferma NewGen", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::TextUnformatted("Seleziona, abbatte e redistribuisce i superstiti.");
            if (ImGui::Button("Applica")) {
                controls_.request("newgen");
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("Annulla"))
                ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
        }
        if (ImGui::Button("Headless - continua senza finestra"))
            controls_.request("headless");
        if (ImGui::Button("Stop - conclude la run"))
            ImGui::OpenPopup("Conferma Stop");
        if (ImGui::BeginPopupModal("Conferma Stop", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::TextUnformatted("Conclude la run. Il processo resta aperto.");
            if (ImGui::Button("Concludi")) {
                controls_.request("stop");
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("Annulla"))
                ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
        }
        if (ImGui::Button("Chiudi simulatore"))
            ImGui::OpenPopup("Conferma uscita");
        if (ImGui::BeginPopupModal("Conferma uscita", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::TextUnformatted("Conclude la run e termina PopX.\nIl launcher chiude anche il backend dashboard.");
            if (ImGui::Button("Termina processo")) {
                controls_.request("exit");
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("Annulla"))
                ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
        }
        if (state == "finished" && !controls_.status.value("config_locked", false) && ImGui::Button("Nuova run da INI"))
            controls_.request("start");
    }
    if (!controls_.error.empty()) {
        ImGui::TextWrapped("%s", controls_.error.c_str());
        if (ImGui::Button("Chiudi messaggio"))
            controls_.error.clear();
    }
    for (auto& [title, draw] : widgets_)
        if (ImGui::CollapsingHeader(title.c_str()))
            draw();
    ImGui::End();
    draw_brain();
}
void ImGuiLayer::render() {
    gfx_.imgui_render();
}

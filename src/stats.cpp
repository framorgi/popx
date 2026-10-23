#include "stats.h"

#include "pops_manager.h"

#include <algorithm>
#include <cmath>
#include <random>

using Json = nlohmann::json;
namespace {
Json empty_totals() {
    return {{"learning_updates", 0.0},
            {"learning_abs_weight_change", 0.0},
            {"learning_abs_plasticity", 0.0},
            {"energy", 0.0},
            {"distance", 0.0},
            {"offspring", 0.0},
            {"movement_attempts", 0.0},
            {"movement_successes", 0.0},
            {"movement_blocked_occupied", 0.0},
            {"movement_blocked_calcium", 0.0},
            {"movement_blocked_slope", 0.0},
            {"movement_blocked_boundary", 0.0},
            {"movement_no_displacement", 0.0},
            {"movement_blocked_other", 0.0},
            {"respirations", 0.0},
            {"photosynthesis", 0.0},
            {"respiration_gain", 0.0},
            {"respiration_heat", 0.0},
            {"metabolism_cost", 0.0},
            {"movement_cost", 0.0},
            {"other_action_cost", 0.0},
            {"pending_action_cost", 0.0},
            {"reproduction_cost", 0.0},
            {"thermoregulation_cost", 0.0},
            {"energy_cap", 0.0},
            {"roundoff", 0.0},
            {"body_temperature", 0.0},
            {"abs_temp_delta", 0.0}};
}
Json resources(const OrganicsT& o) {
    return Json::array({o.c6h12o6, o.h2o, o.caco3, o.co2, o.o2, o.lipids});
}
void add(Json& a, const Json& b) {
    for (auto it = b.begin(); it != b.end(); ++it) {
        if (it.value().is_number())
            a[it.key()] = a.value(it.key(), 0.0) + it.value().get<double>();
    }
}
} // namespace
Stats::Stats(std::unique_ptr<ICommunicator> communicator) : communicator_(std::move(communicator)) {}
void Stats::record_session(SessionRecord session) {
    auto metadata = std::move(session.metadata);
    const auto stamp = std::chrono::system_clock::now().time_since_epoch().count();
    run_id_ = std::to_string(stamp) + "-" + std::to_string(std::random_device{}());
    tick_ = samples_ = frames_ = measured_ticks_ = 0;
    tick_us_ = frame_us_ = frame_interval_us_ = 0;
    latest_ = Json::object();
    closed_ = empty_totals();
    initial_ = Json::object();
    sample_sums_ = empty_totals();
    peak_alive_ = completed_age_ = 0;
    peak_distance_ = correlation_sum_ = 0;
    closed_["resources"] = Json::array({0, 0, 0, 0, 0, 0});
    closed_["actions"] = std::vector<uint64_t>(23, 0);
    closed_["acquired"] = Json::array({0, 0, 0, 0, 0, 0});
    closed_["released"] = Json::array({0, 0, 0, 0, 0, 0});
    counts_ = {{"founders", 0}, {"natural_births", 0}, {"forced_births", 0}, {"inherited_recovery_births", 0},
               {"deaths", 0}, {"culled", 0}};
    rate_history_.clear();
    previous_distances_.clear();
    current_distances_.clear();
    calcium_window_ticks_ = 0;
    recovery_resources_.fill(0);
    recovery_energy_ = 0;
    started_ = std::chrono::steady_clock::now();
    cpu_started_ = std::clock();
    metadata_ = std::move(metadata);
    publish("session", metadata_);
}
void Stats::publish(const std::string& type, Json data) {
    if (communicator_)
        communicator_->send({{"type", type}, {"run_id", run_id_}, {"tick", tick_}, {"data", std::move(data)}});
}
void Stats::record_event(AgentEvent event) {
    const std::string type(event_name(event.kind));
    auto data = std::move(event.agent);
    if (type == "founder" || type == "birth") {
        const std::string key = type == "founder"             ? "founders"
                                : data.value("forced", false) ? "forced_births"
                                                              : "natural_births";
        counts_[key] = counts_[key].get<uint64_t>() + 1;
        if (type == "birth" && data.value("recovery_from_archive", false))
            counts_["inherited_recovery_births"] = counts_.value("inherited_recovery_births", uint64_t(0)) + 1;
        // Recovery endowments are external inputs even when the seed has a known genetic donor.
        if (type == "birth" && data.value("forced", false) &&
            (data.value("recovery_seed", false) || data.value("parent_id", std::string()).empty())) {
            for (std::size_t i = 0; i < recovery_resources_.size(); ++i)
                recovery_resources_[i] += data["resources"][i].get<int64_t>();
            recovery_energy_ += data["totals"]["energy"].get<double>();
        }
        peak_alive_ =
            std::max(peak_alive_, counts_["founders"].get<uint64_t>() + counts_["natural_births"].get<uint64_t>() +
                                      counts_["forced_births"].get<uint64_t>() - counts_["deaths"].get<uint64_t>());
    } else if (type == "death" || type == "cull") {
        counts_["deaths"] = counts_["deaths"].get<uint64_t>() + 1;
        if (type == "cull")
            counts_["culled"] = counts_["culled"].get<uint64_t>() + 1;
        add(closed_, data["totals"]);
        completed_age_ += data["age"].get<uint64_t>();
        peak_distance_ = std::max(peak_distance_, data["totals"]["distance"].get<double>());
        for (const auto* key : {"resources", "actions", "acquired", "released"})
            for (std::size_t i = 0; i < data[key].size(); ++i)
                closed_[key][i] = closed_[key][i].get<double>() + data[key][i].get<double>();
        const auto cause = "death_cause_" + data.value("death_cause", std::string("unknown"));
        counts_[cause] = counts_.value(cause, uint64_t(0)) + 1;
    }
    publish("event", {{"event_type", type}, {"agent", std::move(data)}});
}
void Stats::record_tick_cost(int64_t us) {
    tick_us_ += us;
    ++measured_ticks_;
}
void Stats::record_frame(int64_t us, int64_t interval) {
    frame_us_ += us;
    frame_interval_us_ += interval;
    ++frames_;
}
void Stats::sample(uint64_t tick, const PopsManager& agents, const IWorld& world) {
    const bool replace = samples_ > 0 && latest_.value("tick", uint64_t(0)) == tick;
    const auto previous_averages = replace ? latest_["averages"] : Json::object();
    const double previous_correlation = replace ? latest_.value("corr_offspring_distance_alive", 0.0) : 0.0;
    if (replace && !rate_history_.empty())
        rate_history_.pop_back();
    if (!replace) {
        calcium_window_ticks_ = samples_ ? tick - latest_["tick"].get<uint64_t>() : 0;
        previous_distances_ = std::move(current_distances_);
        current_distances_.clear();
    }

    tick_ = tick;
    auto live = agents.telemetry(false);
    Json active = empty_totals();
    Json learning = Json::object();
    auto all = closed_;
    const auto world_organics = world.get_total_organics();
    auto total_resources = resources(world_organics);
    std::size_t saturated_cells = 0;
    for (const auto& cell : world.get_cells())
        saturated_cells += cell->get_calcium() == MaxCaco3;
    // Online centred covariance avoids cancellation when movement or calcium is nearly constant.
    std::size_t calcium_n = 0;
    double calcium_mean = 0, recent_distance_mean = 0, calcium_m2 = 0, recent_distance_m2 = 0, covariance = 0;
    double max_distance = 0, offspring_sq = 0, distance_sq = 0, product = 0;
    double signal_abs = 0, signal_sq = 0, signal_positive = 0, signal_near_zero = 0;
    for (const auto& p : live) {
        if (p.contains("learning")) {
            add(learning, p["learning"]);
            const double signal = p["learning"]["signal"];
            signal_abs += std::abs(signal);
            signal_sq += signal * signal;
            signal_positive += signal > 0;
            signal_near_zero += std::abs(signal) < 1e-4;
        }
        add(active, p["totals"]);
        add(all, p["totals"]);
        const double d = p["totals"]["distance"], o = p["totals"]["offspring"];
        const std::string id = p["pop_id"];
        current_distances_[id] = d;
        const auto previous = previous_distances_.find(id);
        if (calcium_window_ticks_ && previous != previous_distances_.end()) {
            const int x = p["position"][0], y = p["position"][1];
            double calcium = 0;
            unsigned neighbours = 0;
            for (int dy = -1; dy <= 1; ++dy) {
                for (int dx = -1; dx <= 1; ++dx) {
                    if (!dx && !dy)
                        continue;
                    if (const auto cell = world.get_cell(PositionT{x + dx, y + dy})) {
                        calcium += cell->get_calcium();
                        ++neighbours;
                    }
                }
            }
            if (neighbours) {
                calcium /= neighbours;
                const double distance = d - previous->second;
                ++calcium_n;
                const double dc = calcium - calcium_mean, dd = distance - recent_distance_mean;
                calcium_mean += dc / calcium_n;
                recent_distance_mean += dd / calcium_n;
                calcium_m2 += dc * (calcium - calcium_mean);
                recent_distance_m2 += dd * (distance - recent_distance_mean);
                covariance += dc * (distance - recent_distance_mean);
            }
        }
        max_distance = std::max(max_distance, d);
        offspring_sq += o * o;
        distance_sq += d * d;
        product += o * d;
        for (std::size_t i = 0; i < 6; ++i)
            total_resources[i] = total_resources[i].get<int64_t>() + p["resources"][i].get<int64_t>();
        for (const auto* key : {"actions", "acquired", "released"})
            for (std::size_t i = 0; i < p[key].size(); ++i)
                all[key][i] = all[key][i].get<double>() + p[key][i].get<double>();
    }
    const auto regen = world.get_regeneration_totals();
    if (initial_.empty())
        initial_ = {{"resources", total_resources},
                    {"energy", active["energy"]},
                    {"respirations", all["respirations"]},
                    {"photosynthesis", all["photosynthesis"]},
                    {"regenerated_glucose", regen.first},
                    {"regenerated_o2", regen.second}};
    const auto n = live.size();
    for (auto it = learning.begin(); it != learning.end(); ++it)
        it.value() = n ? it.value().get<double>() / n : 0;
    learning["n"] = n;
    const double mean_signal = learning.value("signal", 0.0);
    learning["mean_abs_signal"] = n ? signal_abs / n : 0;
    learning["signal_std"] = n ? std::sqrt(std::max(0.0, signal_sq / n - mean_signal * mean_signal)) : 0;
    learning["positive_signal_fraction"] = n ? signal_positive / n : 0;
    learning["near_zero_signal_fraction"] = n ? signal_near_zero / n : 0;
    Json averages = active;
    for (auto it = averages.begin(); it != averages.end(); ++it)
        it.value() = n ? it.value().get<double>() / n : 0;
    const double vx = n ? offspring_sq / n - std::pow(averages["offspring"].get<double>(), 2) : 0;
    const double vy = n ? distance_sq / n - std::pow(averages["distance"].get<double>(), 2) : 0;
    const double correlation =
        n && vx > 1e-12 && vy > 1e-12
            ? (product / n - averages["offspring"].get<double>() * averages["distance"].get<double>()) /
                  std::sqrt(vx * vy)
            : 0;
    const int64_t respiration = all["respirations"].get<int64_t>() - initial_["respirations"].get<int64_t>(),
                  photo = all["photosynthesis"].get<int64_t>() - initial_["photosynthesis"].get<int64_t>();
    const auto glucose_input = regen.first - initial_["regenerated_glucose"].get<uint64_t>();
    const auto oxygen_input = regen.second - initial_["regenerated_o2"].get<uint64_t>();
    const std::vector<int64_t> changes = {int64_t(glucose_input) + photo - respiration,
                                          respiration - photo,
                                          0,
                                          respiration - photo,
                                          int64_t(oxygen_input) + photo - respiration,
                                          0};
    Json residual = Json::array();
    for (std::size_t i = 0; i < 6; ++i)
        residual.push_back(total_resources[i].get<int64_t>() - initial_["resources"][i].get<int64_t>() - changes[i] +
                           closed_["resources"][i].get<int64_t>() - recovery_resources_[i]);
    double expected = all["respiration_gain"].get<double>() - all["metabolism_cost"].get<double>() -
                      all["movement_cost"].get<double>() - all["other_action_cost"].get<double>() -
                      all["reproduction_cost"].get<double>() -
                      all["thermoregulation_cost"].get<double>() - all["energy_cap"].get<double>() -
                      closed_["energy"].get<double>() + all["roundoff"].get<double>() +
                      all["pending_action_cost"].get<double>() + recovery_energy_;
    if (!initial_.contains("flow_origin"))
        initial_["flow_origin"] = expected;
    expected -= initial_["flow_origin"].get<double>();
    const auto births = counts_["natural_births"].get<uint64_t>() + counts_["forced_births"].get<uint64_t>();
    double birth_rate = 0, death_rate = 0;
    if (!rate_history_.empty() && tick > rate_history_.front()["tick"].get<uint64_t>()) {
        const auto& base = rate_history_.front();
        const double seconds = (tick - base["tick"].get<uint64_t>()) * .01;
        birth_rate = (births - base["births"].get<uint64_t>()) / seconds;
        death_rate = (counts_["deaths"].get<uint64_t>() - base["deaths"].get<uint64_t>()) / seconds;
    }
    rate_history_.push_back({{"tick", tick}, {"births", births}, {"deaths", counts_["deaths"]}});
    if (rate_history_.size() > 10)
        rate_history_.pop_front();
    latest_ = {{"tick", tick},
               {"simulated_seconds", tick * .01},
               {"wall_seconds", std::chrono::duration<double>(std::chrono::steady_clock::now() - started_).count()},
               {"process_cpu_seconds", double(std::clock() - cpu_started_) / CLOCKS_PER_SEC},
               {"alive", n},
               {"generation", agents.get_generation_count()},
               {"counts", counts_},
               {"learning", learning},
               {"reproduction", agents.reproduction_stats()},
               {"active", active},
               {"averages", averages},
               {"totals", all},
               {"world_resources", resources(world_organics)},
               {"world_nitrogen", world_organics.n2},
               {"calcium_saturated_cells", saturated_cells},
               {"calcium_distance_correlation",
                {{"pearson", calcium_n >= 2 && calcium_m2 > 0 && recent_distance_m2 > 0
                                 ? Json(std::clamp(covariance / std::sqrt(calcium_m2 * recent_distance_m2), -1.0, 1.0))
                                 : Json(nullptr)},
                 {"n", calcium_n},
                 {"window_ticks", calcium_window_ticks_},
                 {"mean_neighbour_calcium", calcium_n ? Json(calcium_mean) : Json(nullptr)},
                 {"mean_distance", calcium_n ? Json(recent_distance_mean) : Json(nullptr)}}},
               {"max_distance_alive", max_distance},
               {"corr_offspring_distance_alive", correlation},
               {"birth_rate_simulated_second", birth_rate},
               {"death_rate_simulated_second", death_rate},
               {"backlog", agents.get_repro_backlog_size()},
               {"backlog_peak", agents.get_repro_backlog_peak()},
               {"backlog_dropped", agents.get_repro_dropped_total()},
               {"reproduction_attempts_last_tick", agents.get_repro_processed_this_tick()},
               {"avg_tick_us", measured_ticks_ ? double(tick_us_) / measured_ticks_ : 0},
               {"avg_frame_us", frames_ ? double(frame_us_) / frames_ : 0},
               {"fps", frame_interval_us_ ? frames_ * 1e6 / frame_interval_us_ : 0},
               {"transport", transport_status()}};
    if (replace) {
        for (auto it = previous_averages.begin(); it != previous_averages.end(); ++it)
            sample_sums_[it.key()] = sample_sums_[it.key()].get<double>() - it.value().get<double>();
        correlation_sum_ -= previous_correlation;
    } else
        ++samples_;
    peak_distance_ = std::max(peak_distance_, max_distance);
    add(sample_sums_, averages);
    correlation_sum_ += correlation;
    auto means = sample_sums_;
    for (auto it = means.begin(); it != means.end(); ++it)
        it.value() = it.value().get<double>() / samples_;
    means["corr_offspring_distance_alive"] = correlation_sum_ / samples_;
    latest_["sample_means"] = std::move(means);
    latest_["peak_alive"] = peak_alive_;
    latest_["max_lifetime_distance"] = peak_distance_;
    const auto completed = counts_["deaths"].get<uint64_t>();
    latest_["completed_lives"] = {{"count", completed},
                                  {"age_sum", completed_age_},
                                  {"mean_age", completed ? double(completed_age_) / completed : 0},
                                  {"mean_offspring", completed ? closed_["offspring"].get<double>() / completed : 0},
                                  {"mean_distance", completed ? closed_["distance"].get<double>() / completed : 0}};
    BalanceRecord balance{initial_["resources"].get<std::array<int64_t, 6>>(),
                          total_resources.get<std::array<int64_t, 6>>(),
                          closed_["resources"].get<std::array<int64_t, 6>>(),
                          residual.get<std::array<int64_t, 6>>(),
                          glucose_input,
                          oxygen_input,
                          initial_["energy"].get<double>(),
                          active["energy"].get<double>(),
                          closed_["energy"].get<double>(),
                          active["energy"].get<double>() - initial_["energy"].get<double>() - expected};
    const double wall_seconds = latest_["wall_seconds"], cpu_seconds = latest_["process_cpu_seconds"];
    record_sample({tick, n, wall_seconds, cpu_seconds, balance, std::move(latest_)});
}
void Stats::end_session(uint64_t tick, const std::string& reason, const PopsManager& agents, const IWorld& world) {
    sample(tick, agents, world);
    for (auto& p : agents.telemetry(false))
        record_event({LifeEvent::Censored, std::move(p)});
    publish("end", {{"reason", reason}, {"summary", latest_}});
}
void Stats::publish_status(Json status) {
    status["performance"] = performance();
    status["transport"] = transport_status();
    status["metadata"] = metadata_;
    publish("status", std::move(status));
}
void Stats::record_diagnostic(DiagnosticResult result) {
    publish("diagnostic", std::move(result.measurements));
}
std::vector<Json> Stats::commands() {
    return communicator_ ? communicator_->receive() : std::vector<Json>{};
}
Json Stats::transport_status() const {
    return communicator_ ? communicator_->status() : Json{{"connected", false}, {"dropped_messages", 0}};
}
bool Stats::flush(unsigned ms) {
    return !communicator_ || communicator_->flush(ms);
}

nlohmann::json Stats::performance() const {
    return {{"process_cpu_seconds", double(std::clock() - cpu_started_) / CLOCKS_PER_SEC},
            {"avg_tick_us", measured_ticks_ ? double(tick_us_) / measured_ticks_ : 0},
            {"avg_frame_us", frames_ ? double(frame_us_) / frames_ : 0},
            {"fps", frame_interval_us_ ? frames_ * 1e6 / frame_interval_us_ : 0}};
}

void Stats::record_environment(const IWorld& world) {
    Json cells = Json::array();
    std::size_t offset = 0;
    const auto& source = world.get_cells();
    for (const auto& cell : source) {
        cells.push_back({{"resources", resources(cell->get_organics())},
                         {"temperature", cell->get_temperature()},
                         {"elevation", cell->get_elevation()}});
        if (cells.size() == 2048) {
            publish("environment", {{"offset", offset}, {"total", source.size()}, {"cells", std::move(cells)}});
            offset += 2048;
            cells = Json::array();
        }
    }
    if (!cells.empty())
        publish("environment", {{"offset", offset}, {"total", source.size()}, {"cells", std::move(cells)}});
}

void Stats::record_sample(SampleRecord sample) {
    tick_ = sample.tick;
    latest_ = std::move(sample.measurements);
    latest_["tick"] = sample.tick;
    latest_["alive"] = sample.alive;
    latest_["simulated_seconds"] = sample.tick * .01;
    latest_["wall_seconds"] = sample.wall_seconds;
    latest_["process_cpu_seconds"] = sample.process_cpu_seconds;
    const auto& b = sample.balance;
    latest_["resource_ledger"] = {{"initial", b.initial_resources},
                                  {"final", b.final_resources},
                                  {"death_overflow", b.death_overflow},
                                  {"residual", b.resource_residual},
                                  {"regenerated_glucose", b.regenerated_glucose},
                                  {"regenerated_o2", b.regenerated_o2}};
    latest_["resource_ledger"]["recovery_input"] = recovery_resources_;
    latest_["energy_ledger"] = {{"initial", b.initial_energy},
                                {"final_alive", b.final_energy},
                                {"dead_sink", b.dead_energy_sink},
                                {"residual", b.energy_residual}};
    latest_["energy_ledger"]["recovery_input"] = recovery_energy_;
    publish("sample", latest_);
}

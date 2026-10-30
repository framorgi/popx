#pragma once

#include "i_agent.h"
#include "pop.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <nlohmann/json.hpp>
#include <random>
#include <sstream>

struct ReaperSettings {
    unsigned count = 0;
    unsigned move_interval = 100;
    unsigned kill_interval = 240;
    unsigned announce_interval = 20;
    unsigned warning_radius = 6;
    unsigned newborn_protection = 250;
};

/// @brief Physical predator. Its warning occupies the existing DANGER channel before a possible attack.
class Reaper final : public IAgent, public std::enable_shared_from_this<Reaper> {
  public:
    Reaper(IWorld& world, ReaperSettings settings, uint32_t seed = std::random_device{}())
        : world_(world), settings_(settings), rng_(seed) { init(); }

    void init() override {
        alive_ = true;
        spawned_ = false;
        move_tick_ = announce_tick_ = 0;
        kill_tick_ = 0;
        last_kills_ = 0;
        heading_ = std::uniform_real_distribution<double>(-pi, pi)(rng_);
        change_curvature();
    }
    void die() override {
        if (spawned_) world_.remove_entity(shared_from_this());
        spawned_ = false;
        alive_ = false;
    }
    void despawn() override { die(); }
    void sense() override {}
    void think() override {}
    void update() override { act(); }
    void act() override {
        last_kills_ = 0;
        if (!alive_ || !spawned_) return;
        if (++announce_tick_ >= settings_.announce_interval) {
            announce_tick_ = 0;
            announce();
        }
        kill_tick_ = std::min(kill_tick_ + 1, settings_.kill_interval);
        const auto ahead = direction(heading_);
        const PositionT target{position_.x + ahead.x, position_.y + ahead.y};
        if (kill_tick_ >= settings_.kill_interval) {
            auto cell = world_.get_cell(target);
            auto victim = cell ? std::dynamic_pointer_cast<Pop>(cell->get_occupant().lock()) : nullptr;
            if (victim && victim->is_alive() && victim->get_age() >= settings_.newborn_protection) {
                victim->die_by_reaper();
                world_.remove_entity(victim);
                ++last_kills_;
                kill_tick_ = 0;
            }
        }
        if (++move_tick_ < settings_.move_interval) return;
        move_tick_ = 0;
        if (--turns_remaining_ == 0) change_curvature();
        // Rarely steer toward a mature Pop. A purely local probe can wander for
        // thousands of ticks in an empty sector of a large world.
        if (std::uniform_int_distribution<int>(0, 3)(rng_) == 0) steer_toward_pop();
        heading_ = std::remainder(heading_ + curvature_, 2 * pi);
        const auto step = direction(heading_);
        if (try_move({position_.x + step.x, position_.y + step.y})) return;
        std::array<unsigned, 8> choices{0, 1, 2, 3, 4, 5, 6, 7};
        std::shuffle(choices.begin(), choices.end(), rng_);
        for (unsigned choice : choices) {
            const double proposed = choice * pi / 4;
            const auto alternative = direction(proposed);
            if (try_move({position_.x + alternative.x, position_.y + alternative.y})) {
                heading_ = proposed;
                change_curvature();
                break;
            }
        }
    }
    bool is_alive() override { return alive_; }
    bool try_spawn(PositionT position) override {
        if (!alive_ || spawned_) return false;
        const auto previous = position_;
        position_ = position;
        if (world_.add_entity(shared_from_this())) { spawned_ = true; return true; }
        position_ = previous;
        return false;
    }
    bool try_move(PositionT position) override {
        if (!alive_ || !spawned_ || !world_.is_free(position) ||
            world_.get_elevation(position) - world_.get_elevation(position_) >= MaxClimbableSlope)
            return false;
        if (!world_.move_entity(shared_from_this(), position)) return false;
        position_ = position;
        return true;
    }
    [[nodiscard]] PositionT get_position() const override { return position_; }
    [[nodiscard]] unsigned last_kills() const { return last_kills_; }
    [[nodiscard]] RenderState get_render_state() const override {
        return {RenderShape::Square, {float(position_.x), float(position_.y)}, 1.3f,
                Color(75, 13, 29), ReaperVisualData{}, true};
    }
    [[nodiscard]] nlohmann::json snapshot() const {
        std::ostringstream state;
        state << rng_;
        return {{"x", position_.x}, {"y", position_.y}, {"heading", heading_}, {"curvature", curvature_},
                {"turns_remaining", turns_remaining_}, {"move_tick", move_tick_},
                {"kill_tick", kill_tick_}, {"announce_tick", announce_tick_}, {"rng", state.str()}};
    }
    bool restore(const nlohmann::json& state) {
        const PositionT p{state.at("x").get<int>(), state.at("y").get<int>()};
        const double heading = state.at("heading"), curvature = state.at("curvature");
        const unsigned turns = state.at("turns_remaining"), move_tick = state.at("move_tick"),
                       kill_tick = state.at("kill_tick"), announce_tick = state.at("announce_tick");
        std::istringstream input(state.at("rng").get<std::string>());
        std::mt19937 restored;
        if (!std::isfinite(heading) || !std::isfinite(curvature) || std::abs(curvature) > .25 ||
            turns == 0 || turns > 24 || move_tick >= settings_.move_interval ||
            kill_tick > settings_.kill_interval || announce_tick >= settings_.announce_interval ||
            !(input >> restored)) return false;
        if (!try_spawn(p)) {
            // Carbonate may fill the cell after the Reaper spawned.
            auto cell = world_.get_cell(p);
            if (!cell || !cell->get_occupant().expired()) return false;
            position_ = p;
            cell->set_occupant(shared_from_this());
            spawned_ = true;
        }
        heading_ = heading; curvature_ = curvature; turns_remaining_ = turns;
        move_tick_ = move_tick; kill_tick_ = kill_tick; announce_tick_ = announce_tick;
        rng_ = restored; last_kills_ = 0;
        return true;
    }

  private:
    static constexpr double pi = 3.14159265358979323846;
    static PositionT direction(double angle) {
        return {int(std::lround(std::cos(angle))), int(std::lround(std::sin(angle)))};
    }
    void change_curvature() {
        curvature_ = std::uniform_real_distribution<double>(-.16, .16)(rng_);
        turns_remaining_ = std::uniform_int_distribution<unsigned>(6, 18)(rng_);
    }
    void announce() {
        const auto forward = direction(heading_);
        const PositionT side{-forward.y, forward.x};
        for (unsigned distance = 0; distance <= settings_.warning_radius; ++distance) {
            const int width = std::max(1, int(distance / 2));
            for (int lateral = -width; lateral <= width; ++lateral) {
                const PositionT p{position_.x + int(distance) * forward.x + lateral * side.x,
                                  position_.y + int(distance) * forward.y + lateral * side.y};
                auto cell = world_.get_cell(p);
                if (!cell) continue;
                const float target = float(MaxFeromones) * (1.f - float(distance) /
                                           float(settings_.warning_radius + 1)) * .85f;
                const auto current = cell->get_feromone_map().at(DANGER_FEROMONE);
                if (target > current) cell->set_feromone(DANGER_FEROMONE, target - current);
            }
        }
    }
    void steer_toward_pop() {
        const int width = world_.get_width();
        const auto cells = world_.get_cells();
        int best = width * width + world_.get_height() * world_.get_height() + 1;
        PositionT target = position_;
        for (std::size_t index = 0; index < cells.size(); ++index) {
            auto occupant = cells[index]->get_occupant().lock();
            if (!occupant) continue;
            auto pop = std::dynamic_pointer_cast<Pop>(occupant);
            if (!pop || !pop->is_alive() || pop->get_age() < settings_.newborn_protection) continue;
            const PositionT p{int(index % width), int(index / width)};
            const int dx = p.x - position_.x, dy = p.y - position_.y;
            const int distance = dx * dx + dy * dy;
            if (distance < best) { best = distance; target = p; }
        }
        if (target.x == position_.x && target.y == position_.y) return;
        const double desired = std::atan2(double(target.y - position_.y), double(target.x - position_.x));
        const double turn = std::remainder(desired - heading_, 2 * pi);
        heading_ = std::remainder(heading_ + std::clamp(turn, -0.5, 0.5), 2 * pi);
    }
    IWorld& world_;
    ReaperSettings settings_;
    std::mt19937 rng_;
    PositionT position_{0, 0};
    double heading_ = 0, curvature_ = 0;
    unsigned turns_remaining_ = 0, move_tick_ = 0, kill_tick_ = 0, announce_tick_ = 0, last_kills_ = 0;
    bool alive_ = true, spawned_ = false;
};

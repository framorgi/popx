#pragma once

#include "i_agent.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <deque>
#include <nlohmann/json.hpp>
#include <random>
#include <sstream>
#include <vector>

struct SourceSettings {
    unsigned count = 2;
    unsigned move_interval = 40;
    unsigned glucose_interval = 40;
    unsigned glucose_per_neighbor = 8;
};

/// @brief A physical, non-reproducing glucose source following a slowly turning path.
class Source final : public IAgent, public std::enable_shared_from_this<Source> {
  public:
    Source(IWorld& world, SourceSettings settings, uint32_t seed = std::random_device{}())
        : world_(world), settings_(settings), rng_(seed) { init(); }

    void init() override {
        alive_ = true;
        spawned_ = false;
        move_tick_ = glucose_tick_ = last_deposit_ = 0;
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
        last_deposit_ = 0;
        if (!alive_ || !spawned_) return;
        if (++move_tick_ >= settings_.move_interval) {
            move_tick_ = 0;
            if (--turns_remaining_ == 0) change_curvature();
            heading_ = wrap(heading_ + curvature_);
            const auto next = direction(heading_);
            if (!try_move({position_.x + next.x, position_.y + next.y})) {
                // A blocked move changes both direction and curvature before another try.
                std::array<unsigned, 8> choices{0, 1, 2, 3, 4, 5, 6, 7};
                std::shuffle(choices.begin(), choices.end(), rng_);
                for (unsigned choice : choices) {
                    heading_ = wrap(choice * pi / 4.0);
                    change_curvature();
                    const auto alternative = direction(heading_);
                    if (try_move({position_.x + alternative.x, position_.y + alternative.y})) break;
                }
            }
        }
        if (++glucose_tick_ >= settings_.glucose_interval) {
            glucose_tick_ = 0;
            deposit();
        }
    }
    bool is_alive() override { return alive_; }
    bool try_spawn(PositionT position) override {
        if (!alive_ || spawned_) return false;
        const auto previous = position_;
        position_ = position;
        if (world_.add_entity(shared_from_this())) {
            spawned_ = true;
            return true;
        }
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
    [[nodiscard]] unsigned last_deposit() const { return last_deposit_; }
    [[nodiscard]] nlohmann::json snapshot() const {
        std::ostringstream state;
        state << rng_;
        return {{"x", position_.x}, {"y", position_.y}, {"heading", heading_}, {"curvature", curvature_},
                {"turns_remaining", turns_remaining_}, {"move_tick", move_tick_},
                {"glucose_tick", glucose_tick_}, {"rng", state.str()}};
    }
    bool restore(const nlohmann::json& state) {
        const auto p = PositionT{state.at("x").get<int>(), state.at("y").get<int>()};
        const auto heading = state.at("heading").get<double>();
        const auto curvature = state.at("curvature").get<double>();
        const auto turns = state.at("turns_remaining").get<unsigned>();
        const auto move_tick = state.at("move_tick").get<unsigned>();
        const auto glucose_tick = state.at("glucose_tick").get<unsigned>();
        std::istringstream input(state.at("rng").get<std::string>());
        std::mt19937 restored;
        if (!std::isfinite(heading) || !std::isfinite(curvature) || std::abs(curvature) > 0.25 ||
            turns == 0 || turns > 24 || move_tick >= settings_.move_interval ||
            glucose_tick >= settings_.glucose_interval || !(input >> restored))
            return false;
        if (!try_spawn(p)) {
            // A source may legitimately stand on carbonate deposited after its spawn.
            auto cell = world_.get_cell(p);
            if (!cell || !cell->get_occupant().expired()) return false;
            position_ = p;
            cell->set_occupant(shared_from_this());
            spawned_ = true;
        }
        heading_ = heading;
        curvature_ = curvature;
        turns_remaining_ = turns;
        move_tick_ = move_tick;
        glucose_tick_ = glucose_tick;
        rng_ = restored;
        last_deposit_ = 0;
        return true;
    }
    [[nodiscard]] RenderState get_render_state() const override {
        return {RenderShape::Square, {static_cast<float>(position_.x), static_cast<float>(position_.y)},
                1.15f, Color(0, 0, 0), SourceVisualData{}, true};
    }

  private:
    static constexpr double pi = 3.14159265358979323846;
    [[nodiscard]] static double wrap(double a) { return std::remainder(a, 2.0 * pi); }
    [[nodiscard]] static PositionT direction(double a) {
        return {static_cast<int>(std::lround(std::cos(a))), static_cast<int>(std::lround(std::sin(a)))};
    }
    void change_curvature() {
        curvature_ = std::uniform_real_distribution<double>(-0.22, 0.22)(rng_);
        turns_remaining_ = std::uniform_int_distribution<unsigned>(6, 18)(rng_);
    }
    void deposit() {
        const int width = world_.get_width(), height = world_.get_height();
        std::vector<uint8_t> visited(static_cast<size_t>(width) * height, 0);
        std::deque<PositionT> frontier;
        auto enqueue = [&](PositionT p) {
            if (p.x < 0 || p.y < 0 || p.x >= width || p.y >= height) return;
            const auto i = static_cast<size_t>(p.y) * width + p.x;
            if (!visited[i]) {
                visited[i] = 1;
                frontier.push_back(p);
            }
        };
        unsigned overflow = 0;
        for (int dy = -1; dy <= 1; ++dy)
            for (int dx = -1; dx <= 1; ++dx) {
                if (!dx && !dy) continue;
                PositionT p{position_.x + dx, position_.y + dy};
                auto cell = world_.get_cell(p);
                if (!cell) continue;
                enqueue(p);
                const auto accepted = cell->give_glucose(settings_.glucose_per_neighbor);
                last_deposit_ += accepted;
                overflow += settings_.glucose_per_neighbor - accepted;
            }
        // Only overflow traverses the flood; the source's own cell is never filled.
        while (overflow && !frontier.empty()) {
            const auto p = frontier.front();
            frontier.pop_front();
            if (p.x != position_.x || p.y != position_.y) {
                const auto accepted = world_.get_cell(p)->give_glucose(overflow);
                last_deposit_ += accepted;
                overflow -= accepted;
            }
            for (int dy = -1; dy <= 1; ++dy)
                for (int dx = -1; dx <= 1; ++dx)
                    if (dx || dy) enqueue({p.x + dx, p.y + dy});
        }
    }
    IWorld& world_;
    SourceSettings settings_;
    std::mt19937 rng_;
    PositionT position_{0, 0};
    double heading_ = 0, curvature_ = 0;
    unsigned turns_remaining_ = 0, move_tick_ = 0, glucose_tick_ = 0, last_deposit_ = 0;
    bool alive_ = true, spawned_ = false;
};

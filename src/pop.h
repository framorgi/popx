#pragma once
#include "i_agent.h"
#include "i_logger.h"
#include "i_world.h"

/// -----------------------------------------------------------------------------
/// @class Pop
/// @brief Implements the IAgent interface representing a population entity within the simulation.
/// -----------------------------------------------------------------------------
class Pop : public IAgent, public std::enable_shared_from_this<Pop> {
  private:
    /* data */
  public:
    Pop(std::weak_ptr<IWorld> world, std::shared_ptr<ILogger> logger);

    void init() override;
    void die() override;
    void sense() override;
    void think() override;
    void act() override;
    bool try_move(Position p) override;
    [[nodiscard]] Position get_position() const override;
    bool is_alive() override;
    bool try_spawn(Position p) override;
    void update() override;
    void despawn() override;

  private:
    Position pos_;
    bool alive_ = false;
    std::weak_ptr<IWorld> world_; // Observes the world without owning it
    std::shared_ptr<ILogger> logger_;
};

#pragma once
#include "i_agent.h"
#include "i_logger.h"
#include "i_world.h"

/// -----------------------------------------------------------------------------
/// @class Pop
/// @brief Implements the IAgent interface representing a population entity within the simulation.
/// -----------------------------------------------------------------------------
class Pop : public IAgent, public std::enable_shared_from_this<Pop> {
  public:
    ///----------------------------------------------------------------------------
    /// @brief Constructor for Pop class.
    /// @param world Weak pointer to the world the Pop belongs to.
    /// @param logger Shared pointer to a logger for logging events and errors.
    ///----------------------------------------------------------------------------
    Pop(std::weak_ptr<IWorld> world, std::shared_ptr<ILogger> logger);
    ///---------------------------------------------------------------------------
    /// @brief Initializes the Pop entity (sets initial stats, behaviors, etc.).
    ///---------------------------------------------------------------------------
    void init() override;
    ///---------------------------------------------------------------------------
    /// @brief Handles the death of the Pop entity (cleanup, final actions, mark as dead).
    ///---------------------------------------------------------------------------
    void die() override;
    ///---------------------------------------------------------------------------
    /// @brief Senses the environment (perceive nearby entities, environment, etc.).
    ///---------------------------------------------------------------------------
    void sense() override;
    ///---------------------------------------------------------------------------
    /// @brief Thinks and makes decisions (AI, behavior selection, goal planning).
    ///---------------------------------------------------------------------------
    void think() override;
    ///---------------------------------------------------------------------------
    /// @brief Acts based on decisions made (move, interact, consume resources, etc.).
    ///---------------------------------------------------------------------------
    void act() override;
    ///---------------------------------------------------------------------------
    /// @brief Attempts to move the Pop to a new position.
    /// @param p The target position to move to.
    /// @return True if the move was successful, false otherwise.
    ///---------------------------------------------------------------------------
    bool try_move(Position p) override;
    ///---------------------------------------------------------------------------
    /// @brief Gets the current position of the Pop.
    /// @return The current position of the Pop.
    ///---------------------------------------------------------------------------

    [[nodiscard]] Position get_position() const override;
    ///---------------------------------------------------------------------------
    /// @brief Checks if the Pop is alive.
    /// @return True if the Pop is alive, false otherwise.
    ///---------------------------------------------------------------------------
    bool is_alive() override;
    ///---------------------------------------------------------------------------
    /// @brief Attempts to spawn a new Pop at the specified position.
    /// @param p The position to spawn the new Pop.
    /// @return True if the spawn was successful, false otherwise.
    ///---------------------------------------------------------------------------
    bool try_spawn(Position p) override;
    ///---------------------------------------------------------------------------
    /// @brief Updates the state of the Pop (called each simulation tick).
    ///---------------------------------------------------------------------------
    void update() override;
    ///---------------------------------------------------------------------------
    /// @brief Despawns the Pop from the simulation (removes it from the world).
    ///---------------------------------------------------------------------------
    void despawn() override;

  private:
    /// ---------------------------------------------------------------------------
    /// @brief Current position of the Pop in the world.
    /// ---------------------------------------------------------------------------
    Position pos_;
    /// ---------------------------------------------------------------------------
    /// @brief Alive status of the Pop entity.
    /// ---------------------------------------------------------------------------
    bool alive_ = false;
    /// ---------------------------------------------------------------------------
    /// @brief Weak pointer to the world the Pop belongs to.
    /// ---------------------------------------------------------------------------
    std::weak_ptr<IWorld> world_; // Observes the world without owning it
    /// ---------------------------------------------------------------------------
    /// @brief Shared pointer to a logger for logging events and errors.
    /// ---------------------------------------------------------------------------
    std::shared_ptr<ILogger> logger_;
};

#pragma once
#include "common.h"
#include "i_entity.h"
#include "i_world.h"

#include <memory>
class IAgent : public IEntity {
  private:
    /* data */
  public:
    ~IAgent() = default;

    /// @brief Initialize the agent
    virtual void init() = 0;

    /// @brief Terminate the agent
    virtual void die() = 0;

    /// @brief Make the agent sense its environment
    virtual void sense() = 0;

    /// @brief Make the agent think
    virtual void think() = 0;

    /// @brief Make the agent act
    virtual void act() = 0;

    /// @brief Check if the agent is alive
    virtual bool is_alive() = 0;
};
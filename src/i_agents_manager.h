#pragma once
#include "common.h"
#include "i_agent.h"

class IAgentsManager 
{
public:
    virtual ~IAgentsManager() = default;
    //TODO: rethink al agent manager actions and logic here

    /// @brief Create and register a new agent
    virtual bool spawn_population() = 0;

    /// @brief Remove an agent
    virtual void update_cycle() = 0;

};
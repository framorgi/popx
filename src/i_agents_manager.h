#pragma once
#include "common.h"
#include "i_agent.h"

class i_agents_manager 
{
public:
    virtual ~i_agents_manager() = default;

    /// @brief Create and register a new agent
    virtual bool spawn_population() = 0;

    /// @brief Remove an agent
    virtual void update_cycle() = 0;

};
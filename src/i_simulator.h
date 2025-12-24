#pragma once 
#include <memory>
#include "common.h"


///--------------------------------------------------------------------------
/// @brief    Interface that defines the simulator functionalities and logics 
///           Simulator is decoupled from rendering objects
///--------------------------------------------------------------------------

class ISimulator {
public:
    virtual ~ISimulator() = default;

///--------------------------------------------------------------------------
/// @brief    Initializes the simulator
///--------------------------------------------------------------------------
    virtual void init() = 0;
///--------------------------------------------------------------------------
/// @brief    Updates the simulator state
///--------------------------------------------------------------------------
    virtual void update() = 0;
 
};
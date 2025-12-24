#pragma once 
#include <memory>
#include "common.h"



class ISimulator {
public:
    virtual ~ISimulator() = default;

    virtual void init() = 0;
    virtual void update() = 0;
 
};
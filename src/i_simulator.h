#pragma once 
#include <memory>
#include "common.h"



class i_simulator {
public:
    virtual ~i_simulator() = default;

    virtual void init() = 0;
    virtual void update() = 0;
 
};
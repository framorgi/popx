#pragma once
#include <memory> 

class IRenderer
{
public:
    virtual ~IRenderer() = default;

    virtual void init()=0;
    virtual void draw(  )=0;
    virtual void save_frame()=0;

};

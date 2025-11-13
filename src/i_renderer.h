#pragma once
#include <memory> 

class i_renderer
{
public:
    virtual ~i_renderer() = default;

    virtual void init()=0;
    virtual void draw(  )=0;
    virtual void save_frame()=0;

};

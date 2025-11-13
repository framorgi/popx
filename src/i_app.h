#pragma once
class i_app
{
public:

    virtual ~i_app() = default;

    virtual void init()=0;
    virtual void run()=0;
    virtual void stop()=0;

};
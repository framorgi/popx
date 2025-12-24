#pragma once
class IApp
{
public:

    virtual ~IApp() = default;

    virtual void init()=0;
    virtual void run()=0;
    virtual void stop()=0;

};
#pragma once
class i_random {
public:

    virtual ~i_random() = default;

    virtual int rnd_int(int min, int max) = 0;
    virtual double rnd_double(double min, double max) = 0;
    virtual float rnd_float(float min, float max) = 0;
};
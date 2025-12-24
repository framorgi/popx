
#pragma once
#include "i_random.h"
#include <random>
class RandomUtility : public IRandom {
public:
    RandomUtility();

    int rnd_int(int min, int max) override;
    double rnd_double(double min, double max) override;
    float rnd_float(float min, float max) override;
private:
    std::mt19937 mt_;
};

    
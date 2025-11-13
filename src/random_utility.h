
#pragma once
#include "i_random.h"
#include <random>
class random_utility : public i_random {
public:
    random_utility();

    int rnd_int(int min, int max) override;
    double rnd_double(double min, double max) override;
    float rnd_float(float min, float max) override;
private:
    std::mt19937 mt_;
};

    
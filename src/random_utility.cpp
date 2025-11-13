#include "random_utility.h"

random_utility::random_utility() : mt_(std::random_device{}())  
{
}

int random_utility::rnd_int(int min, int max)
{
    std::uniform_int_distribution<int> dist(min, max);
    return dist(mt_);
}

double random_utility::rnd_double(double min, double max)
{
    std::uniform_real_distribution<double> dist(min, max);
    return dist(mt_);
}

float random_utility::rnd_float(float min, float max)
{
    std::uniform_real_distribution<float> dist(min, max);
    return dist(mt_);
}
double Random_double(double min, double max)
{
    return min + (max - min) * (rand() / (double)RAND_MAX);
}

int Random_int(int min, int max)
{
    return min + rand() % (max - min + 1);
}


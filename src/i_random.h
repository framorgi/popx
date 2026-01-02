#pragma once
#include <vector>
using GaussianBaseFunction = struct {
    double amplitude;          // Peak height of the Gaussian
    double mean_x, mean_y;     // Center position of the Gaussian  x and y
    double stddev_x, stddev_y; // Standard deviation (width and height)  of the Gaussian
};

using RBFSet = struct {
    double max_value;
    double min_value;
    std::vector<GaussianBaseFunction> data;
};

class IRandom {
  public:
    virtual ~IRandom() = default;

    virtual int rnd_int(int min, int max) = 0;
    virtual double rnd_double(double min, double max) = 0;
    virtual float rnd_float(float min, float max) = 0;
    virtual GaussianBaseFunction rnd_gaussian_base_function(double amp_min, double amp_max, double mean_x_min,
                                                            double mean_x_max, double mean_y_min, double mean_y_max,
                                                            double stddev_x_min, double stddev_x_max,
                                                            double stddev_y_min, double stddev_y_max) = 0;
    virtual GaussianBaseFunction rnd_symmetric_gaussian_base_function(double amp_min, double amp_max, double mean_x_min,
                                                                      double mean_x_max, double mean_y_min,
                                                                      double mean_y_max, double stddev_min,
                                                                      double stddev_max) = 0;
    virtual RBFSet rnd_rbf_set(int count, double amp_min, double amp_max, double mean_x_min, double mean_x_max,
                               double mean_y_min, double mean_y_max, double stddev_x_min, double stddev_x_max,
                               double stddev_y_min, double stddev_y_max) = 0;

    virtual RBFSet rnd_symmetric_rbf_set(int count, double amp_min, double amp_max, double mean_x_min,
                                         double mean_x_max, double mean_y_min, double mean_y_max, double stddev_min,
                                         double stddev_max) = 0;

    virtual double evaluate_gaussian(const GaussianBaseFunction& gbf, double x, double y) = 0;
    virtual double evaluate_rbf_set(const RBFSet& rbf_set, double x, double y) = 0;
};
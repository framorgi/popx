
#pragma once
#include "i_random.h"

#include <cmath>
#include <random>
class RandomUtility : public IRandom {
  public:
    RandomUtility();

    int rnd_int(int min, int max) override;
    double rnd_double(double min, double max) override;
    float rnd_float(float min, float max) override;
    GaussianBaseFunction rnd_gaussian_base_function(double amp_min, double amp_max, double mean_x_min,
                                                    double mean_x_max, double mean_y_min, double mean_y_max,
                                                    double stddev_x_min, double stddev_x_max, double stddev_y_min,
                                                    double stddev_y_max) override;
    GaussianBaseFunction rnd_symmetric_gaussian_base_function(double amp_min, double amp_max, double mean_x_min,
                                                              double mean_x_max, double mean_y_min, double mean_y_max,
                                                              double stddev_min, double stddev_max) override;

    RBFSet rnd_rbf_set(int count, double amp_min, double amp_max, double mean_x_min, double mean_x_max,
                       double mean_y_min, double mean_y_max, double stddev_x_min, double stddev_x_max,
                       double stddev_y_min, double stddev_y_max) override;
    RBFSet rnd_symmetric_rbf_set(int count, double amp_min, double amp_max, double mean_x_min, double mean_x_max,
                                 double mean_y_min, double mean_y_max, double stddev_min, double stddev_max) override;
    double evaluate_gaussian(const GaussianBaseFunction& gbf, double x, double y) override;
    double evaluate_rbf_set(const RBFSet& rbf_set, double x, double y) override;

  private:
    std::mt19937 mt_;
};

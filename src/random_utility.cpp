#include "random_utility.h"

#include <algorithm>

RandomUtility::RandomUtility() : mt_(std::random_device{}()) {}

int RandomUtility::rnd_int(int min, int max) {
    std::uniform_int_distribution<int> dist(min, max);
    return dist(mt_);
}

double RandomUtility::rnd_double(double min, double max) {
    std::uniform_real_distribution<double> dist(min, max);
    return dist(mt_);
}

float RandomUtility::rnd_float(float min, float max) {
    std::uniform_real_distribution<float> dist(min, max);
    return dist(mt_);
}

GaussianBaseFunction RandomUtility::rnd_gaussian_base_function(double amp_min, double amp_max, double mean_x_min,
                                                               double mean_x_max, double mean_y_min, double mean_y_max,
                                                               double stddev_x_min, double stddev_x_max,
                                                               double stddev_y_min, double stddev_y_max) {
    GaussianBaseFunction gbf;
    gbf.amplitude = rnd_double(amp_min, amp_max);
    gbf.mean_x = rnd_double(mean_x_min, mean_x_max);
    gbf.mean_y = rnd_double(mean_y_min, mean_y_max);
    gbf.stddev_x = rnd_double(stddev_x_min, stddev_x_max);
    gbf.stddev_y = rnd_double(stddev_y_min, stddev_y_max);
    return gbf;
}

GaussianBaseFunction RandomUtility::rnd_symmetric_gaussian_base_function(double amp_min, double amp_max,
                                                                         double mean_x_min, double mean_x_max,
                                                                         double mean_y_min, double mean_y_max,
                                                                         double stddev_min, double stddev_max) {
    GaussianBaseFunction gbf;
    gbf.amplitude = rnd_double(amp_min, amp_max);
    gbf.mean_x = rnd_double(mean_x_min, mean_x_max);
    gbf.mean_y = rnd_double(mean_y_min, mean_y_max);
    gbf.stddev_x = rnd_double(stddev_min, stddev_max);
    gbf.stddev_y = gbf.stddev_x;
    return gbf;
}

RBFSet RandomUtility::rnd_rbf_set(int count, double amp_min, double amp_max, double mean_x_min, double mean_x_max,
                                  double mean_y_min, double mean_y_max, double stddev_x_min, double stddev_x_max,
                                  double stddev_y_min, double stddev_y_max) {
    RBFSet rbf_set;
    rbf_set.max_value = 0.0;

    for (int i = 0; i < count; ++i) {
        rbf_set.data.push_back(rnd_gaussian_base_function(amp_min, amp_max, mean_x_min, mean_x_max, mean_y_min,
                                                          mean_y_max, stddev_x_min, stddev_x_max, stddev_y_min,
                                                          stddev_y_max));
        if (rbf_set.data.back().amplitude > rbf_set.max_value) {
            rbf_set.max_value = rbf_set.data.back().amplitude;
        }
    }
    return rbf_set;
}

RBFSet RandomUtility::rnd_symmetric_rbf_set(int count, double amp_min, double amp_max, double mean_x_min,
                                            double mean_x_max, double mean_y_min, double mean_y_max, double stddev_min,
                                            double stddev_max) {
    RBFSet rbf_set;
    rbf_set.max_value = 0.0;
    for (int i = 0; i < count; ++i) {
        rbf_set.data.push_back(rnd_symmetric_gaussian_base_function(amp_min, amp_max, mean_x_min, mean_x_max,
                                                                    mean_y_min, mean_y_max, stddev_min, stddev_max));

        if (rbf_set.data.back().amplitude > rbf_set.max_value) {
            rbf_set.max_value = rbf_set.data.back().amplitude;
        }
    }

    return rbf_set;
}

double RandomUtility::evaluate_gaussian(const GaussianBaseFunction& gbf, double x, double y) {
    double coeff_x = (x - gbf.mean_x) / gbf.stddev_x;
    double coeff_y = (y - gbf.mean_y) / gbf.stddev_y;
    return gbf.amplitude * std::exp(-0.5 * (coeff_x * coeff_x + coeff_y * coeff_y));
}

double RandomUtility::evaluate_rbf_set(const RBFSet& rbf_set, double x, double y) {
    double result = 0.0;
    for (const auto& gbf : rbf_set.data) {
        result += evaluate_gaussian(gbf, x, y);
        if (result > rbf_set.max_value) {
            result = rbf_set.max_value;
        }
    }
    return result;
}

RBFSet RandomUtility::rotate_rbf_set(const RBFSet& rbf_set, unsigned int tick, unsigned int radius, double center_x,
                                     double center_y) {
    RBFSet moved_set = rbf_set;

    if (radius == 0U || moved_set.data.empty()) {
        return moved_set;
    }

    // Keep the same recalc cadence in GridWorld, but make each update move only a tiny amount.
    // We move along a circle using the incremental delta between consecutive angles.
    constexpr double kAngularStepPerUpdate = 0.009; // 0.0025;
    const double angle_now = static_cast<double>(tick) * kAngularStepPerUpdate;
    const double angle_prev = static_cast<double>(tick > 0U ? tick - 1U : 0U) * kAngularStepPerUpdate;

    const double dtheta = angle_now - angle_prev;
    const double cos_dt = std::cos(dtheta);
    const double sin_dt = std::sin(dtheta);

    for (auto& gbf : moved_set.data) {
        const double rel_x = gbf.mean_x - center_x;
        const double rel_y = gbf.mean_y - center_y;

        gbf.mean_x = center_x + (rel_x * cos_dt - rel_y * sin_dt);
        gbf.mean_y = center_y + (rel_x * sin_dt + rel_y * cos_dt);

        gbf.stddev_x = gbf.stddev_x + cos_dt * 0.01;
        gbf.stddev_y = gbf.stddev_y + sin_dt * 0.01;
    }

    return moved_set;
}
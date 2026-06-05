#pragma once
#include "i_brain.h"
#include "i_config.h"

#include <Eigen/Sparse>
#include <memory>
#include <string>
#include <vector>

/// @brief Concrete feed-forward neural network implementing IBrain.
/// Uses Eigen sparse matrices for memory-efficient weight storage.
class Brain : public IBrain {
  public:
    explicit Brain(std::shared_ptr<IConfig> config);

    void wire(const Genome& genome) override;
    int feed_forward(const std::vector<float>& sensor_values) override;
    void resize(unsigned size_s, unsigned size_n, unsigned size_y, unsigned num_hidden) override;
    void serialize(const std::string& pop_id, unsigned generation) const override;

    unsigned get_size_s() const override;
    unsigned get_size_n() const override;
    unsigned get_size_y() const override;
    unsigned get_num_hidden() const override;
    std::vector<bool> get_connected_sensors() const override;

  private:
    void allocate();
    /// Returns the neuron count for layer k:
    ///   k == 0             → size_s_ (sensors)
    ///   k == num_layers_-1 → size_y_ (outputs)
    ///   otherwise          → size_n_ (hidden)
    unsigned layer_size(unsigned k) const;
    float sigmoid(float x) const;

    /// Flat connection-matrix storage, indexed by src * num_layers_ + dst.
    /// Only entries with src < dst are populated during wire().
    std::vector<Eigen::SparseMatrix<float>> M_;

    unsigned size_s_;
    unsigned size_n_;
    unsigned size_y_;
    unsigned num_hidden_; ///< Number of hidden layers (1-6)
    unsigned num_layers_; ///< = num_hidden_ + 2

    std::shared_ptr<IConfig> config_;
};

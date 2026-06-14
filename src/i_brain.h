#pragma once
#include "genome.h"

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

struct BrainConnectionActivity {
    unsigned from_layer = 0;
    unsigned from_neuron = 0;
    unsigned to_layer = 0;
    unsigned to_neuron = 0;
    float weight = 0.0f;
    float activity_score = 0.0f;
};

/// @brief Pure interface for a feed-forward neural network brain.
///
/// Architecture (feedforward, H hidden layers):
///   h[0]   = sensor values
///   h[k]   = σ( Σ_{j<k} M[j,k] · h[j] )   for k = 1..H   (hidden layers)
///   h[H+1] = σ( Σ_{j<H+1} M[j,H+1] · h[j] ) (output layer)
///   action = argmax( h[H+1] )
///
/// Skip connections (src layer j → any deeper layer k > j) are supported.
/// Gene type encoding: 0=SENSOR, 1..H=HIDDEN_1..H, 7=OUTPUT (always 7).
/// Weight matrices are built from a Genome via wire().
/// Call resize() to change network dimensions at runtime (clears all weights).
/// Call serialize() to persist the current weights to a JSON snapshot.
class IBrain {
  public:
    virtual ~IBrain() = default;

    /// @brief Populates weight matrices W, V, D from a genome.
    virtual void wire(const Genome& genome) = 0;

    /// @brief Runs a forward pass.
    /// @param sensor_values  Input vector (clipped/padded to brain_size_s).
    /// @return Index of the highest-activated output, or -1 if no output fires.
    virtual int feed_forward(const std::vector<float>& sensor_values) = 0;

    virtual void hebbian_update(float reward) = 0;

    /// @brief Returns the genome rebuilt from the current learned weights plus preserved silent genes.
    /// Returns nullptr if hebbian_inheritance is disabled or wire() has not yet been called.
    virtual std::shared_ptr<const Genome> get_learned_genome() const = 0;

    /// @brief Resizes the network and resets all weights.
    virtual void resize(unsigned size_s, unsigned size_n, unsigned size_y, unsigned num_hidden) = 0;

    /// @brief Writes a JSON snapshot of the current weights to disk.
    /// @param pop_id      Identifier embedded in the filename and JSON metadata.
    /// @param generation  Simulation generation number stored in the metadata.
    virtual void serialize(const std::string& pop_id, unsigned generation) const = 0;

    /// @brief Removes the serialized brain file for a given pop_id.
    virtual bool remove_serialization(const std::string& pop_id) = 0;

    virtual unsigned get_size_s() const = 0;
    virtual unsigned get_size_n() const = 0;
    virtual unsigned get_size_y() const = 0;
    virtual unsigned get_num_hidden() const = 0;

    virtual std::size_t get_total_connections() const = 0;
    virtual std::size_t get_useful_connection_count(float epsilon) const = 0;
    virtual std::vector<BrainConnectionActivity> get_top_active_connections(float epsilon, std::size_t limit) const = 0;
    /// Returns true[i] iff sensor index i has at least one outgoing connection.
    virtual std::vector<bool> get_connected_sensors() const = 0;
};

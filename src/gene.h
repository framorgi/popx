#pragma once

#include <cstdint>

class Gene {
  public:
    // Constructors
    Gene();
    Gene(uint16_t src_type, uint16_t src_num, uint16_t snk_type, uint16_t snk_num, int16_t w);

    // Member functions

    //--------------------------------------------------------------------------
    /// @brief          Converts the weight to a float representation.
    /// @return         The weight as a float.
    //--------------------------------------------------------------------------
    [[nodiscard]] float get_weight_as_float() const;

    [[nodiscard]] uint16_t get_source_type() const;
    [[nodiscard]] uint16_t get_source_num() const;
    [[nodiscard]] uint16_t get_sink_type() const;
    [[nodiscard]] uint16_t get_sink_num() const;
    [[nodiscard]] int16_t get_weight() const;

  private:
    ///----------------------------------------------------------------------------
    /// Generates a random gene.
    ///----------------------------------------------------------------------------
    void genetic_lottery();

    ///----------------------------------------------------------------------------
    /// Source type: 0 = SENSOR, 1 = NEURON
    ///----------------------------------------------------------------------------
    uint16_t source_type_ : 1; // SENSOR or NEURON
    ///----------------------------------------------------------------------------
    /// Source number: 0-127
    ///----------------------------------------------------------------------------
    uint16_t source_num_ : 7;
    ///----------------------------------------------------------------------------
    /// Sink type: 0 = NEURON, 1 = ACTION
    ///----------------------------------------------------------------------------

    uint16_t sink_type_ : 1; // NEURON or ACTION
    ///----------------------------------------------------------------------------
    /// Sink number: 0-127
    ///----------------------------------------------------------------------------
    uint16_t sink_num_ : 7;
    ///----------------------------------------------------------------------------
    /// Weight: -32768 to 32767
    ///----------------------------------------------------------------------------
    int16_t weight_;
};

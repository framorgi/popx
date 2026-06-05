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
    /// Source type: 0=SENSOR, 1-6=HIDDEN_1..6, 7=OUTPUT
    ///----------------------------------------------------------------------------
    uint16_t source_type_ : 3;
    ///----------------------------------------------------------------------------
    /// Source number: 0-31  (mapped via modulo to actual layer size in Brain::wire)
    ///----------------------------------------------------------------------------
    uint16_t source_num_ : 5;
    ///----------------------------------------------------------------------------
    /// Sink type: 0=SENSOR, 1-6=HIDDEN_1..6, 7=OUTPUT
    ///----------------------------------------------------------------------------
    uint16_t sink_type_ : 3;
    ///----------------------------------------------------------------------------
    /// Sink number: 0-31
    ///----------------------------------------------------------------------------
    uint16_t sink_num_ : 5;
    ///----------------------------------------------------------------------------
    /// Weight: -32768 to 32767
    ///----------------------------------------------------------------------------
    int16_t weight_;
};

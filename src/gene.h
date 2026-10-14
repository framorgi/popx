#pragma once

#include <cstdint>

/// Random endpoint domains: counts, not maximum indices. Legacy standalone defaults.
struct GeneEncoding {
    uint32_t source_index_count = 32;
    uint32_t sink_index_count = 32;
    void validate() const;
};

class Gene {
  public:
    // Constructors
    Gene();
    explicit Gene(const GeneEncoding& encoding);
    Gene(uint16_t src_type, uint32_t src_num, uint16_t snk_type, uint32_t snk_num, int16_t w);

    // Member functions

    //--------------------------------------------------------------------------
    /// @brief          Converts the weight to a float representation.
    /// @return         The weight as a float.
    //--------------------------------------------------------------------------
    [[nodiscard]] float get_weight_as_float() const;

    [[nodiscard]] uint16_t get_source_type() const;
    [[nodiscard]] uint32_t get_source_num() const;
    [[nodiscard]] uint16_t get_sink_type() const;
    [[nodiscard]] uint32_t get_sink_num() const;
    [[nodiscard]] int16_t get_weight() const;

  private:
    ///----------------------------------------------------------------------------
    /// Generates a random gene.
    ///----------------------------------------------------------------------------
    void genetic_lottery(const GeneEncoding& encoding);

    ///----------------------------------------------------------------------------
    /// Source type: 0=SENSOR, 1-6=HIDDEN_1..6, 7=OUTPUT
    ///----------------------------------------------------------------------------
    uint16_t source_type_ : 3;
    ///----------------------------------------------------------------------------
    /// Full index; configurable generation domain. Brain rejects overflow without modulo.
    ///----------------------------------------------------------------------------
    uint32_t source_num_;
    ///----------------------------------------------------------------------------
    /// Sink type: 0=SENSOR, 1-6=HIDDEN_1..6, 7=OUTPUT
    ///----------------------------------------------------------------------------
    uint16_t sink_type_ : 3;
    ///----------------------------------------------------------------------------
    /// Full destination index, with the same representation as source indices.
    ///----------------------------------------------------------------------------
    uint32_t sink_num_;
    ///----------------------------------------------------------------------------
    /// Weight: -32768 to 32767
    ///----------------------------------------------------------------------------
    int16_t weight_;
};

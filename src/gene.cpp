#include "gene.h"

#include "random_utility.h"

#include <limits>
#include <stdexcept>

void GeneEncoding::validate() const {
    const auto max_count = static_cast<uint32_t>(std::numeric_limits<int>::max());
    if (source_index_count == 0 || sink_index_count == 0 || source_index_count > max_count ||
        sink_index_count > max_count)
        throw std::invalid_argument("Gene endpoint counts must be in [1, INT_MAX]");
}

// Default constructor
Gene::Gene() : Gene(GeneEncoding{}) {}

Gene::Gene(const GeneEncoding& encoding) : source_type_(0), source_num_(0), sink_type_(0), sink_num_(0), weight_(0) {
    genetic_lottery(encoding);
}

// Parameterized constructor
Gene::Gene(uint16_t src_type, uint32_t src_num, uint16_t snk_type, uint32_t snk_num, int16_t w)
    : source_type_(src_type), source_num_(src_num), sink_type_(snk_type), sink_num_(snk_num), weight_(w) {
    if (src_type > 7 || snk_type > 7)
        throw std::invalid_argument("Gene layer types must be in [0, 7]");
}

// Convert weight to float
float Gene::get_weight_as_float() const {
    return weight_ / 8192.0f;
}

// Getter implementations
uint16_t Gene::get_source_type() const {
    return source_type_;
}

uint32_t Gene::get_source_num() const {
    return source_num_;
}

uint16_t Gene::get_sink_type() const {
    return sink_type_;
}

uint32_t Gene::get_sink_num() const {
    return sink_num_;
}

int16_t Gene::get_weight() const {
    return weight_;
}

// Generate a random weight
void Gene::genetic_lottery(const GeneEncoding& encoding) {
    encoding.validate();
    RandomUtility rand;
    // weight range: [-4.0, +4.0]
    weight_ = rand.rnd_int(0, 0xffff) - 0x8000;
    source_type_ = rand.rnd_int(0, 7);
    source_num_ = rand.rnd_int(0, static_cast<int>(encoding.source_index_count - 1));
    sink_type_ = rand.rnd_int(0, 7);
    sink_num_ = rand.rnd_int(0, static_cast<int>(encoding.sink_index_count - 1));
}

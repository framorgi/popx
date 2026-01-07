#include "gene.h"

#include "random_utility.h"

// Default constructor
Gene::Gene() : source_type_(0), source_num_(0), sink_type_(0), sink_num_(0), weight_(0) {
    genetic_lottery();
}

// Parameterized constructor
Gene::Gene(uint16_t src_type, uint16_t src_num, uint16_t snk_type, uint16_t snk_num, int16_t w)
    : source_type_(src_type), source_num_(src_num), sink_type_(snk_type), sink_num_(snk_num), weight_(w) {}

// Convert weight to float
float Gene::get_weight_as_float() const {
    return weight_ / 8192.0f;
}

// Getter implementations
uint16_t Gene::get_source_type() const {
    return source_type_;
}

uint16_t Gene::get_source_num() const {
    return source_num_;
}

uint16_t Gene::get_sink_type() const {
    return sink_type_;
}

uint16_t Gene::get_sink_num() const {
    return sink_num_;
}

int16_t Gene::get_weight() const {
    return weight_;
}

// Generate a random weight
void Gene::genetic_lottery() {
    RandomUtility rand;
    weight_ = rand.rnd_int(0, 0xffff) - 0x8000;
    source_type_ = rand.rnd_int(0, 1);
    source_num_ = rand.rnd_int(0, 127);
    sink_type_ = rand.rnd_int(0, 1);
    sink_num_ = rand.rnd_int(0, 127);
}

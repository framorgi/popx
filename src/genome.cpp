#include "genome.h"

#include "random_utility.h"

#include <cstdint>

Genome::Genome() : Genome(RandomUtility().rnd_int(0, 50)) {}

Genome::Genome(int size) {
    genome_size_ = size;
    for (int i = 0; i < genome_size_; ++i) {
        Gene gene;
        genes_.push_back(gene);
    }
    compute_genetic_color_value();
}

IGraphicEngine::Color Genome::get_genetic_color() const {
    return genetic_color_value_;
}

void Genome::add_gene(const Gene& gene) {
    genes_.push_back(gene);
}
const std::vector<Gene>& Genome ::get_genes() const {
    return genes_;
}

void Genome::compute_genetic_color_value() {
    uint8_t r = 0;
    uint8_t g = 0;
    uint8_t b = 0;

    // R: struttura
    r |= (genes_.size() & 1) << 7;
    r |= (genes_.front().get_source_type() & 1) << 6;
    r |= (genes_.back().get_source_type() & 1) << 5;

    // G: sink/source type
    g |= (genes_.front().get_sink_type() & 1) << 7;
    g |= (genes_.back().get_sink_type() & 1) << 6;

    // B: numeri
    b |= (genes_.front().get_source_num() & 3) << 6; // usa più bit
    b |= (genes_.back().get_sink_num() & 3) << 4;

    genetic_color_value_ = {r, g, b};
}
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

Color Genome::get_genetic_color() const {
    return genetic_color_value_;
}

void Genome::add_gene(const Gene& gene) {
    genes_.push_back(gene);
}
const std::vector<Gene>& Genome ::get_genes() const {
    return genes_;
}

Genome Genome::mutated(double point_mutation_rate) const {
    Genome offspring = *this;
    RandomUtility rand;
    for (auto& gene : offspring.genes_) {
        if (rand.rnd_double(0.0, 1.0) < point_mutation_rate) {
            gene = Gene{};
        }
    }
    offspring.compute_genetic_color_value();
    return offspring;
}

void Genome::compute_genetic_color_value() {
    if (genes_.empty()) {
        genetic_color_value_ = {0, 0, 0};
        return;
    }

    uint8_t r = 0;
    uint8_t g = 0;
    uint8_t b = 0;

    // R: struttura
    r |= (genes_.size() & 1) << 7;
    r |= (genes_.front().get_source_type() & 7) << 4;
    r |= (genes_.back().get_source_type() & 7) << 1;

    // G: sink/source type
    g |= (genes_.front().get_sink_type() & 7) << 5;
    g |= (genes_.back().get_sink_type() & 7) << 2;

    // B: numeri
    b |= (genes_.front().get_source_num() & 15) << 4;
    b |= (genes_.back().get_sink_num() & 15);

    genetic_color_value_ = {r, g, b};
}
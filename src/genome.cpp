#include "genome.h"

#include "i_config.h"
#include "random_utility.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <stdexcept>

Genome::Genome() : Genome(RandomUtility().rnd_int(0, 50)) {}

Genome::Genome(int size) : Genome(size, GeneEncoding{}) {}

Genome::Genome(int size, GeneEncoding encoding) : encoding_(encoding) {
    encoding_.validate();
    if (size < 0)
        throw std::invalid_argument("Genome size cannot be negative");
    genes_.reserve(static_cast<std::size_t>(size));
    for (int i = 0; i < size; ++i) {
        Gene gene(encoding_);
        genes_.push_back(gene);
    }
    compute_genetic_color_value();
}

GeneEncoding Genome::encoding_from_config(const IConfig& config) {
    GeneEncoding encoding{config.get_genome_source_index_count(), config.get_genome_sink_index_count()};
    if (encoding.source_index_count == 0)
        encoding.source_index_count = std::max(config.get_brain_size_s(), config.get_brain_size_n());
    if (encoding.sink_index_count == 0)
        encoding.sink_index_count = std::max(config.get_brain_size_y(), config.get_brain_size_n());
    encoding.validate();
    return encoding;
}

const GeneEncoding& Genome::get_encoding() const {
    return encoding_;
}

Color Genome::get_genetic_color() const {
    return genetic_color_value_;
}

void Genome::add_gene(const Gene& gene) {
    genes_.push_back(gene);
    compute_genetic_color_value();
}
const std::vector<Gene>& Genome ::get_genes() const {
    return genes_;
}

Genome Genome::mutated(double point_mutation_rate) const {
    return mutated(point_mutation_rate, encoding_);
}

Genome Genome::mutated(double point_mutation_rate, GeneEncoding encoding) const {
    encoding.validate();
    if (!std::isfinite(point_mutation_rate) || point_mutation_rate < 0 || point_mutation_rate > 1)
        throw std::invalid_argument("Mutation rate must be finite and in [0, 1]");
    Genome offspring = *this;
    offspring.encoding_ = encoding;
    RandomUtility rand;
    for (auto& gene : offspring.genes_) {
        if (rand.rnd_double(0.0, 1.0) < point_mutation_rate) {
            gene = Gene(encoding);
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

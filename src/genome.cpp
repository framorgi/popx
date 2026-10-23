#include "genome.h"

#include "i_config.h"
#include "random_utility.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <sstream>
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

Genome::Genome(std::vector<Gene> genes, GeneEncoding encoding) : genes_(std::move(genes)), encoding_(encoding) {
    encoding_.validate();
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

Genome Genome::mutated(double point_mutation_rate, GeneEncoding encoding, double weight_mutation_rate,
                       unsigned weight_mutation_step) const {
    encoding.validate();
    if (!std::isfinite(point_mutation_rate) || point_mutation_rate < 0 || point_mutation_rate > 1)
        throw std::invalid_argument("Mutation rate must be finite and in [0, 1]");
    if (!std::isfinite(weight_mutation_rate) || weight_mutation_rate < 0 || weight_mutation_rate > 1 ||
        weight_mutation_step == 0 || weight_mutation_step > 32767)
        throw std::invalid_argument("Invalid local weight mutation parameters");
    Genome offspring = *this;
    offspring.encoding_ = encoding;
    RandomUtility rand;
    for (auto& gene : offspring.genes_) {
        if (rand.rnd_double(0.0, 1.0) < point_mutation_rate) {
            gene = Gene(encoding);
        } else if (weight_mutation_rate > 0 && rand.rnd_double(0.0, 1.0) < weight_mutation_rate) {
            int delta = rand.rnd_int(1, static_cast<int>(weight_mutation_step));
            if (rand.rnd_int(0, 1) == 0)
                delta = -delta;
            const int weight = std::clamp(static_cast<int>(gene.get_weight()) + delta, -32768, 32767);
            gene = Gene(gene.get_source_type(), gene.get_source_num(), gene.get_sink_type(), gene.get_sink_num(),
                        static_cast<int16_t>(weight));
        }
    }
    offspring.compute_genetic_color_value();
    return offspring;
}

void Genome::compute_genetic_color_value() {
    canonical_genes_.clear();
    for (const auto& g : genes_)
        canonical_genes_.push_back(
            {g.get_source_type(), g.get_source_num(), g.get_sink_type(), g.get_sink_num(), g.get_weight()});
    std::sort(canonical_genes_.begin(), canonical_genes_.end());
    uint64_t signature = 14695981039346656037ull;
    auto append = [&](uint32_t value) {
        for (unsigned shift = 0; shift < 32; shift += 8)
            signature = (signature ^ ((value >> shift) & 255)) * 1099511628211ull;
    };
    append(static_cast<uint32_t>(genes_.size()));
    for (const auto& g : canonical_genes_)
        for (const auto value : g)
            append(static_cast<uint32_t>(value));
    std::ostringstream text;
    text << std::hex << std::setw(16) << std::setfill('0') << signature;
    fingerprint_ = text.str();
    if (genes_.empty()) {
        genetic_color_value_ = {0, 0, 0};
        return;
    }

    std::array<int64_t, 3> sums{};
    for (const auto& gene : canonical_genes_) {
        uint32_t h = 2166136261u;
        for (unsigned field = 0; field < 4; ++field)
            for (unsigned shift = 0; shift < 32; shift += 8)
                h = (h ^ ((static_cast<uint32_t>(gene[field]) >> shift) & 255)) * 16777619u;
        for (unsigned c = 0; c < 3; ++c) {
            sums[c] += (h & (1u << c) ? 1 : -1) * 98304;
            sums[c] += (h & (1u << (c + 8)) ? 1 : -1) * gene[4];
        }
    }
    const double scale = 131072.0 * std::sqrt(static_cast<double>(genes_.size()));
    auto channel = [&](unsigned c) {
        return static_cast<uint8_t>(std::lround(144 + 80 * std::clamp(sums[c] / scale, -1.0, 1.0)));
    };
    genetic_color_value_ = {channel(0), channel(1), channel(2)};
}

double Genome::similarity(const Genome& other) const {
    const auto& a = canonical_genes_;
    const auto& b = other.canonical_genes_;
    if (a.empty() && b.empty())
        return 1.0;
    std::size_t i = 0, j = 0;
    double score = 0;
    while (i < a.size() && j < b.size()) {
        const bool same = std::equal(a[i].begin(), a[i].begin() + 4, b[j].begin());
        if (same) {
            score += 1.0 - std::abs(a[i][4] - b[j][4]) / 65535.0;
            ++i;
            ++j;
        } else if (std::lexicographical_compare(a[i].begin(), a[i].begin() + 4, b[j].begin(), b[j].begin() + 4))
            ++i;
        else
            ++j;
    }
    return score / std::max(a.size(), b.size());
}

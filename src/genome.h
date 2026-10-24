
#pragma once

#include "color.h"
#include "gene.h"

#include <array>
#include <string>
#include <vector>
class IConfig;
/// -----------------------------------------------------------------------------
/// @class Genome
/// @brief Represents the genetic makeup of an entity, consisting of multiple genes.
/// -----------------------------------------------------------------------------
class Genome {
  public:
    ///----------------------------------------------------------------------------
    /// @brief Constructor for the Genome class. Sizes of genome is defined by a random value.
    ///----------------------------------------------------------------------------
    Genome();
    ///----------------------------------------------------------------------------
    /// @brief Constructor for the Genome class with specified size.
    /// @param size The size of the genome (number of genes).
    ///----------------------------------------------------------------------------
    Genome(int size);
    Genome(int size, GeneEncoding encoding);
    Genome(std::vector<Gene> genes, GeneEncoding encoding);
    [[nodiscard]] static GeneEncoding encoding_from_config(const IConfig& config);
    [[nodiscard]] const GeneEncoding& get_encoding() const;
    ///----------------------------------------------------------------------------
    /// @brief Retrieves the genetic color for visualization purposes.
    /// @return The genetic color.
    ///----------------------------------------------------------------------------
    [[nodiscard]] Color get_genetic_color() const;
    /// @brief Birth-genome similarity in [0,1], invariant to ordering and preserving duplicate genes.
    [[nodiscard]] double similarity(const Genome& other) const;
    [[nodiscard]] const std::string& fingerprint() const {
        return fingerprint_;
    }
    ///----------------------------------------------------------------------------
    /// @brief Adds a gene to the genome.
    /// @param gene The gene to be added.
    ///----------------------------------------------------------------------------
    void add_gene(const Gene& gene);

    ///----------------------------------------------------------------------------
    /// @brief Retrieves the genes in the genome.
    /// @return A constant reference to the vector of genes.
    ///----------------------------------------------------------------------------
    [[nodiscard]] const std::vector<Gene>& get_genes() const;

    ///----------------------------------------------------------------------------
    /// @brief Returns a copy of this genome with random point mutations applied.
    /// @param point_mutation_rate Probability [0,1] of each gene being replaced by a new random gene.
    ///----------------------------------------------------------------------------
    [[nodiscard]] Genome mutated(double point_mutation_rate) const;
    [[nodiscard]] Genome mutated(double point_mutation_rate, GeneEncoding encoding, double weight_mutation_rate = 0,
                                 unsigned weight_mutation_step = 256, const IConfig* active_config = nullptr) const;

  private:
    ///----------------------------------------------------------------------------
    /// @brief          Computes the genetic color value based on the genes.
    ///----------------------------------------------------------------------------
    void compute_genetic_color_value();
    //--------------------------------------------------------------------------
    /// @brief          Vector of genes that make up the genome.
    //--------------------------------------------------------------------------
    std::vector<Gene> genes_;
    std::vector<std::array<int64_t, 5>> canonical_genes_;
    std::string fingerprint_;
    GeneEncoding encoding_;
    //--------------------------------------------------------------------------
    /// @brief          Genetic color value for visualization purposes.
    //--------------------------------------------------------------------------
    Color genetic_color_value_ = {0, 0, 0};
};


#pragma once

#include "gene.h"
#include "i_graphic_engine.h"

#include <vector>
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
    ///----------------------------------------------------------------------------
    /// @brief Retrieves the genetic color for visualization purposes.
    /// @return The genetic color.
    ///----------------------------------------------------------------------------
    [[nodiscard]] IGraphicEngine::Color get_genetic_color() const;
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

  private:
    ///----------------------------------------------------------------------------
    /// @brief          Computes the genetic color value based on the genes.
    ///----------------------------------------------------------------------------
    void compute_genetic_color_value();
    //--------------------------------------------------------------------------
    /// @brief          Vector of genes that make up the genome.
    //--------------------------------------------------------------------------
    std::vector<Gene> genes_;
    //--------------------------------------------------------------------------
    /// @brief          Genetic color value for visualization purposes.
    //--------------------------------------------------------------------------
    IGraphicEngine::Color genetic_color_value_ = {0, 0, 0};

    //--------------------------------------------------------------------------
    /// @brief          Size of the genome (number of genes, so the number of connections).
    //--------------------------------------------------------------------------
    int genome_size_ = 0;
};
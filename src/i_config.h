#pragma once
#include <string>

/// @brief Interface for the application configuration system.
/// Provides read access to simulation parameters loaded from an INI file.
/// Call update_from_file() once per generation to pick up live edits.
class IConfig {
  public:
    virtual ~IConfig() = default;

    // --- Brain (neural network) layer sizes ----------------------------------
    virtual unsigned get_brain_size_s() const = 0;            ///< Sensor (input) layer
    virtual unsigned get_brain_size_n() const = 0;            ///< Hidden layer
    virtual unsigned get_brain_size_y() const = 0;            ///< Output layer
    virtual unsigned get_brain_num_hidden_layers() const = 0; ///< Number of hidden layers (1-6)

    // --- I/O directories -----------------------------------------------------
    virtual const std::string& get_nnets_dir() const = 0;

    // --- Simulation parameters -----------------------------------------------
    virtual unsigned get_population() const = 0;
    virtual unsigned get_genome_min_length() const = 0;
    virtual unsigned get_genome_max_length() const = 0;
    virtual unsigned get_steps_per_generation() const = 0;
    virtual unsigned get_max_generations() const = 0;
    virtual unsigned get_num_threads() const = 0;

    // --- Genetics / mutation -------------------------------------------------
    virtual double get_point_mutation_rate() const = 0;
    virtual double get_gene_insertion_deletion_rate() const = 0;
    virtual double get_deletion_ratio() const = 0;
    virtual bool get_sexual_reproduction() const = 0;
    virtual bool get_choose_parents_by_fitness() const = 0;

    // --- Additional I/O directories ------------------------------------------
    virtual const std::string& get_log_dir() const = 0;
    virtual const std::string& get_image_dir() const = 0;

    // --- Runtime reload -------------------------------------------------------
    /// Re-reads the INI file. Parameters annotated with @N only activate from
    /// generation N onwards (same convention as the legacy param system).
    virtual void update_from_file(unsigned generation) = 0;
};

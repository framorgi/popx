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
    virtual void set_point_mutation_rate(double rate) = 0;
    // --- Simulation parameters -----------------------------------------------
    virtual unsigned get_start_population() const = 0;
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
    virtual bool get_hebbian_inheritance() const = 0;

    // --- Additional I/O directories ------------------------------------------
    virtual const std::string& get_log_dir() const = 0;
    virtual const std::string& get_image_dir() const = 0;

    // --- Physiology ----------------------------------------------------------
    virtual unsigned get_phy_init_mitochondrions_max() const = 0;
    virtual unsigned get_phy_init_chloroplasts_max() const = 0;
    virtual unsigned get_phy_init_sensitiveness_max() const = 0;
    virtual unsigned get_phy_adipose_stock_max() const = 0;
    virtual unsigned get_phy_min_chloroplasts() const = 0;
    virtual unsigned get_phy_max_chloroplasts_ref() const = 0;
    virtual double get_phy_energy_per_respiration() const = 0;
    virtual double get_phy_heat_per_respiration() const = 0;
    virtual double get_phy_alpha_min() const = 0;
    virtual double get_phy_alpha_max() const = 0;
    virtual double get_phy_max_lipids_ref() const = 0;
    virtual double get_phy_init_temperature() const = 0;
    virtual double get_phy_photo_min_elevation() const = 0;

    // --- New generation controls ---------------------------------------------
    virtual bool get_newgen_enabled() const = 0;
    virtual unsigned get_newgen_interval_ticks() const = 0;
    virtual double get_newgen_survival_ratio() const = 0;
    virtual unsigned get_newgen_min_survivors() const = 0;

    // --- Runtime reload -------------------------------------------------------
    /// Re-reads the INI file. Parameters annotated with @N only activate from
    /// generation N onwards (same convention as the legacy param system).
    virtual void update_from_file(unsigned generation) = 0;
};

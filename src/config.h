#pragma once
#include "i_config.h"

#include <filesystem>
#include <string>

struct ConfigParams {
    // Brain layer sizes
    unsigned brain_size_s = 5;
    unsigned brain_size_n = 5;
    unsigned brain_size_y = 5;
    unsigned brain_num_hidden_layers = 1;

    // Simulation
    unsigned population = 100;
    unsigned steps_per_generation = 300;
    unsigned max_generations = 200000;
    unsigned num_threads = 4;

    // Genome
    unsigned genome_min_length = 24;
    unsigned genome_max_length = 24;

    // Mutation / reproduction
    double point_mutation_rate = 0.001;
    double gene_insertion_deletion_rate = 0.0;
    double deletion_ratio = 0.5;
    bool sexual_reproduction = true;
    bool choose_parents_by_fitness = true;
    bool hebbian_inheritance = false;

    // Directories
    std::string nnets_dir = "../data_out/nnets/";
    std::string log_dir = "../data_out/logs/";
    std::string image_dir = "../data_out/images/";

    // Internal
    unsigned param_change_generation = 0;

    // Physiology
    unsigned phy_init_mitochondrions_max = 3;
    unsigned phy_init_chloroplasts_max = 2;
    unsigned phy_init_sensitiveness_max = 6;
    unsigned phy_adipose_stock_max = 80;
    unsigned phy_min_chloroplasts = 1;
    unsigned phy_max_chloroplasts_ref = 3;
    double phy_energy_per_respiration = 0.2;
    double phy_heat_per_respiration = 0.3;
    double phy_alpha_min = 0.01;
    double phy_alpha_max = 0.5;
    double phy_max_lipids_ref = 80.0;
    double phy_init_temperature = 25.0;
    double phy_photo_min_elevation = 30.0;

    // New generation controls
    bool newgen_enabled = false;
    unsigned newgen_interval_ticks = 5000;
    double newgen_survival_ratio = 0.35;
    unsigned newgen_min_survivors = 8;
};

/// @brief Concrete implementation of IConfig backed by a key=value INI file.
/// Supports '#' line comments and per-generation overrides with the '@N' suffix
/// (e.g. `brain_size_n@500 = 8` activates from generation 500 onwards).
class Config : public IConfig {
  public:
    explicit Config(const std::string& filepath);
    void set_point_mutation_rate(double rate) override;
    unsigned get_brain_size_s() const override;
    unsigned get_brain_size_n() const override;
    unsigned get_brain_size_y() const override;
    unsigned get_brain_num_hidden_layers() const override;
    const std::string& get_nnets_dir() const override;
    unsigned get_start_population() const override;
    unsigned get_genome_min_length() const override;
    unsigned get_genome_max_length() const override;
    unsigned get_steps_per_generation() const override;
    unsigned get_max_generations() const override;
    unsigned get_num_threads() const override;
    double get_point_mutation_rate() const override;
    double get_gene_insertion_deletion_rate() const override;
    double get_deletion_ratio() const override;
    bool get_sexual_reproduction() const override;
    bool get_choose_parents_by_fitness() const override;
    bool get_hebbian_inheritance() const override;
    const std::string& get_log_dir() const override;
    const std::string& get_image_dir() const override;

    unsigned get_phy_init_mitochondrions_max() const override;
    unsigned get_phy_init_chloroplasts_max() const override;
    unsigned get_phy_init_sensitiveness_max() const override;
    unsigned get_phy_adipose_stock_max() const override;
    unsigned get_phy_min_chloroplasts() const override;
    unsigned get_phy_max_chloroplasts_ref() const override;
    double get_phy_energy_per_respiration() const override;
    double get_phy_heat_per_respiration() const override;
    double get_phy_alpha_min() const override;
    double get_phy_alpha_max() const override;
    double get_phy_max_lipids_ref() const override;
    double get_phy_init_temperature() const override;
    double get_phy_photo_min_elevation() const override;

    bool get_newgen_enabled() const override;
    unsigned get_newgen_interval_ticks() const override;
    double get_newgen_survival_ratio() const override;
    unsigned get_newgen_min_survivors() const override;

    void update_from_file(unsigned generation) override;

  private:
    void set_defaults();
    void ingest_parameter(const std::string& name, const std::string& value);
    /// Converts a relative directory path to absolute, anchored at the
    /// directory that contains the INI file. Absolute paths are unchanged.
    void resolve_dir(std::string& dir) const;

    ConfigParams params_;
    std::string filepath_;
    std::filesystem::path config_dir_;
};

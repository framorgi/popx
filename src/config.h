#pragma once
#include "i_config.h"

#include <filesystem>
#include <nlohmann/json.hpp>
#include <string>

struct ConfigParams {
    // Brain layer sizes
    unsigned brain_size_s = 5;
    unsigned brain_size_n = 5;
    unsigned brain_size_y = 5;
    unsigned brain_num_hidden_layers = 1;

    // Simulation
    unsigned map_size = 120;
    unsigned source_count = 2;
    unsigned source_move_interval_ticks = 40;
    unsigned source_glucose_interval_ticks = 40;
    unsigned source_glucose_per_neighbor = 8;
    double max_duration_minutes = 0.0;
    double database_limit_gb = 10.0;
    unsigned gui_fps = 30;
    unsigned population = 100;
    unsigned max_population_allowed = 240;
    unsigned offspring_dispersal_radius = 1;
    unsigned steps_per_generation = 300;
    unsigned max_generations = 200000;
    unsigned num_threads = 4;

    // Genome
    unsigned genome_min_length = 24;
    unsigned genome_max_length = 24;
    unsigned genome_source_index_count = 0;
    unsigned genome_sink_index_count = 0;

    // Mutation / reproduction
    double point_mutation_rate = 0.001;
    double weight_mutation_rate = 0.05;
    unsigned weight_mutation_step = 256;
    double learning_rate = 0.06;
    double learning_trace_decay = 0.97;
    double learning_weight_decay = 0.00003;
    double learning_baseline_rate = 0.01;

    double gene_insertion_deletion_rate = 0.0;
    double deletion_ratio = 0.5;
    bool sexual_reproduction = true;
    bool choose_parents_by_fitness = true;
    bool hebbian_inheritance = false;
    bool brain_serialization_enabled = false;

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
    double phy_respiration_energy_target = 100.0;
    double phy_respiration_demand_band = 25.0;
    double phy_alpha_min = 0.01;
    double phy_alpha_max = 0.5;
    double phy_max_lipids_ref = 80.0;
    double phy_init_temperature = 25.0;
    double phy_opt_temperature = 37.0;
    double phy_photo_min_elevation = 30.0;
    double phy_efficiency_half_range_c = 10.0;

    // New generation controls
    bool newgen_enabled = false;
    unsigned newgen_interval_ticks = 5000;
    double newgen_survival_ratio = 0.35;
    unsigned newgen_min_survivors = 8;

    // Reproduction throttling
    unsigned max_reproducers_per_tick = 3;
    unsigned max_repro_backlog = 64;
    unsigned forced_repro_priority_per_tick = 1;
};

/// @brief Concrete implementation of IConfig backed by a key=value INI file.
/// Supports '#' line comments and per-generation overrides with the '@N' suffix
/// (e.g. `brain_size_n@500 = 8` activates from generation 500 onwards).
class Config : public IConfig {
  public:
    explicit Config(const std::string& filepath, bool strict = false);
    static nlohmann::json schema();
    nlohmann::json values() const;
    unsigned get_source_count() const override { return params_.source_count; }
    unsigned get_max_population_allowed() const override { return params_.max_population_allowed; }
    unsigned get_offspring_dispersal_radius() const override { return params_.offspring_dispersal_radius; }
    unsigned get_source_move_interval_ticks() const override { return params_.source_move_interval_ticks; }
    unsigned get_source_glucose_interval_ticks() const override { return params_.source_glucose_interval_ticks; }
    unsigned get_source_glucose_per_neighbor() const override { return params_.source_glucose_per_neighbor; }
    unsigned get_map_size() const override {
        return params_.map_size;
    }
    double get_max_duration_minutes() const override {
        return params_.max_duration_minutes;
    }
    unsigned get_gui_fps() const override {
        return params_.gui_fps;
    }
    const std::string& path() const {
        return filepath_;
    }
    void set_point_mutation_rate(double rate) override;
    unsigned get_brain_size_s() const override;
    unsigned get_brain_size_n() const override;
    unsigned get_brain_size_y() const override;
    unsigned get_brain_num_hidden_layers() const override;
    const std::string& get_nnets_dir() const override;
    unsigned get_start_population() const override;
    unsigned get_genome_min_length() const override;
    unsigned get_genome_max_length() const override;
    unsigned get_genome_source_index_count() const override;
    unsigned get_genome_sink_index_count() const override;
    unsigned get_steps_per_generation() const override;
    unsigned get_max_generations() const override;
    unsigned get_num_threads() const override;
    double get_point_mutation_rate() const override;
    double get_weight_mutation_rate() const override {
        return params_.weight_mutation_rate;
    }
    unsigned get_weight_mutation_step() const override {
        return params_.weight_mutation_step;
    }
    double get_learning_rate() const override {
        return params_.learning_rate;
    }
    double get_learning_trace_decay() const override {
        return params_.learning_trace_decay;
    }
    double get_learning_weight_decay() const override {
        return params_.learning_weight_decay;
    }
    double get_learning_baseline_rate() const override {
        return params_.learning_baseline_rate;
    }

    double get_gene_insertion_deletion_rate() const override;
    double get_deletion_ratio() const override;
    bool get_sexual_reproduction() const override;
    bool get_choose_parents_by_fitness() const override;
    bool get_hebbian_inheritance() const override;
    bool get_brain_serialization_enabled() const override;
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
    double get_phy_respiration_energy_target() const override;
    double get_phy_respiration_demand_band() const override;
    double get_phy_alpha_min() const override;
    double get_phy_alpha_max() const override;
    double get_phy_max_lipids_ref() const override;
    double get_phy_init_temperature() const override;
    double get_phy_opt_temperature() const override;
    double get_phy_photo_min_elevation() const override;
    double get_phy_efficiency_half_range_c() const override;

    bool get_newgen_enabled() const override;
    unsigned get_newgen_interval_ticks() const override;
    double get_newgen_survival_ratio() const override;
    unsigned get_newgen_min_survivors() const override;

    unsigned get_max_reproducers_per_tick() const override;
    unsigned get_max_repro_backlog() const override;
    unsigned get_forced_repro_priority_per_tick() const override;

    void update_from_file(unsigned generation) override;

  private:
    friend class PopXApp;
    void set_defaults();
    void ingest_parameter(const std::string& name, const std::string& value);
    /// Converts a relative directory path to absolute, anchored at the
    /// directory that contains the INI file. Absolute paths are unchanged.
    void resolve_dir(std::string& dir) const;

    bool strict_ = false;
    void validate() const;
    ConfigParams params_;
    std::string filepath_;
    std::filesystem::path config_dir_;
};

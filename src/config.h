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

    // Directories
    std::string nnets_dir = "../data_out/nnets/";
    std::string log_dir = "../data_out/logs/";
    std::string image_dir = "../data_out/images/";

    // Internal
    unsigned param_change_generation = 0;
};

/// @brief Concrete implementation of IConfig backed by a key=value INI file.
/// Supports '#' line comments and per-generation overrides with the '@N' suffix
/// (e.g. `brain_size_n@500 = 8` activates from generation 500 onwards).
class Config : public IConfig {
  public:
    explicit Config(const std::string& filepath);

    unsigned get_brain_size_s() const override;
    unsigned get_brain_size_n() const override;
    unsigned get_brain_size_y() const override;
    unsigned get_brain_num_hidden_layers() const override;
    const std::string& get_nnets_dir() const override;
    unsigned get_population() const override;
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
    const std::string& get_log_dir() const override;
    const std::string& get_image_dir() const override;

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

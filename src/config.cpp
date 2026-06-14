#include "config.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------

Config::Config(const std::string& filepath) : filepath_(filepath) {
    config_dir_ = std::filesystem::weakly_canonical(std::filesystem::path(filepath).parent_path());
    set_defaults();
    // Resolve default directory paths relative to the INI file location
    resolve_dir(params_.nnets_dir);
    resolve_dir(params_.log_dir);
    resolve_dir(params_.image_dir);
    update_from_file(0);
}

void Config::resolve_dir(std::string& dir) const {
    std::filesystem::path p(dir);
    if (p.is_relative()) {
        dir = std::filesystem::weakly_canonical(config_dir_ / p).string();
    }
    // Ensure trailing separator for consistent concatenation
    if (!dir.empty() && dir.back() != '/')
        dir += '/';
}

void Config::set_defaults() {
    params_ = ConfigParams{}; // member-wise defaults defined in the struct
}

// ---------------------------------------------------------------------------
// IConfig setters
//---------------------------------------------------------------------------

void Config::set_point_mutation_rate(double rate) {
    params_.point_mutation_rate = rate;
}

// ---------------------------------------------------------------------------
// IConfig getters
// ---------------------------------------------------------------------------

unsigned Config::get_brain_size_s() const {
    return params_.brain_size_s;
}
unsigned Config::get_brain_size_n() const {
    return params_.brain_size_n;
}
unsigned Config::get_brain_size_y() const {
    return params_.brain_size_y;
}
unsigned Config::get_brain_num_hidden_layers() const {
    return params_.brain_num_hidden_layers;
}
const std::string& Config::get_nnets_dir() const {
    return params_.nnets_dir;
}
unsigned Config::get_start_population() const {
    return params_.population;
}
unsigned Config::get_genome_min_length() const {
    return params_.genome_min_length;
}
unsigned Config::get_genome_max_length() const {
    return params_.genome_max_length;
}
unsigned Config::get_steps_per_generation() const {
    return params_.steps_per_generation;
}
unsigned Config::get_max_generations() const {
    return params_.max_generations;
}
unsigned Config::get_num_threads() const {
    return params_.num_threads;
}
double Config::get_point_mutation_rate() const {
    return params_.point_mutation_rate;
}
double Config::get_gene_insertion_deletion_rate() const {
    return params_.gene_insertion_deletion_rate;
}
double Config::get_deletion_ratio() const {
    return params_.deletion_ratio;
}
bool Config::get_sexual_reproduction() const {
    return params_.sexual_reproduction;
}
bool Config::get_choose_parents_by_fitness() const {
    return params_.choose_parents_by_fitness;
}
bool Config::get_hebbian_inheritance() const {
    return params_.hebbian_inheritance;
}
const std::string& Config::get_log_dir() const {
    return params_.log_dir;
}
const std::string& Config::get_image_dir() const {
    return params_.image_dir;
}

unsigned Config::get_phy_init_mitochondrions_max() const {
    return params_.phy_init_mitochondrions_max;
}
unsigned Config::get_phy_init_chloroplasts_max() const {
    return params_.phy_init_chloroplasts_max;
}
unsigned Config::get_phy_init_sensitiveness_max() const {
    return params_.phy_init_sensitiveness_max;
}
unsigned Config::get_phy_adipose_stock_max() const {
    return params_.phy_adipose_stock_max;
}
unsigned Config::get_phy_min_chloroplasts() const {
    return params_.phy_min_chloroplasts;
}
unsigned Config::get_phy_max_chloroplasts_ref() const {
    return params_.phy_max_chloroplasts_ref;
}
double Config::get_phy_energy_per_respiration() const {
    return params_.phy_energy_per_respiration;
}
double Config::get_phy_heat_per_respiration() const {
    return params_.phy_heat_per_respiration;
}
double Config::get_phy_alpha_min() const {
    return params_.phy_alpha_min;
}
double Config::get_phy_alpha_max() const {
    return params_.phy_alpha_max;
}
double Config::get_phy_max_lipids_ref() const {
    return params_.phy_max_lipids_ref;
}
double Config::get_phy_init_temperature() const {
    return params_.phy_init_temperature;
}
double Config::get_phy_photo_min_elevation() const {
    return params_.phy_photo_min_elevation;
}

bool Config::get_newgen_enabled() const {
    return params_.newgen_enabled;
}

unsigned Config::get_newgen_interval_ticks() const {
    return params_.newgen_interval_ticks;
}

double Config::get_newgen_survival_ratio() const {
    return params_.newgen_survival_ratio;
}

unsigned Config::get_newgen_min_survivors() const {
    return params_.newgen_min_survivors;
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

namespace {

bool is_uint(const std::string& s) {
    return !s.empty() && s.find_first_not_of("0123456789") == std::string::npos;
}

unsigned to_uint(const std::string& s) {
    return static_cast<unsigned>(std::stoul(s));
}

std::string trim(std::string s) {
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](unsigned char c) { return !std::isspace(c); }));
    s.erase(std::find_if(s.rbegin(), s.rend(), [](unsigned char c) { return !std::isspace(c); }).base(), s.end());
    return s;
}

std::string to_lower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return std::tolower(c); });
    return s;
}

bool is_double(const std::string& s) {
    std::istringstream iss(s);
    double d = 0;
    iss >> std::noskipws >> d;
    return iss.eof() && !iss.fail();
}

double to_double(const std::string& s) {
    return std::stod(s);
}

bool is_bool(const std::string& s) {
    return s == "true" || s == "false" || s == "1" || s == "0";
}

bool to_bool(const std::string& s) {
    return s == "true" || s == "1";
}

} // namespace

// ---------------------------------------------------------------------------
// INI ingestion
// ---------------------------------------------------------------------------

void Config::ingest_parameter(const std::string& raw_name, const std::string& raw_value) {
    const std::string name = to_lower(trim(raw_name));
    const std::string value = trim(raw_value);

    if (value.empty())
        return;

    const bool is_u = is_uint(value);
    const unsigned uval = is_u ? to_uint(value) : 0u;

    // Brain sizes
    if (name == "brain_size_s" && is_u && uval > 0) {
        params_.brain_size_s = uval;
        return;
    }
    if (name == "brain_size_n" && is_u && uval > 0) {
        params_.brain_size_n = uval;
        return;
    }
    if (name == "brain_size_y" && is_u && uval > 0) {
        params_.brain_size_y = uval;
        return;
    }
    if (name == "brain_num_hidden_layers" && is_u && uval >= 1 && uval <= 6) {
        params_.brain_num_hidden_layers = uval;
        return;
    }

    // Simulation
    if (name == "population" && is_u && uval > 0) {
        params_.population = uval;
        return;
    }
    if (name == "genome_min_length" && is_u && uval > 0) {
        params_.genome_min_length = uval;
        return;
    }
    if (name == "genome_max_length" && is_u && uval > 0) {
        params_.genome_max_length = uval;
        return;
    }
    if (name == "steps_per_generation" && is_u && uval > 0) {
        params_.steps_per_generation = uval;
        return;
    }
    if (name == "max_generations" && is_u && uval > 0) {
        params_.max_generations = uval;
        return;
    }
    if (name == "num_threads" && is_u && uval > 0) {
        params_.num_threads = uval;
        return;
    }

    // Mutation / reproduction (float and bool params)
    const bool is_d = is_double(value);
    const double dval = is_d ? to_double(value) : 0.0;
    const bool is_b = is_bool(value);

    if (name == "point_mutation_rate" && is_d && dval >= 0.0 && dval <= 1.0) {
        params_.point_mutation_rate = dval;
        return;
    }
    if (name == "gene_insertion_deletion_rate" && is_d && dval >= 0.0 && dval <= 1.0) {
        params_.gene_insertion_deletion_rate = dval;
        return;
    }
    if (name == "deletion_ratio" && is_d && dval >= 0.0 && dval <= 1.0) {
        params_.deletion_ratio = dval;
        return;
    }
    if (name == "sexual_reproduction" && is_b) {
        params_.sexual_reproduction = to_bool(value);
        return;
    }
    if (name == "choose_parents_by_fitness" && is_b) {
        params_.choose_parents_by_fitness = to_bool(value);
        return;
    }
    if (name == "hebbian_inheritance" && is_b) {
        params_.hebbian_inheritance = to_bool(value);
        return;
    }

    // Directories (resolve relative paths against the INI file location)
    if (name == "nnets_dir") {
        params_.nnets_dir = value;
        resolve_dir(params_.nnets_dir);
        return;
    }
    if (name == "log_dir") {
        params_.log_dir = value;
        resolve_dir(params_.log_dir);
        return;
    }
    if (name == "image_dir") {
        params_.image_dir = value;
        resolve_dir(params_.image_dir);
        return;
    }

    // Physiology (unsigned)
    if (name == "phy_init_mitochondrions_max" && is_u && uval > 0) {
        params_.phy_init_mitochondrions_max = uval;
        return;
    }
    if (name == "phy_init_chloroplasts_max" && is_u) {
        params_.phy_init_chloroplasts_max = uval;
        return;
    }
    if (name == "phy_init_sensitiveness_max" && is_u && uval > 0) {
        params_.phy_init_sensitiveness_max = uval;
        return;
    }
    if (name == "phy_adipose_stock_max" && is_u && uval > 0) {
        params_.phy_adipose_stock_max = uval;
        return;
    }
    if (name == "phy_min_chloroplasts" && is_u) {
        params_.phy_min_chloroplasts = uval;
        return;
    }
    if (name == "phy_max_chloroplasts_ref" && is_u && uval > 0) {
        params_.phy_max_chloroplasts_ref = uval;
        return;
    }

    // Physiology (double) — reuse is_d/dval already computed above
    if (name == "phy_energy_per_respiration" && is_d && dval >= 0.0) {
        params_.phy_energy_per_respiration = dval;
        return;
    }
    if (name == "phy_heat_per_respiration" && is_d && dval >= 0.0) {
        params_.phy_heat_per_respiration = dval;
        return;
    }
    if (name == "phy_alpha_min" && is_d && dval >= 0.0) {
        params_.phy_alpha_min = dval;
        return;
    }
    if (name == "phy_alpha_max" && is_d && dval >= 0.0) {
        params_.phy_alpha_max = dval;
        return;
    }
    if (name == "phy_max_lipids_ref" && is_d && dval > 0.0) {
        params_.phy_max_lipids_ref = dval;
        return;
    }
    if (name == "phy_init_temperature" && is_d) {
        params_.phy_init_temperature = dval;
        return;
    }
    if (name == "phy_photo_min_elevation" && is_d && dval >= 0.0) {
        params_.phy_photo_min_elevation = dval;
        return;
    }

    // New generation controls
    if (name == "newgen_enabled" && is_b) {
        params_.newgen_enabled = to_bool(value);
        return;
    }
    if (name == "newgen_interval_ticks" && is_u && uval > 0) {
        params_.newgen_interval_ticks = uval;
        return;
    }
    if (name == "newgen_survival_ratio" && is_d && dval > 0.0 && dval <= 1.0) {
        params_.newgen_survival_ratio = dval;
        return;
    }
    if (name == "newgen_min_survivors" && is_u && uval > 0) {
        params_.newgen_min_survivors = uval;
        return;
    }

    std::cerr << "[Config] Unknown or invalid parameter: " << name << " = " << value << '\n';
}

void Config::update_from_file(unsigned generation) {
    std::ifstream file(filepath_);
    if (!file.is_open()) {
        std::cerr << "[Config] Cannot open config file: " << filepath_ << '\n';
        return;
    }

    std::string line;
    while (std::getline(file, line)) {
        // Strip inline comments
        const auto comment_pos = line.find('#');
        if (comment_pos != std::string::npos)
            line.erase(comment_pos);

        line = trim(line);
        if (line.empty())
            continue;

        const auto eq = line.find('=');
        if (eq == std::string::npos)
            continue;

        std::string name_part = line.substr(0, eq);
        std::string value_part = line.substr(eq + 1);

        // Handle per-generation activation: key@N = value
        const auto at_pos = name_part.find('@');
        if (at_pos != std::string::npos) {
            const std::string gen_str = trim(name_part.substr(at_pos + 1));
            name_part = name_part.substr(0, at_pos);
            if (!is_uint(gen_str))
                continue;
            const unsigned active_from = to_uint(gen_str);
            if (active_from > generation)
                continue;
            if (active_from == generation)
                params_.param_change_generation = generation;
        }

        ingest_parameter(name_part, value_part);
    }
}

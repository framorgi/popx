#include "config.h"

#include "common.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------

Config::Config(const std::string& filepath, bool strict) : strict_(strict), filepath_(filepath) {
    config_dir_ = std::filesystem::weakly_canonical(std::filesystem::path(filepath).parent_path());
    set_defaults();
    // Resolve default directory paths relative to the INI file location
    resolve_dir(params_.nnets_dir);
    resolve_dir(params_.log_dir);
    resolve_dir(params_.image_dir);
    update_from_file(0);
    if (strict_)
        validate();
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
unsigned Config::get_genome_source_index_count() const {
    return params_.genome_source_index_count;
}
unsigned Config::get_genome_sink_index_count() const {
    return params_.genome_sink_index_count;
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
bool Config::get_brain_serialization_enabled() const {
    return params_.brain_serialization_enabled;
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
double Config::get_phy_respiration_energy_target() const {
    return params_.phy_respiration_energy_target;
}
double Config::get_phy_respiration_demand_band() const {
    return params_.phy_respiration_demand_band;
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
double Config::get_phy_opt_temperature() const {
    return params_.phy_opt_temperature;
}
double Config::get_phy_photo_min_elevation() const {
    return params_.phy_photo_min_elevation;
}

double Config::get_phy_efficiency_half_range_c() const {
    return params_.phy_efficiency_half_range_c;
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

unsigned Config::get_max_reproducers_per_tick() const {
    return params_.max_reproducers_per_tick;
}

unsigned Config::get_max_repro_backlog() const {
    return params_.max_repro_backlog;
}

unsigned Config::get_forced_repro_priority_per_tick() const {
    return params_.forced_repro_priority_per_tick;
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

namespace {

bool is_uint(const std::string& s) {
    return !s.empty() && s.find_first_not_of("0123456789") == std::string::npos;
}

unsigned to_uint(const std::string& s) {
    const auto value = std::stoull(s);
    if (value > std::numeric_limits<unsigned>::max())
        throw std::out_of_range("Configuration integer exceeds unsigned range");
    return static_cast<unsigned>(value);
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

nlohmann::json Config::schema() {
    return nlohmann::json::array({
        {{"name", "brain_size_s"},
         {"type", "integer"},
         {"default", 5},
         {"group", "brain"},
         {"description", "Ingressi 0..56: 24..27 densità FOOD/DANGER/MATE/HOME, 32 ultimo movimento fallito, 49..56 percorribilità"},
         {"effective", true},
         {"min", 1},
         {"max", 4096}},
        {{"name", "brain_size_n"},
         {"type", "integer"},
         {"default", 5},
         {"group", "brain"},
         {"description", "brain size n"},
         {"effective", true},
         {"min", 1},
         {"max", 4096}},
        {{"name", "brain_size_y"},
         {"type", "integer"},
         {"default", 5},
         {"group", "brain"},
         {"description", "22 outputs enable DO_NOTHING at 21; 23 also enables KILL_FORWARD at 22"},
         {"effective", true},
         {"min", 1},
         {"max", 4096}},
        {{"name", "brain_num_hidden_layers"},
         {"type", "integer"},
         {"default", 1},
         {"group", "brain"},
         {"description", "brain num hidden layers"},
         {"effective", true},
         {"min", 1},
         {"max", 6}},
        {{"name", "map_size"},
         {"type", "integer"},
         {"default", 120},
         {"group", "simulation"},
         {"description", "Cells per side of the square world"},
         {"effective", true},
         {"min", 4},
         {"max", 1024}},
        {{"name", "source_count"},
         {"type", "integer"},
         {"default", 2},
         {"group", "simulation"},
         {"description", "Physical glucose sources spawned at random free cells; applied on restart"},
         {"effective", true},
         {"min", 0},
         {"max", 32}},
        {{"name", "source_move_interval_ticks"},
         {"type", "integer"},
         {"default", 40},
         {"group", "simulation"},
         {"description", "Ticks between Source movement attempts; applied on restart"},
         {"effective", true},
         {"min", 1},
         {"max", 100000}},
        {{"name", "source_glucose_interval_ticks"},
         {"type", "integer"},
         {"default", 40},
         {"group", "simulation"},
         {"description", "Ticks between Source glucose deposits; applied on restart"},
         {"effective", true},
         {"min", 1},
         {"max", 100000}},
        {{"name", "source_glucose_per_neighbor"},
         {"type", "integer"},
         {"default", 8},
         {"group", "simulation"},
         {"description", "Units offered to each neighboring cell; overflow floods outward; applied on restart"},
         {"effective", true},
         {"min", 0},
         {"max", 500}},
        {{"name", "max_duration_minutes"},
         {"type", "number"},
         {"default", 0.0},
         {"group", "simulation"},
         {"description", "Simulated minutes; 0 means unlimited"},
         {"effective", true},
         {"min", 0},
         {"max", 1000000000.0}},
        {{"name", "database_limit_gb"},
         {"type", "number"},
         {"default", 10.0},
         {"group", "simulation"},
         {"description", "Limite database in GB decimali (SQLite + WAL + SHM): la dashboard salva e verifica un "
                         "checkpoint, elimina tutti gli storici e riparte dalla snapshot; checkpoint esterni esclusi. "
                         "Applicato al prossimo Start/Restart."},
         {"effective", true},
         {"min", 0.001},
         {"max", 1000000.0}},
        {{"name", "gui_fps"},
         {"type", "integer"},
         {"default", 30},
         {"group", "simulation"},
         {"description", "Maximum visible frames per second"},
         {"effective", true},
         {"min", 5},
         {"max", 60}},
        {{"name", "population"},
         {"type", "integer"},
         {"default", 100},
         {"group", "simulation"},
         {"description", "population"},
         {"effective", true},
         {"min", 1},
         {"max", 120}},
        {{"name", "max_population_allowed"},
         {"label", "Cap massimo dei Pop vivi"},
         {"type", "integer"},
         {"default", 240},
         {"group", "simulation"},
         {"description", "Numero massimo di Pop vivi contemporaneamente. Al raggiungimento del limite, le nuove nascite attendono capacità libera. Applicato al prossimo Start/Restart."},
         {"effective", true},
         {"min", 1},
         {"max", 10000}},
        {{"name", "offspring_dispersal_radius"},
         {"type", "integer"},
         {"default", 1},
         {"group", "simulation"},
         {"description", "Raggio massimo di deposizione dei figli: prova le corone successive solo se le precedenti non offrono spazio."},
         {"effective", true},
         {"min", 1},
         {"max", 8}},
        {{"name", "steps_per_generation"},
         {"type", "integer"},
         {"default", 300},
         {"group", "simulation"},
         {"description", "Age/oscillator scale; does not schedule births"},
         {"effective", true},
         {"min", 1},
         {"max", 4294967295}},
        {{"name", "max_generations"},
         {"type", "integer"},
         {"default", 200000},
         {"group", "simulation"},
         {"description", "max generations (legacy: no application effect)"},
         {"effective", false},
         {"min", 1},
         {"max", 4294967295}},
        {{"name", "num_threads"},
         {"type", "integer"},
         {"default", 4},
         {"group", "simulation"},
         {"description", "num threads"},
         {"effective", true},
         {"min", 1},
         {"max", 256}},
        {{"name", "genome_min_length"},
         {"type", "integer"},
         {"default", 24},
         {"group", "genome"},
         {"description", "genome min length"},
         {"effective", true},
         {"min", 1},
         {"max", 100000}},
        {{"name", "genome_max_length"},
         {"type", "integer"},
         {"default", 24},
         {"group", "genome"},
         {"description", "genome max length"},
         {"effective", true},
         {"min", 1},
         {"max", 100000}},
        {{"name", "genome_source_index_count"},
         {"type", "integer"},
         {"default", 0},
         {"group", "genome"},
         {"description", "genome source index count"},
         {"effective", true},
         {"min", 0},
         {"max", 2147483647}},
        {{"name", "genome_sink_index_count"},
         {"type", "integer"},
         {"default", 0},
         {"group", "genome"},
         {"description", "genome sink index count"},
         {"effective", true},
         {"min", 0},
         {"max", 2147483647}},
        {{"name", "weight_mutation_rate"},
         {"type", "number"},
         {"default", 0.05},
         {"group", "genome"},
         {"description", "Probabilità di ritoccare solo il peso, se il gene non è stato sostituito"},
         {"effective", true},
         {"min", 0},
         {"max", 1}},
        {{"name", "weight_mutation_step"},
         {"type", "integer"},
         {"default", 256},
         {"group", "genome"},
         {"description", "Ampiezza massima del ritocco in quanti genetici (8192 quanti = peso 1)"},
         {"effective", true},
         {"min", 1},
         {"max", 32767}},
        {{"name", "learning_rate"},
         {"type", "number"},
         {"default", 0.06},
         {"group", "learning"},
         {"description", "Intensità della plasticità modulata dal vantaggio rispetto alla ricompensa recente"},
         {"effective", true},
         {"min", 0},
         {"max", 1}},
        {{"name", "learning_trace_decay"},
         {"type", "number"},
         {"default", 0.97},
         {"group", "learning"},
         {"description", "Persistenza per tick delle tracce delle azioni eseguite; 0 = solo tick corrente"},
         {"effective", true},
         {"min", 0},
         {"max", 0.9999}},
        {{"name", "learning_weight_decay"},
         {"type", "number"},
         {"default", 0.00003},
         {"group", "learning"},
         {"description", "Decadimento per tick dell'eccesso del modulo del peso sopra 1/8192"},
         {"effective", true},
         {"min", 0},
         {"max", 0.01}},
        {{"name", "learning_baseline_rate"},
         {"type", "number"},
         {"default", 0.01},
         {"group", "learning"},
         {"description", "Velocità di aggiornamento della media recente della ricompensa"},
         {"effective", true},
         {"min", 1e-06},
         {"max", 1}},
        {{"name", "point_mutation_rate"},
         {"type", "number"},
         {"default", 0.001},
         {"group", "simulation"},
         {"description", "point mutation rate"},
         {"effective", true},
         {"min", 0},
         {"max", 1}},
        {{"name", "gene_insertion_deletion_rate"},
         {"type", "number"},
         {"default", 0.0},
         {"group", "simulation"},
         {"description", "gene insertion deletion rate (legacy: no application effect)"},
         {"effective", false},
         {"min", 0},
         {"max", 1}},
        {{"name", "deletion_ratio"},
         {"type", "number"},
         {"default", 0.5},
         {"group", "simulation"},
         {"description", "deletion ratio (legacy: no application effect)"},
         {"effective", false},
         {"min", 0},
         {"max", 1}},
        {{"name", "sexual_reproduction"},
         {"type", "boolean"},
         {"default", true},
         {"group", "simulation"},
         {"description", "sexual reproduction (legacy: no application effect)"},
         {"effective", false}},
        {{"name", "choose_parents_by_fitness"},
         {"type", "boolean"},
         {"default", true},
         {"group", "simulation"},
         {"description", "choose parents by fitness (legacy: no application effect)"},
         {"effective", false}},
        {{"name", "hebbian_inheritance"},
         {"type", "boolean"},
         {"default", false},
         {"group", "simulation"},
         {"description", "Offspring inherit learned weights"},
         {"effective", true}},
        {{"name", "brain_serialization_enabled"},
         {"type", "boolean"},
         {"default", false},
         {"group", "brain"},
         {"description", "brain serialization enabled (legacy: no application effect)"},
         {"effective", false}},
        {{"name", "nnets_dir"},
         {"type", "string"},
         {"default", "../data_out/nnets/"},
         {"group", "simulation"},
         {"description", "nnets dir"},
         {"effective", true}},
        {{"name", "log_dir"},
         {"type", "string"},
         {"default", "../data_out/logs/"},
         {"group", "simulation"},
         {"description", "log dir"},
         {"effective", true}},
        {{"name", "image_dir"},
         {"type", "string"},
         {"default", "../data_out/images/"},
         {"group", "simulation"},
         {"description", "image dir"},
         {"effective", true}},
        {{"name", "phy_init_mitochondrions_max"},
         {"type", "integer"},
         {"default", 3},
         {"group", "physiology"},
         {"description", "phy init mitochondrions max"},
         {"effective", true},
         {"min", 1},
         {"max", 4294967295}},
        {{"name", "phy_init_chloroplasts_max"},
         {"type", "integer"},
         {"default", 2},
         {"group", "physiology"},
         {"description", "phy init chloroplasts max"},
         {"effective", true},
         {"min", 0},
         {"max", 4294967295}},
        {{"name", "phy_init_sensitiveness_max"},
         {"type", "integer"},
         {"default", 6},
         {"group", "physiology"},
         {"description", "Massimo del raggio sensoriale ereditato. Il raggio effettivo è limitato a 12 celle e può crescere tramite l'azione 20."},
         {"effective", true},
         {"min", 1},
         {"max", 4294967295}},
        {{"name", "phy_adipose_stock_max"},
         {"type", "integer"},
         {"default", 80},
         {"group", "physiology"},
         {"description", "phy adipose stock max"},
         {"effective", true},
         {"min", 1},
         {"max", 4294967295}},
        {{"name", "phy_min_chloroplasts"},
         {"type", "integer"},
         {"default", 1},
         {"group", "physiology"},
         {"description", "phy min chloroplasts"},
         {"effective", true},
         {"min", 0},
         {"max", 4294967295}},
        {{"name", "phy_max_chloroplasts_ref"},
         {"type", "integer"},
         {"default", 3},
         {"group", "physiology"},
         {"description", "phy max chloroplasts ref"},
         {"effective", true},
         {"min", 1},
         {"max", 4294967295}},
        {{"name", "phy_energy_per_respiration"},
         {"type", "number"},
         {"default", 0.2},
         {"group", "physiology"},
         {"description", "phy energy per respiration"},
         {"effective", true},
         {"min", 0},
         {"max", 1000000.0}},
        {{"name", "phy_heat_per_respiration"},
         {"type", "number"},
         {"default", 0.3},
         {"group", "physiology"},
         {"description", "phy heat per respiration"},
         {"effective", true},
         {"min", 0},
         {"max", 1000000.0}},
        {{"name", "phy_respiration_energy_target"},
         {"type", "number"},
         {"default", 100.0},
         {"group", "physiology"},
         {"description", "phy respiration energy target"},
         {"effective", true},
         {"min", 1e-06},
         {"max", 400}},
        {{"name", "phy_respiration_demand_band"},
         {"type", "number"},
         {"default", 25.0},
         {"group", "physiology"},
         {"description", "phy respiration demand band"},
         {"effective", true},
         {"min", 1e-06},
         {"max", 400}},
        {{"name", "phy_alpha_min"},
         {"type", "number"},
         {"default", 0.01},
         {"group", "physiology"},
         {"description", "phy alpha min"},
         {"effective", true},
         {"min", 0},
         {"max", 1000000.0}},
        {{"name", "phy_alpha_max"},
         {"type", "number"},
         {"default", 0.5},
         {"group", "physiology"},
         {"description", "phy alpha max"},
         {"effective", true},
         {"min", 0},
         {"max", 1000000.0}},
        {{"name", "phy_max_lipids_ref"},
         {"type", "number"},
         {"default", 80.0},
         {"group", "physiology"},
         {"description", "phy max lipids ref"},
         {"effective", true},
         {"min", 0},
         {"max", 1000000.0}},
        {{"name", "phy_init_temperature"},
         {"type", "number"},
         {"default", 25.0},
         {"group", "physiology"},
         {"description", "phy init temperature"},
         {"effective", true},
         {"min", -273},
         {"max", 1000}},
        {{"name", "phy_opt_temperature"},
         {"type", "number"},
         {"default", 37.0},
         {"group", "physiology"},
         {"description", "phy opt temperature"},
         {"effective", true},
         {"min", -273},
         {"max", 1000}},
        {{"name", "phy_photo_min_elevation"},
         {"type", "number"},
         {"default", 30.0},
         {"group", "physiology"},
         {"description", "phy photo min elevation"},
         {"effective", true},
         {"min", 0},
         {"max", 1000000.0}},
        {{"name", "phy_efficiency_half_range_c"},
         {"type", "number"},
         {"default", 10.0},
         {"group", "physiology"},
         {"description", "phy efficiency half range c"},
         {"effective", true},
         {"min", 0.1},
         {"max", 100}},
        {{"name", "newgen_enabled"},
         {"type", "boolean"},
         {"default", false},
         {"group", "newgen"},
         {"description", "newgen enabled"},
         {"effective", true}},
        {{"name", "newgen_interval_ticks"},
         {"type", "integer"},
         {"default", 5000},
         {"group", "newgen"},
         {"description", "newgen interval ticks"},
         {"effective", true},
         {"min", 1},
         {"max", 4294967295}},
        {{"name", "newgen_survival_ratio"},
         {"type", "number"},
         {"default", 0.35},
         {"group", "newgen"},
         {"description", "newgen survival ratio"},
         {"effective", true},
         {"min", 0.01},
         {"max", 1}},
        {{"name", "newgen_min_survivors"},
         {"type", "integer"},
         {"default", 8},
         {"group", "newgen"},
         {"description", "newgen min survivors"},
         {"effective", true},
         {"min", 1},
         {"max", 120}},
        {{"name", "max_reproducers_per_tick"},
         {"type", "integer"},
         {"default", 3},
         {"group", "simulation"},
         {"description", "max reproducers per tick"},
         {"effective", true},
         {"min", 1},
         {"max", 32}},
        {{"name", "max_repro_backlog"},
         {"type", "integer"},
         {"default", 64},
         {"group", "simulation"},
         {"description", "max repro backlog"},
         {"effective", true},
         {"min", 8},
         {"max", 2048}},
        {{"name", "forced_repro_priority_per_tick"},
         {"type", "integer"},
         {"default", 1},
         {"group", "simulation"},
         {"description", "Legacy: forced reproduction disabled; retained for INI compatibility"},
         {"effective", false},
         {"min", 0},
         {"max", 32}},
    });
}
nlohmann::json Config::values() const {
    return {
        {"brain_size_s", params_.brain_size_s},
        {"brain_size_n", params_.brain_size_n},
        {"brain_size_y", params_.brain_size_y},
        {"brain_num_hidden_layers", params_.brain_num_hidden_layers},
        {"map_size", params_.map_size},
        {"source_count", params_.source_count},
        {"source_move_interval_ticks", params_.source_move_interval_ticks},
        {"source_glucose_interval_ticks", params_.source_glucose_interval_ticks},
        {"source_glucose_per_neighbor", params_.source_glucose_per_neighbor},
        {"max_duration_minutes", params_.max_duration_minutes},
        {"database_limit_gb", params_.database_limit_gb},
        {"gui_fps", params_.gui_fps},
        {"population", params_.population},
        {"max_population_allowed", params_.max_population_allowed},
        {"offspring_dispersal_radius", params_.offspring_dispersal_radius},
        {"steps_per_generation", params_.steps_per_generation},
        {"max_generations", params_.max_generations},
        {"num_threads", params_.num_threads},
        {"genome_min_length", params_.genome_min_length},
        {"genome_max_length", params_.genome_max_length},
        {"genome_source_index_count", params_.genome_source_index_count},
        {"genome_sink_index_count", params_.genome_sink_index_count},
        {"point_mutation_rate", params_.point_mutation_rate},
        {"weight_mutation_rate", params_.weight_mutation_rate},
        {"weight_mutation_step", params_.weight_mutation_step},
        {"learning_rate", params_.learning_rate},
        {"learning_trace_decay", params_.learning_trace_decay},
        {"learning_weight_decay", params_.learning_weight_decay},
        {"learning_baseline_rate", params_.learning_baseline_rate},

        {"gene_insertion_deletion_rate", params_.gene_insertion_deletion_rate},
        {"deletion_ratio", params_.deletion_ratio},
        {"sexual_reproduction", params_.sexual_reproduction},
        {"choose_parents_by_fitness", params_.choose_parents_by_fitness},
        {"hebbian_inheritance", params_.hebbian_inheritance},
        {"brain_serialization_enabled", params_.brain_serialization_enabled},
        {"nnets_dir", params_.nnets_dir},
        {"log_dir", params_.log_dir},
        {"image_dir", params_.image_dir},
        {"phy_init_mitochondrions_max", params_.phy_init_mitochondrions_max},
        {"phy_init_chloroplasts_max", params_.phy_init_chloroplasts_max},
        {"phy_init_sensitiveness_max", params_.phy_init_sensitiveness_max},
        {"phy_adipose_stock_max", params_.phy_adipose_stock_max},
        {"phy_min_chloroplasts", params_.phy_min_chloroplasts},
        {"phy_max_chloroplasts_ref", params_.phy_max_chloroplasts_ref},
        {"phy_energy_per_respiration", params_.phy_energy_per_respiration},
        {"phy_heat_per_respiration", params_.phy_heat_per_respiration},
        {"phy_respiration_energy_target", params_.phy_respiration_energy_target},
        {"phy_respiration_demand_band", params_.phy_respiration_demand_band},
        {"phy_alpha_min", params_.phy_alpha_min},
        {"phy_alpha_max", params_.phy_alpha_max},
        {"phy_max_lipids_ref", params_.phy_max_lipids_ref},
        {"phy_init_temperature", params_.phy_init_temperature},
        {"phy_opt_temperature", params_.phy_opt_temperature},
        {"phy_photo_min_elevation", params_.phy_photo_min_elevation},
        {"phy_efficiency_half_range_c", params_.phy_efficiency_half_range_c},
        {"newgen_enabled", params_.newgen_enabled},
        {"newgen_interval_ticks", params_.newgen_interval_ticks},
        {"newgen_survival_ratio", params_.newgen_survival_ratio},
        {"newgen_min_survivors", params_.newgen_min_survivors},
        {"max_reproducers_per_tick", params_.max_reproducers_per_tick},
        {"max_repro_backlog", params_.max_repro_backlog},
        {"forced_repro_priority_per_tick", params_.forced_repro_priority_per_tick},
    };
}
void Config::validate() const {
    if (params_.population > params_.max_population_allowed)
        throw std::invalid_argument("population must not exceed max_population_allowed");
    if (params_.genome_min_length > params_.genome_max_length)
        throw std::invalid_argument("genome_min_length must not exceed genome_max_length");
    if (params_.phy_alpha_min > params_.phy_alpha_max)
        throw std::invalid_argument("phy_alpha_min must not exceed phy_alpha_max");
    if (params_.population + params_.source_count > uint64_t(params_.map_size) * params_.map_size)
        throw std::invalid_argument("population plus sources exceeds world capacity");
    if (params_.population > uint64_t(params_.map_size) * params_.map_size)
        throw std::invalid_argument("population exceeds world capacity");
}
void Config::ingest_parameter(const std::string& raw_name, const std::string& raw_value) {
    const auto name = to_lower(trim(raw_name));
    const auto value = trim(raw_value);
    try {
        static const auto specs = schema();
        auto spec = std::find_if(specs.begin(), specs.end(), [&](const auto& x) { return x["name"] == name; });
        if (spec == specs.end())
            throw std::invalid_argument("unknown parameter");
        const auto type = spec->at("type").get<std::string>();
        if (type == "integer" && !is_uint(value))
            throw std::invalid_argument("expected unsigned integer");
        if (type == "number" && !is_double(value))
            throw std::invalid_argument("expected finite number");
        if (type == "boolean" && !is_bool(value))
            throw std::invalid_argument("expected true/false");
        if (type == "string" && value.empty())
            throw std::invalid_argument("empty path");
        if (type == "integer" || type == "number") {
            double number = std::stod(value);
            if (!std::isfinite(number) || number < spec->at("min").get<double>() ||
                number > spec->at("max").get<double>())
                throw std::invalid_argument("outside allowed range");
        }
        if (name == "brain_size_s") {
            params_.brain_size_s = to_uint(value);
            return;
        }
        if (name == "brain_size_n") {
            params_.brain_size_n = to_uint(value);
            return;
        }
        if (name == "brain_size_y") {
            params_.brain_size_y = to_uint(value);
            return;
        }
        if (name == "brain_num_hidden_layers") {
            params_.brain_num_hidden_layers = to_uint(value);
            return;
        }
        if (name == "map_size") {
            params_.map_size = to_uint(value);
            return;
        }
        if (name == "source_count") { params_.source_count = to_uint(value); return; }
        if (name == "source_move_interval_ticks") { params_.source_move_interval_ticks = to_uint(value); return; }
        if (name == "source_glucose_interval_ticks") { params_.source_glucose_interval_ticks = to_uint(value); return; }
        if (name == "source_glucose_per_neighbor") { params_.source_glucose_per_neighbor = to_uint(value); return; }
        if (name == "max_duration_minutes") {
            params_.max_duration_minutes = to_double(value);
            return;
        }
        if (name == "database_limit_gb") {
            params_.database_limit_gb = to_double(value);
            return;
        }
        if (name == "gui_fps") {
            params_.gui_fps = to_uint(value);
            return;
        }
        if (name == "population") {
            params_.population = to_uint(value);
            return;
        }
        if (name == "max_population_allowed") {
            params_.max_population_allowed = to_uint(value);
            return;
        }
        if (name == "offspring_dispersal_radius") {
            params_.offspring_dispersal_radius = to_uint(value);
            return;
        }
        if (name == "steps_per_generation") {
            params_.steps_per_generation = to_uint(value);
            return;
        }
        if (name == "max_generations") {
            params_.max_generations = to_uint(value);
            return;
        }
        if (name == "num_threads") {
            params_.num_threads = to_uint(value);
            return;
        }
        if (name == "genome_min_length") {
            params_.genome_min_length = to_uint(value);
            return;
        }
        if (name == "genome_max_length") {
            params_.genome_max_length = to_uint(value);
            return;
        }
        if (name == "genome_source_index_count") {
            params_.genome_source_index_count = to_uint(value);
            return;
        }
        if (name == "genome_sink_index_count") {
            params_.genome_sink_index_count = to_uint(value);
            return;
        }
        if (name == "weight_mutation_rate") {
            params_.weight_mutation_rate = to_double(value);
            return;
        }
        if (name == "weight_mutation_step") {
            params_.weight_mutation_step = to_uint(value);
            return;
        }
        if (name == "learning_rate") {
            params_.learning_rate = to_double(value);
            return;
        }
        if (name == "learning_trace_decay") {
            params_.learning_trace_decay = to_double(value);
            return;
        }
        if (name == "learning_weight_decay") {
            params_.learning_weight_decay = to_double(value);
            return;
        }
        if (name == "learning_baseline_rate") {
            params_.learning_baseline_rate = to_double(value);
            return;
        }
        if (name == "point_mutation_rate") {
            params_.point_mutation_rate = to_double(value);
            return;
        }
        if (name == "gene_insertion_deletion_rate") {
            params_.gene_insertion_deletion_rate = to_double(value);
            return;
        }
        if (name == "deletion_ratio") {
            params_.deletion_ratio = to_double(value);
            return;
        }
        if (name == "sexual_reproduction") {
            params_.sexual_reproduction = to_bool(value);
            return;
        }
        if (name == "choose_parents_by_fitness") {
            params_.choose_parents_by_fitness = to_bool(value);
            return;
        }
        if (name == "hebbian_inheritance") {
            params_.hebbian_inheritance = to_bool(value);
            return;
        }
        if (name == "brain_serialization_enabled") {
            params_.brain_serialization_enabled = to_bool(value);
            return;
        }
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
        if (name == "phy_init_mitochondrions_max") {
            params_.phy_init_mitochondrions_max = to_uint(value);
            return;
        }
        if (name == "phy_init_chloroplasts_max") {
            params_.phy_init_chloroplasts_max = to_uint(value);
            return;
        }
        if (name == "phy_init_sensitiveness_max") {
            params_.phy_init_sensitiveness_max = to_uint(value);
            return;
        }
        if (name == "phy_adipose_stock_max") {
            params_.phy_adipose_stock_max = to_uint(value);
            return;
        }
        if (name == "phy_min_chloroplasts") {
            params_.phy_min_chloroplasts = to_uint(value);
            return;
        }
        if (name == "phy_max_chloroplasts_ref") {
            params_.phy_max_chloroplasts_ref = to_uint(value);
            return;
        }
        if (name == "phy_energy_per_respiration") {
            params_.phy_energy_per_respiration = to_double(value);
            return;
        }
        if (name == "phy_heat_per_respiration") {
            params_.phy_heat_per_respiration = to_double(value);
            return;
        }
        if (name == "phy_respiration_energy_target") {
            params_.phy_respiration_energy_target = to_double(value);
            return;
        }
        if (name == "phy_respiration_demand_band") {
            params_.phy_respiration_demand_band = to_double(value);
            return;
        }
        if (name == "phy_alpha_min") {
            params_.phy_alpha_min = to_double(value);
            return;
        }
        if (name == "phy_alpha_max") {
            params_.phy_alpha_max = to_double(value);
            return;
        }
        if (name == "phy_max_lipids_ref") {
            params_.phy_max_lipids_ref = to_double(value);
            return;
        }
        if (name == "phy_init_temperature") {
            params_.phy_init_temperature = to_double(value);
            return;
        }
        if (name == "phy_opt_temperature") {
            params_.phy_opt_temperature = to_double(value);
            return;
        }
        if (name == "phy_photo_min_elevation") {
            params_.phy_photo_min_elevation = to_double(value);
            return;
        }
        if (name == "phy_efficiency_half_range_c") {
            params_.phy_efficiency_half_range_c = to_double(value);
            return;
        }
        if (name == "newgen_enabled") {
            params_.newgen_enabled = to_bool(value);
            return;
        }
        if (name == "newgen_interval_ticks") {
            params_.newgen_interval_ticks = to_uint(value);
            return;
        }
        if (name == "newgen_survival_ratio") {
            params_.newgen_survival_ratio = to_double(value);
            return;
        }
        if (name == "newgen_min_survivors") {
            params_.newgen_min_survivors = to_uint(value);
            return;
        }
        if (name == "max_reproducers_per_tick") {
            params_.max_reproducers_per_tick = to_uint(value);
            return;
        }
        if (name == "max_repro_backlog") {
            params_.max_repro_backlog = to_uint(value);
            return;
        }
        if (name == "forced_repro_priority_per_tick") {
            params_.forced_repro_priority_per_tick = to_uint(value);
            return;
        }
    } catch (const std::exception& error) {
        const auto message = name + " = " + value + ": " + error.what();
        if (strict_ || name == "genome_source_index_count" || name == "genome_sink_index_count")
            throw std::invalid_argument(message);
        std::cerr << "[Config] " << message << '\n';
    }
}

void Config::update_from_file(unsigned generation) {
    std::ifstream file(filepath_);
    if (!file.is_open()) {
        if (strict_)
            throw std::runtime_error("Cannot open config file: " + filepath_);
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
        if (eq == std::string::npos) {
            if (strict_)
                throw std::invalid_argument("Expected key=value: " + line);
            continue;
        }

        std::string name_part = line.substr(0, eq);
        std::string value_part = line.substr(eq + 1);

        // Handle per-generation activation: key@N = value
        const auto at_pos = name_part.find('@');
        if (at_pos != std::string::npos) {
            const std::string gen_str = trim(name_part.substr(at_pos + 1));
            name_part = name_part.substr(0, at_pos);
            if (!is_uint(gen_str)) {
                if (strict_)
                    throw std::invalid_argument("Invalid generation override");
                continue;
            }
            const unsigned active_from = to_uint(gen_str);
            if (active_from > generation) {
                if (strict_) {
                    const auto saved = params_;
                    ingest_parameter(name_part, value_part);
                    params_ = saved;
                }
                continue;
            }
            if (active_from == generation)
                params_.param_change_generation = generation;
        }

        ingest_parameter(name_part, value_part);
    }
}

// Standalone endpoint-domain and sensor-to-actuator regression checks.
#include "brain.h"
#include "config.h"
#include "console_logger.h"
#include "grid_world.h"
#include "pop.h"

#include <unistd.h>

#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <nlohmann/json.hpp>
#include <stdexcept>

namespace {
void require(bool value, const char* message) {
    if (!value)
        throw std::runtime_error(message);
}
template <class F>
void rejects(F action) {
    bool threw = false;
    try {
        action();
    } catch (const std::exception&) {
        threw = true;
    }
    require(threw, "Invalid encoding/configuration was accepted");
}
class TestConfig : public Config {
  public:
    unsigned inputs = 37, hidden = 16, outputs = 21;
    unsigned source_count = 0, sink_count = 0;
    bool serialization = false;
    std::string export_dir;
    TestConfig() : Config("/dev/null") {}
    unsigned get_brain_size_s() const override {
        return inputs;
    }
    unsigned get_brain_size_n() const override {
        return hidden;
    }
    unsigned get_brain_size_y() const override {
        return outputs;
    }
    unsigned get_genome_source_index_count() const override {
        return source_count;
    }
    unsigned get_genome_sink_index_count() const override {
        return sink_count;
    }
    bool get_hebbian_inheritance() const override {
        return true;
    }
    bool get_brain_serialization_enabled() const override {
        return serialization;
    }
    const std::string& get_nnets_dir() const override {
        return export_dir;
    }
    double get_phy_init_temperature() const override {
        return 37;
    }
};
bool same_genes(const Genome& a, const Genome& b) {
    if (a.get_genes().size() != b.get_genes().size())
        return false;
    for (unsigned i = 0; i < a.get_genes().size(); ++i) {
        auto& x = a.get_genes()[i];
        auto& y = b.get_genes()[i];
        if (x.get_source_type() != y.get_source_type() || x.get_source_num() != y.get_source_num() ||
            x.get_sink_type() != y.get_sink_type() || x.get_sink_num() != y.get_sink_num() ||
            x.get_weight() != y.get_weight())
            return false;
    }
    return true;
}
} // namespace

class SimulationAudit {
  public:
    static void check_pop(const std::shared_ptr<ILogger>& logger, const std::shared_ptr<TestConfig>& cfg) {
        cfg->inputs = 37;
        cfg->hidden = 16;
        cfg->outputs = 21;
        cfg->source_count = cfg->sink_count = 0;
        auto world = std::make_shared<GridWorld>(9, 9, logger);
        for (const auto& c : world->get_cells()) {
            c->take_glucose(MaxC6h12o6);
            c->take_calcium(MaxCaco3);
            c->set_elevation(0);
            c->set_temperature(37);
        }
        Genome g(0, Genome::encoding_from_config(*cfg));
        g.add_gene(Gene(0, static_cast<unsigned>(Sensor::GLUCOSE_DENSITY_E), 7,
                        static_cast<unsigned>(Action::MOVE_EAST), 8192));
        g.add_gene(Gene(0, static_cast<unsigned>(Sensor::GLUCOSE_DENSITY_W), 7,
                        static_cast<unsigned>(Action::MOVE_WEST), 8192));
        auto pop = std::make_shared<Pop>(world, logger, cfg, g, PhyT{1, 0, 1, 80});
        require(pop->try_spawn({4, 4}), "Pop placement failed");
        pop->lazyness_ = 0;
        for (int x = 5; x < 9; ++x)
            world->get_cell({x, 4})->set_glucose(500);
        pop->sense();
        pop->think();
        require(pop->last_action_ == static_cast<int>(Action::MOVE_EAST), "East glucose did not select east");
        pop->act();
        require(pop->pos_.x == 5 && pop->pos_.y == 4, "Glucose-driven movement was not executed");
        for (const auto& c : world->get_cells())
            c->take_glucose(MaxC6h12o6);
        for (int x = 0; x < 5; ++x)
            world->get_cell({x, 4})->set_glucose(500);
        pop->sense();
        pop->think();
        require(pop->last_action_ == static_cast<int>(Action::MOVE_WEST), "West glucose did not select west");
        pop->act();
        require(pop->pos_.x == 4, "West movement was not executed");

        cfg->set_point_mutation_rate(0);
        require(same_genes(g, pop->make_offspring_genome()), "Unmutated high-index inheritance changed genes");
        cfg->source_count = 1;
        cfg->sink_count = 1;
        cfg->set_point_mutation_rate(1);
        auto child = pop->make_offspring_genome();
        for (const auto& gene : child.get_genes())
            require(gene.get_source_num() == 0 && gene.get_sink_num() == 0, "Offspring ignored configured domains");
        auto founder = std::make_shared<Pop>(world, logger, cfg);
        for (const auto& gene : founder->genome_.get_genes())
            require(gene.get_source_num() == 0 && gene.get_sink_num() == 0, "Founder ignored configured domains");

        // Configured output 23 exposes the existing actuator at its stable index 22.
        cfg->source_count = cfg->sink_count = 0;
        cfg->outputs = 23;
        Genome lethal(0, Genome::encoding_from_config(*cfg));
        lethal.add_gene(
            Gene(0, static_cast<unsigned>(Sensor::LOC_X), 7, static_cast<unsigned>(Action::KILL_FORWARD), 8192));
        auto killer = std::make_shared<Pop>(world, logger, cfg, lethal, PhyT{1, 0, 1, 80});
        auto victim = std::make_shared<Pop>(world, logger, cfg, Genome(0), PhyT{1, 0, 1, 80});
        require(killer->try_spawn({2, 2}) && victim->try_spawn({3, 2}), "Actuator fixture placement failed");
        killer->last_direction_ = {1, 0};
        killer->sense();
        killer->think();
        killer->act();
        require(!victim->alive_, "Configured actuator 22 did not execute");
    }
};

int main() {
    try {
        Gene large(0, 65539, 7, 65541, 8192);
        require(large.get_source_num() == 65539 && large.get_sink_num() == 65541, "Index truncation");
        rejects([] { Gene g(8, 0, 7, 0, 0); });
        rejects([] { Genome g(-1); });
        rejects([] { Genome g(1, {0, 1}); });
        rejects([] { (void)Genome(0).mutated(std::numeric_limits<double>::quiet_NaN()); });

        auto cfg = std::make_shared<TestConfig>();
        auto encoding = Genome::encoding_from_config(*cfg);
        require(encoding.source_index_count == 37 && encoding.sink_index_count == 21, "Automatic domains wrong");
        cfg->inputs = 70;
        cfg->hidden = 40;
        cfg->outputs = 65;
        encoding = Genome::encoding_from_config(*cfg);
        require(encoding.source_index_count == 70 && encoding.sink_index_count == 65, "Domains still capped at 32");
        Genome sampled(256, encoding);
        auto replaced = sampled.mutated(1);
        for (const auto& gene : replaced.get_genes())
            require(gene.get_source_num() < 70 && gene.get_sink_num() < 65, "Mutation escaped domains");
        require(same_genes(sampled, sampled.mutated(0)), "Zero mutation changed genes");
        auto one = sampled.mutated(1, {1, 1});
        for (const auto& gene : one.get_genes())
            require(gene.get_source_num() == 0 && gene.get_sink_num() == 0, "Explicit mutation limits ignored");

        Genome wired(0, encoding);
        wired.add_gene(Gene(0, 64, 7, 60, 8192));
        Brain brain(cfg);
        brain.wire(wired);
        std::vector<float> input(70, 0);
        input[64] = 1;
        require(brain.feed_forward(input) == 60, "High-index input/output path unreachable");
        brain.hebbian_update(0.5f);
        auto learned = brain.get_learned_genome();
        require(learned && learned->get_genes().front().get_source_num() == 64 &&
                    learned->get_genes().front().get_sink_num() == 60,
                "Learned export truncated indices");
        Brain clone(cfg);
        clone.wire(*learned);
        require(clone.feed_forward(input) == 60, "Rebuilt genotype lost high-index action");
        cfg->inputs = 32;
        cfg->outputs = 21;
        Brain limited(cfg);
        limited.wire(wired);
        require(limited.get_total_connections() == 0, "Out-of-range endpoints were remapped");
        Genome legacy(0);
        legacy.add_gene(Gene(0, 0, 7, 4, 8192));
        limited.wire(legacy);
        std::vector<float> legacy_inputs(32, 0);
        legacy_inputs[0] = 1;
        require(limited.feed_forward(legacy_inputs) == 4, "Legacy gene changed its endpoint meaning");
        cfg->inputs = 70;
        cfg->outputs = 65;
        Brain restored(cfg);
        restored.wire(wired);
        require(restored.feed_forward(input) == 60, "Widening layers did not reactivate a silent high-index gene");
        cfg->serialization = true;
        cfg->export_dir =
            (std::filesystem::temp_directory_path() / ("popx_encoding_export_" + std::to_string(getpid()))).string() +
            "/";
        const auto snapshot = restored.snapshot();
        require(snapshot["gene_encoding"]["version"] == 2 && snapshot["gene_encoding"]["source_index_count"] == 70 &&
                    snapshot["gene_encoding"]["sink_index_count"] == 65,
                "Snapshot lost encoding domain metadata");
        require(snapshot["connections"][0]["entries"][0]["col"] == 64 &&
                    snapshot["connections"][0]["entries"][0]["row"] == 60,
                "Snapshot truncated endpoint indices");

        std::filesystem::remove(cfg->export_dir);
        cfg->serialization = false;

        const auto path =
            std::filesystem::temp_directory_path() / ("popx_encoding_" + std::to_string(getpid()) + ".ini");
        {
            std::ofstream f(path);
            f << "brain_size_s=70\nbrain_size_n=40\nbrain_size_y=65\n"
                 "genome_source_index_count=9\ngenome_sink_index_count=11\n";
        }
        Config parsed(path.string());
        auto limits = Genome::encoding_from_config(parsed);
        require(limits.source_index_count == 9 && limits.sink_index_count == 11, "INI limits not parsed");
        {
            std::ofstream f(path);
            f << "genome_source_index_count=4294967296\n";
        }
        rejects([&] { Config invalid(path.string()); });
        std::filesystem::remove(path);
        auto logger = std::make_shared<ConsoleLogger>();
        logger->set_level(LogLevel::Error);
        SimulationAudit::check_pop(logger, cfg);
        Config active("config/popx.ini");
        require(active.get_brain_size_s() >= 37 && active.get_brain_size_y() == 21, "Active I/O configuration wrong");
        std::cout << "PASS: configurable encoding, mutation, inheritance, high-index export, glucose decisions and "
                     "actuators\n";
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}

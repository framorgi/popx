#include "checkpoint.h"
#include "console_logger.h"

#include <iostream>
#include <map>
#include <stdexcept>

namespace {
void require(bool ok, const char* message) {
    if (!ok)
        throw std::runtime_error(message);
}
class RecoveryConfig : public Config {
  public:
    RecoveryConfig() : Config("/dev/null") {}
    unsigned get_map_size() const override { return 8; }
    unsigned get_source_count() const override { return 0; }
    unsigned get_brain_size_s() const override { return 2; }
    unsigned get_brain_size_n() const override { return 2; }
    unsigned get_brain_size_y() const override { return 22; }
    unsigned get_brain_num_hidden_layers() const override { return 1; }
    unsigned get_max_population_allowed() const override { return 20; }
    bool get_newgen_enabled() const override { return false; }
    bool get_hebbian_inheritance() const override { return true; }
    // Recovery must preserve archived data even when natural reproduction would mutate every gene.
    double get_point_mutation_rate() const override { return 1; }
};
}

class SimulationAudit {
  public:
    static void run() {
        auto config = std::make_shared<RecoveryConfig>();
        auto logger = std::make_shared<ConsoleLogger>();
        logger->set_level(LogLevel::Error);
        auto make_world = [&] {
            auto w = std::make_shared<GridWorld>(8, 8, logger, SourceSettings{0, 40, 40, 8});
            w->init();
            for (const auto& cell : w->get_cells()) {
                cell->take_calcium(MaxCaco3);
                cell->set_elevation(0);
            }
            return w;
        };
        auto world = make_world();
        PopsManager manager(world, logger, config);
        std::map<std::string, Genome> expected;
        for (unsigned i = 0; i < 3; ++i) {
            Genome birth({Gene(0, 0, 7, 4 + i, 8192)}, Genome::encoding_from_config(*config));
            auto donor = std::make_shared<Pop>(world, logger, config, birth, PhyT{5, 1, 2, 80});
            require(donor->try_spawn({static_cast<int>(i), 0}), "donor placement");
            donor->age_ = 1234;
            donor->offspring_count_ = 7;
            donor->energy_ = 12;
            // A controlled learned network differs from the birth genome; also include a silent high-index gene.
            Genome learned({Gene(0, 0, 7, 4 + i, 4096), Gene(0, 500, 7, 4, -2)},
                           Genome::encoding_from_config(*config));
            donor->brain_->wire(learned);
            donor->brain_->feed_forward({1, 0});
            donor->brain_->record_action(4 + i);
            donor->brain_->hebbian_update(0);
            donor->scorer_.observe({1234, 7, 5, 12, 20, 37}, *config);
            expected.emplace(donor->get_pop_id(), donor->heritable_genome());
            manager.pops_.push_back(donor);
            donor->die();
        }
        manager.update_cycle();
        require(manager.recovery_bank_.size() == 3 && manager.pops_.empty(), "deaths not archived");
        require(manager.recover_from_extinction(), "recovery failed");
        require(manager.pops_.size() == 20, "recovery target/cap changed");
        std::map<std::string, int> uses;
        for (const auto& pop : manager.pops_) {
            require(expected.contains(pop->parent_id_) && pop->birth_was_forced_, "donor provenance lost");
            require(pop->genome_.similarity(expected.at(pop->parent_id_)) == 1, "learned or silent genes lost/mutated");
            require(pop->phy_.mitochondrions == 5 && pop->phy_.chloroplasts == 1 &&
                    pop->phy_.sensitiveness == 2 && pop->phy_.adipose_stock_max == 80, "traits lost/mutated");
            require(pop->age_ == 0 && pop->offspring_count_ == 0 && pop->energy_ == 100 &&
                    pop->glucose_ == 100 && pop->water_ == 100, "recovery copied exhausted physical state");
            require(pop->scorer_.baseline() == 0 && pop->scorer_.raw_reward() == 0,
                    "recovery inherited scorer memory");
            ++uses[pop->parent_id_];
        }
        for (const auto& [id, count] : uses)
            require(count == 6 || count == 7, "donor sampling was not balanced");

        auto saved = CheckpointCodec::capture(*world, manager, "", config->values(), "fixture", 1, 0);
        for (const auto& pop : saved["pops"]) {
            require(pop["scorer"][0] == 0, "recovery inherited scorer observations");
            for (const auto& trace : pop["brain"]["eligibility"])
                for (const auto& value : trace)
                    require(value == 0, "recovery inherited neural eligibility traces");
        }
        auto restored_world = make_world();
        PopsManager restored(restored_world, logger, config);
        CheckpointCodec::restore(saved, *restored_world, restored);
        auto again = CheckpointCodec::capture(*restored_world, restored, "", config->values(), "fixture", 1, 0);
        require(saved["manager"]["recovery_bank"] == again["manager"]["recovery_bank"], "archive checkpoint mismatch");
        for (const auto& seed : restored.recovery_bank_)
            require(!seed.parent_in_run, "checkpoint donor was treated as part of new history");
        auto legacy = saved;
        legacy["manager"].erase("recovery_bank");
        auto legacy_world = make_world();
        PopsManager legacy_manager(legacy_world, logger, config);
        CheckpointCodec::restore(legacy, *legacy_world, legacy_manager);
        require(legacy_manager.recovery_bank_.empty(), "legacy checkpoint invented an archive");

        auto bad = saved;
        bad["manager"]["recovery_bank"] = nlohmann::json::array();
        for (int i = 0; i < 65; ++i)
            bad["manager"]["recovery_bank"].push_back(saved["manager"]["recovery_bank"][0]);
        bool rejected = false;
        try {
            auto bad_world = make_world();
            PopsManager bad_manager(bad_world, logger, config);
            CheckpointCodec::restore(bad, *bad_world, bad_manager);
        } catch (const std::exception&) { rejected = true; }
        require(rejected, "oversized checkpoint archive accepted");

        manager.reset_population_state();
        require(manager.recovery_bank_.empty(), "new run retained archive");
        for (int i = 0; i < 70; ++i) {
            Genome genome({Gene(0, i, 7, 4, 8192)}, Genome::encoding_from_config(*config));
            Pop donor(world, logger, config, genome, PhyT{5, 0, 2, 80});
            manager.remember_for_recovery(donor); // Isolated capacity fixture, no ecological death.
        }
        require(manager.recovery_bank_.size() == 64, "archive exceeded memory bound");
        Genome duplicate({Gene(0, 69, 7, 4, 8192)}, Genome::encoding_from_config(*config));
        Pop latest(world, logger, config, duplicate, PhyT{5, 0, 2, 80});
        manager.remember_for_recovery(latest);
        require(manager.recovery_bank_.size() == 64 && manager.recovery_bank_.back().parent_id == latest.get_pop_id(),
                "duplicate profile did not replace its earlier donor");
        manager.reset_population_state();
        require(manager.recover_from_extinction(), "empty-archive compatibility recovery failed");
        for (const auto& pop : manager.pops_)
            require(pop->parent_id_.empty() && pop->birth_was_forced_, "empty archive invented ancestry");
    }
};

int main() {
    SimulationAudit::run();
    std::cout << "PASS: hereditary extinction recovery, balanced donors, memory bound and archive checkpoints\n";
}

#include "config.h"
#include "console_logger.h"
#include "genome.h"
#include "grid_world.h"
#include "pop.h"

#include <cmath>
#include <iostream>
#include <stdexcept>

namespace {
void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
Genome genome(std::initializer_list<Gene> genes) {
    Genome result(0);
    for (const auto& gene : genes) result.add_gene(gene);
    return result;
}
class SimilarityConfig : public Config {
  public:
    SimilarityConfig() : Config("/dev/null") {}
    unsigned get_brain_size_s() const override { return 49; }
    unsigned get_brain_size_y() const override { return 22; }
};
}

class SimulationAudit {
  public:
    static void run() {
        auto config = std::make_shared<SimilarityConfig>();
        auto logger = std::make_shared<ConsoleLogger>();
        logger->set_level(LogLevel::Error);
        auto world = std::make_shared<GridWorld>(5, 5, logger);
        for (const auto& c : world->get_cells()) {
            c->take_calcium(MaxCaco3);
            c->set_elevation(0);
        }
        const auto genes = genome({Gene(0, 5, 7, 4, 8192), Gene(1, 1, 7, 0, -100)});
        auto observer = std::make_shared<Pop>(world, logger, config, genes);
        auto neighbour = std::make_shared<Pop>(world, logger, config, genes);
        require(observer->try_spawn({2,2}) && neighbour->try_spawn({3,2}), "fixture spawn");
        observer->last_direction_ = {7,0}; // Heading uses signs, not the previous movement stride.
        auto sense = [&] {
            observer->next_phase_ = State::SENSE;
            observer->sense();
            return observer->sensor_values_[5];
        };
        require(sense() == 1.0f, "identical neighbour should be recognized");
        observer->think();
        require(observer->last_action_ == static_cast<int>(Action::MOVE_EAST), "sensor did not reach decision");
        observer->last_direction_ = {-1,0};
        require(sense() == 0.0f, "empty cell should return zero");
        observer->last_direction_ = {0,0};
        require(sense() == 0.0f, "zero heading must not sense self");
        observer->last_direction_ = {1,0};
        neighbour->genome_ = genome({Gene(0,5,7,4,-8192), Gene(1,1,7,0,-100)});
        const double expected = genes.similarity(neighbour->genome_);
        require(std::abs(sense()-expected)<1e-6, "different weights ignored");
        neighbour->alive_ = false;
        require(sense() == 0.0f, "dead occupant counted");
        observer->pos_ = {4,2};
        require(sense() == 0.0f, "boundary sensing failed");
    }
};

int main() {
    const Gene a(0,0xffffffffu,7,0x8000000eu,-32768), b(1,2,7,3,32767), c(0,4,1,5,8192);
    auto first=genome({a,b,c}), reordered=genome({c,a,b}), duplicate=genome({a,b,c,c});
    require(first.similarity(reordered)==1, "order must not change similarity");
    require(first.fingerprint()==reordered.fingerprint(), "order changed fingerprint");
    require(first.similarity(duplicate)==.75 && duplicate.similarity(first)==.75, "duplicate multiplicity");
    require(Genome(0).similarity(Genome(0))==1 && first.similarity(Genome(0))==0, "empty convention");
    auto changed=genome({a,Gene(1,2,7,3,32766),c});
    require(first.similarity(changed)<1 && first.similarity(changed)>.99, "small weight mutation");
    require(first.fingerprint()!=changed.fingerprint(), "middle weight missing from signature");
    auto high=genome({Gene(0,0x7fffffffu,7,0x8000000eu,-32768),b,c});
    require(first.similarity(high)==2.0/3.0, "endpoint high bits ignored");
    const auto color=first.get_genetic_color(), other=reordered.get_genetic_color();
    require(first.fingerprint()=="f390b98f0fc55cfc" && color.r==106 && color.g==130 && color.b==89,
            "cross-language genetic vector changed");
    require(color.r==other.r && color.g==other.g && color.b==other.b, "order changed color");
    std::cout << "GENETIC_VECTOR " << first.fingerprint() << " " << color.r << " " << color.g << " " << color.b << '\n';
    SimulationAudit::run();
    std::cout << "PASS: canonical genes, duplicates, weights, full indices, color, forward sensor and decision\n";
}

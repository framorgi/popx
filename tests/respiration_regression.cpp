// Invariant and boundary checks for regulated respiration, using production physiology.
#include "config.h"
#include "console_logger.h"
#include "grid_world.h"
#include "pops_manager.h"

#include <cmath>
#include <fstream>
#include <iostream>
#include <stdexcept>

class RespirationConfig : public Config {
  public:
    RespirationConfig() : Config("config/popx.ini") {}
    double get_point_mutation_rate() const override {
        return 0.0;
    }
};
class SimulationAudit {
    static void require(bool ok, const char* message) {
        if (!ok)
            throw std::runtime_error(message);
    }

  public:
    static void run() {
        auto logger = std::make_shared<ConsoleLogger>();
        logger->set_level(LogLevel::Error);
        auto cfg = std::make_shared<RespirationConfig>();
        require(cfg->get_phy_respiration_energy_target() == 100 && cfg->get_phy_respiration_demand_band() == 25,
                "production respiration parameters not loaded");
        const std::string config_path = "/tmp/popx_respiration_parameters_regression.ini";
        {
            std::ofstream out(config_path);
            out << "phy_respiration_energy_target=80\nphy_respiration_demand_band=10\n"
                   "phy_respiration_energy_target=-1\nphy_respiration_energy_target=401\n"
                   "phy_respiration_demand_band=0\n";
        }
        Config parameters(config_path);
        std::remove(config_path.c_str());
        require(parameters.get_phy_respiration_energy_target() == 80 &&
                    parameters.get_phy_respiration_demand_band() == 10,
                "invalid respiration parameters accepted");
        auto world = std::make_shared<GridWorld>(5, 5, logger);
        for (const auto& c : world->get_cells()) {
            c->set_temperature(25);
            c->set_elevation(0);
            c->take_calcium(MaxCaco3);
            c->take_lipids(MaxLipids);
            c->take_co2(MaxCo2);
        }
        auto p =
            std::make_shared<Pop>(world, logger, cfg, Genome(0, Genome::encoding_from_config(*cfg)), PhyT{1, 0, 1, 80});
        require(p->try_spawn({2, 2}), "spawn failed");
        p->temperature_ = 37;
        p->energy_ = 100;
        p->glucose_ = p->o2_ = 100;
        p->run_mitochondrions();
        require(p->total_respiration_count_ == 0 && p->glucose_ == 100 && p->metabolism_heat_ == 0,
                "satisfied demand consumed fuel or produced heat");
        p->energy_ = 0;
        p->run_mitochondrions();
        require(p->glucose_ == 99 && p->o2_ == 99 && p->total_respiration_count_ == 1,
                "one reaction did not consume exactly one of each substrate");
        require(std::abs(p->energy_ - 0.3) < 1e-6 && std::abs(p->total_respiration_heat_ - 0.3) < 1e-6,
                "one reaction energy/heat mismatch");
        // Half-width gives half capacity and half useful yield, without a zero-yield dead zone.
        p->temperature_ = 27;
        p->energy_ = 0;
        p->respiration_credit_ = 0;
        p->run_mitochondrions();
        require(p->total_respiration_count_ == 1, "fractional capacity executed too early");
        p->run_mitochondrions();
        require(p->total_respiration_count_ == 2 && std::abs(p->energy_ - 0.15) < 1e-6,
                "thermal activity/yield not continuous at the old cutoff");
        p->temperature_ = 37;
        p->respiration_credit_ = 0;
        const auto before = p->total_respiration_count_;
        for (int i = 0; i < 8; ++i) {
            p->energy_ = 96.875f; // Controlled constant 1/8 demand for the fractional-capacity check.
            p->run_mitochondrions();
        }
        require(p->total_respiration_count_ == before + 1, "fractional demand lost or fabricated reactions");
        p->respiration_credit_ = 0.875;
        p->o2_ = 0;
        p->run_mitochondrions();
        require(p->respiration_credit_ == 0, "oxygen starvation stored deferred capacity");
        p->o2_ = 100;
        p->energy_ = 96.875f;
        p->run_mitochondrions();
        require(p->total_respiration_count_ == before + 1, "substrate refill caused catch-up burst");
        p->energy_ = 400;
        p->run_mitochondrions();
        require(p->respiration_credit_ == 0 && p->energy_ == 400, "saturation did not stop respiration");
        p->energy_ = 0;
        p->temperature_ = 65;
        p->phy_.mitochondrions = 25;
        const auto hot_before = p->total_respiration_count_;
        for (int i = 0; i < 100; ++i)
            p->run_mitochondrions();
        require(p->total_respiration_count_ - hot_before < 20, "hot body retained full throughput");
        require(p->total_respiration_count_ == hot_before, "upper thermal inhibition still consumed fuel");
        p->temperature_ = 47;
        p->respiration_credit_ = 0.9;
        p->run_mitochondrions();
        require(p->respiration_credit_ == 0 && p->total_respiration_count_ == hot_before,
                "upper boundary retained reaction credit");
        p->respiration_credit_ = 0.5;
        p->init();
        require(p->respiration_credit_ == 0 && p->total_respiration_heat_ == 0, "init retained metabolic state");
        world->remove_entity(p);

        for (unsigned mitochondria : {1u, 5u, 25u}) {
            auto q = std::make_shared<Pop>(world, logger, cfg, Genome(0, Genome::encoding_from_config(*cfg)),
                                           PhyT{mitochondria, 0, 1, 80});
            require(q->try_spawn({2, 2}), "cold fixture spawn failed");
            q->temperature_ = 25;
            q->energy_ = 25;
            q->lipids_ = 80;
            const double initial = q->energy_;
            for (int tick = 0; tick < 2000 && q->alive_; ++tick) {
                auto c = world->get_cell(q->pos_);
                c->set_glucose(500);
                c->give_o2(1000);
                q->update();
            }
            require(q->alive_ && q->energy_ > 75 && q->temperature_ > 25 && q->temperature_ < 47,
                    "continuous kinetics failed cold-start recovery in a viable supplied niche");
            const double expected = initial + q->total_respiration_energy_gain_ - q->total_metabolism_energy_loss_ -
                                    q->total_energy_cap_loss_ + q->total_energy_roundoff_;
            require(std::abs(q->energy_ - expected) < 1e-5, "energy ledger failed");
            require(std::abs(q->total_respiration_heat_ -
                             q->total_respiration_count_ *
                                 static_cast<double>(static_cast<float>(cfg->get_phy_heat_per_respiration()))) < 1e-8,
                    "heat did not track completed reactions");
            std::cout << "cold recovery M=" << mitochondria << " energy=" << q->energy_
                      << " temperature=" << q->temperature_ << '\n';
            world->remove_entity(q);
        }
        // Mature parent is explicit fixture setup; child must reach maturity via 1500 real updates.
        PopsManager manager(world, logger, cfg);
        auto parent =
            std::make_shared<Pop>(world, logger, cfg, Genome(0, Genome::encoding_from_config(*cfg)), PhyT{5, 0, 1, 80});
        require(parent->try_spawn({2, 2}), "parent spawn failed");
        parent->age_ = 1001;
        parent->energy_ = 100;
        manager.pops_.push_back(parent);
        manager.try_reproduce(parent);
        require(manager.pops_.size() == 2, "natural fixture birth failed");
        auto child = manager.pops_.back();
        require(child->energy_ < 13 && child->temperature_ == 25, "birth gained free reserve or temperature");
        for (int tick = 0; tick < 1500 && child->alive_; ++tick) {
            auto c = world->get_cell(child->pos_);
            c->set_glucose(500);
            c->give_o2(1000);
            child->update();
        }
        require(child->wants_to_reproduce(), "child failed to reach reproductive eligibility");
        manager.try_reproduce(child);
        require(manager.pops_.size() == 3 && child->offspring_count_ == 1, "mature child could not reproduce");
        std::cout << "PASS: demand, fractional flux, heat/matter/energy accounting, hot suppression, cold recovery, "
                     "child reproduction\n";
    }
};
int main() {
    try {
        SimulationAudit::run();
    } catch (const std::exception& e) {
        std::cerr << "FAIL: " << e.what() << '\n';
        return 1;
    }
}

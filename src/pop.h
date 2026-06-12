#pragma once
#include "common.h"
#include "genome.h"
#include "i_agent.h"
#include "i_brain.h"
#include "i_config.h"
#include "i_logger.h"
#include "i_world.h"
#include "neuron.h"
#include "random_utility.h"

#include <cstdint>
#include <memory>
#include <string>
#include <type_traits>

/// -----------------------------------------------------------------------------
/// enum class representing every phase of the Pop's lifecycle, used to manage its behavior and state transitions.
/// -----------------------------------------------------------------------------
enum class State {
    SENSE,
    THINK,
    ACT

};

///----------------------------------------------------------------------------
/// @brief The cause of death for this Pop (used for logging and analysis).
///----------------------------------------------------------------------------
enum class DeathCause { None, EnergyDepletion, OldAge };
/// @brief Physical traits of a Pop entity (set at birth, inherited/mutated).
struct PhyT {
    unsigned mitochondrions;    ///< aerobic respiration capacity (energy from glucose+O2)
    unsigned chloroplasts;      ///< photosynthesis capacity (glucose from CO2+H2O)
    unsigned sensitiveness;     ///< directional scan radius in cells
    unsigned adipose_stock_max; ///< maximum lipid storage (thermal insulation)
};

/// -----------------------------------------------------------------------------
/// @class Pop
/// @brief Implements the IAgent interface representing a population entity within the simulation.
/// -----------------------------------------------------------------------------
class Pop : public IAgent, public std::enable_shared_from_this<Pop> {
  public:
    ///----------------------------------------------------------------------------
    /// @brief Constructor for Pop class.
    /// @param world Weak pointer to the world the Pop belongs to.
    /// @param logger Shared pointer to a logger for logging events and errors.
    ///----------------------------------------------------------------------------
    Pop(std::weak_ptr<IWorld> world, std::shared_ptr<ILogger> logger, std::shared_ptr<IConfig> config);
    ///----------------------------------------------------------------------------
    /// @brief Constructor for offspring: uses a pre-built genome instead of generating a random one.
    ///----------------------------------------------------------------------------
    Pop(std::weak_ptr<IWorld> world, std::shared_ptr<ILogger> logger, std::shared_ptr<IConfig> config, Genome genome);

    /// @brief Serialises brain weights to JSON with the given generation stamp.
    void serialize_brain(unsigned generation) const;
    ///---------------------------------------------------------------------------
    /// @brief Initializes the Pop entity (sets initial stats, behaviors, etc.).
    ///---------------------------------------------------------------------------
    void init() override;
    ///-----------------------------------------------------------\----------------
    /// @brief Handles the death of the Pop entity (cleanup, final actions, mark as dead).
    ///---------------------------------------------------------------------------
    void die() override;
    ///---------------------------------------------------------------------------
    /// @brief Senses the environment (perceive nearby entities, environment, etc.).
    ///---------------------------------------------------------------------------
    void sense() override;
    ///---------------------------------------------------------------------------
    /// @brief Thinks and makes decisions (AI, behavior selection, goal planning).
    ///---------------------------------------------------------------------------
    void think() override;
    ///---------------------------------------------------------------------------
    /// @brief Acts based on decisions made (move, interact, consume resources, etc.).
    ///---------------------------------------------------------------------------
    void act() override;
    ///---------------------------------------------------------------------------
    /// @brief Attempts to move the Pop to a new position.
    /// @param p The target position to move to.
    /// @return True if the move was successful, false otherwise.
    ///---------------------------------------------------------------------------
    bool try_move(PositionT p) override;
    ///---------------------------------------------------------------------------
    /// @brief Gets the current position of the Pop.
    /// @return The current position of the Pop.
    ///---------------------------------------------------------------------------

    [[nodiscard]] PositionT get_position() const override;
    ///---------------------------------------------------------------------------
    /// @brief Checks if the Pop is alive.
    /// @return True if the Pop is alive, false otherwise.
    ///---------------------------------------------------------------------------
    bool is_alive() override;
    ///---------------------------------------------------------------------------
    /// @brief Attempts to spawn a new Pop at the specified position.
    /// @param p The position to spawn the new Pop.
    /// @return True if the spawn was successful, false otherwise.
    ///---------------------------------------------------------------------------
    bool try_spawn(PositionT p) override;
    ///---------------------------------------------------------------------------
    /// @brief Updates the state of the Pop (called each simulation tick).
    ///---------------------------------------------------------------------------
    void update() override;
    ///---------------------------------------------------------------------------
    /// @brief Despawns the Pop from the simulation (removes it from the world).
    ///---------------------------------------------------------------------------
    void despawn() override;

    ///----------------------------------------------------------------------------
    /// @brief Returns true if this Pop satisfies all conditions to reproduce this cycle.
    ///----------------------------------------------------------------------------
    [[nodiscard]] bool wants_to_reproduce() const;

    ///----------------------------------------------------------------------------
    /// @brief Creates a (possibly mutated) copy of this genome for an offspring.
    ///----------------------------------------------------------------------------
    [[nodiscard]] Genome make_offspring_genome() const;

    ///----------------------------------------------------------------------------
    /// @brief Halves the parent's internal reserves and writes the donated amounts to the out params.
    ///----------------------------------------------------------------------------
    void donate_resources(unsigned& out_glucose, unsigned& out_water, unsigned& out_calcium, unsigned& out_carbon);

    ///----------------------------------------------------------------------------
    /// @brief Sets internal reserves directly (used to initialise offspring with parent donation).
    ///----------------------------------------------------------------------------
    void set_resources(unsigned glucose, unsigned water, unsigned calcium, unsigned carbon);

    ///---------------------------------------------------------------------------
    /// @brief Gets the render state of the Pop for visualization.
    /// @return The render state of the Pop.
    ///---------------------------------------------------------------------------
    [[nodiscard]] RenderState get_render_state() const override;

    // ---------------------------------------------------------------------------
    /// @brief Increments the offspring count for this Pop (called when an offspring is successfully spawned).
    // ---------------------------------------------------------------------------
    void increment_offspring_count();

    // ---------------------------------------------------------------------------
    /// @brief get the cause of death for this Pop (used for logging and analysis).
    /// @return The cause of death for this Pop.
    // ---------------------------------------------------------------------------
    DeathCause get_death_cause() const;

  private:
    ///---------------------------------------------------------------------------
    /// @brief guard flag to avoid some action at first run
    ///---------------------------------------------------------------------------
    bool first_run_ = true;

    /// ---------------------------------------------------------------------------
    /// @brief Age of the Pop entity.
    /// ---------------------------------------------------------------------------
    uint32_t age_;
    /// ---------------------------------------------------------------------------
    /// @brief Energy level of the Pop entity.
    /// ---------------------------------------------------------------------------
    float energy_;

    /// ---------------------------------------------------------------------------
    /// @brief Genome representing the genetic makeup of the Pop.
    /// ---------------------------------------------------------------------------
    Genome genome_;
    /// ---------------------------------------------------------------------------
    /// @brief Current position of the Pop in the world.
    /// ---------------------------------------------------------------------------
    PositionT pos_;

    /// ---------------------------------------------------------------------------
    /// @brief Last movement direction of the Pop entity.
    /// ---------------------------------------------------------------------------
    PositionT last_direction_;
    /// ---------------------------------------------------------------------------
    /// @brief Alive status of the Pop entity.
    /// ---------------------------------------------------------------------------
    bool alive_ = false;

    /// ---------------------------------------------------------------------------
    /// @brief Current state of the Pop entity state machine  .
    /// ---------------------------------------------------------------------------
    State current_state_ = State::SENSE;

    /// ---------------------------------------------------------------------------
    /// @brief INternal counter of steps taken in the current state.
    /// ---------------------------------------------------------------------------
    uint64_t steps_in_current_state_ = 0;
    /// ---------------------------------------------------------------------------
    /// @brief Weak pointer to the world the Pop belongs to.
    /// ---------------------------------------------------------------------------
    std::weak_ptr<IWorld> world_; // Observes the world without owning it
    /// ---------------------------------------------------------------------------
    /// @brief Shared pointer to a logger for logging events and errors.
    /// ---------------------------------------------------------------------------
    std::shared_ptr<ILogger> logger_;
    /// ---------------------------------------------------------------------------
    /// @brief Shared pointer to the simulation configuration.
    /// ---------------------------------------------------------------------------
    std::shared_ptr<IConfig> config_;
    /// ---------------------------------------------------------------------------
    /// @brief Neural network brain used for decision making.
    /// ---------------------------------------------------------------------------
    std::unique_ptr<IBrain> brain_;
    /// ---------------------------------------------------------------------------
    /// @brief Shared pointer to a random utility for generating random numbers.
    /// ---------------------------------------------------------------------------
    std::shared_ptr<RandomUtility> random_util_;
    /// ---------------------------------------------------------------------------
    /// @brief Unique string identifier for this Pop instance.
    /// ---------------------------------------------------------------------------
    std::string pop_id_;
    /// ---------------------------------------------------------------------------
    /// @brief Last action index produced by the brain (-1 = no action).
    /// ---------------------------------------------------------------------------
    int last_action_ = -1;
    /// ---------------------------------------------------------------------------
    /// @brief Sensor values computed by sense(), consumed by think().
    /// ---------------------------------------------------------------------------
    std::vector<float> sensor_values_;
    /// ---------------------------------------------------------------------------
    /// @brief Directional scan radius in cells (used by POPULATION_DENSITY etc.).
    /// ---------------------------------------------------------------------------
    int probe_dist_ = 4;
    /// ---------------------------------------------------------------------------
    /// @brief Oscillator period in simulation steps.
    /// ---------------------------------------------------------------------------
    float osc_period_ = 100.0f;
    /// ---------------------------------------------------------------------------
    /// @brief Reactivity factor, modified by SET_RESPONSIVENESS.
    /// ---------------------------------------------------------------------------
    float responsiveness_ = 1;

    /// ---------------------------------------------------------------------------
    /// @brief Lazyness factor to reduce action frequency
    /// ---------------------------------------------------------------------------
    float lazyness_ = 0.0f;
    /// ---------------------------------------------------------------------------
    /// @brief Internal glucose reserve.
    /// ---------------------------------------------------------------------------
    unsigned glucose_ = 100;
    /// ---------------------------------------------------------------------------
    /// @brief Internal water reserve.
    /// ---------------------------------------------------------------------------
    unsigned water_ = 100;
    /// ---------------------------------------------------------------------------
    /// @brief Internal calcium reserve.
    /// ---------------------------------------------------------------------------
    unsigned calcium_ = 0;
    /// ---------------------------------------------------------------------------
    /// @brief Internal carbon reserve  .
    /// ---------------------------------------------------------------------------
    unsigned carbon_ = 0;
    /// ---------------------------------------------------------------------------
    /// @brief Energy cost incurred by the Pop's actions in the current cycle.
    /// ---------------------------------------------------------------------------
    float energy_cost_ = 0.0f;

    /// ---------------------------------------------------------------------------
    /// @brief Previous energy level (used to calculate energy delta for reward).
    /// ---------------------------------------------------------------------------
    float previous_energy_ = 0.0f;
    ///----------------------------------------------------------------------------
    /// @brief Counter for the number of offspring produced by this Pop
    ///----------------------------------------------------------------------------
    int offspring_count_ = 0;

    ///----------------------------------------------------------------------------
    /// @brief The cause of death for this Pop (used for logging and analysis).
    ///--------------------------------------------------------------------------
    DeathCause death_cause_ = DeathCause::None;

    /// ---------------------------------------------------------------------------
    /// @brief Physical traits (mitochondria, chloroplasts, lipid capacity).
    /// ---------------------------------------------------------------------------
    PhyT phy_;
    /// ---------------------------------------------------------------------------
    /// @brief Internal body temperature (Celsius).
    /// ---------------------------------------------------------------------------
    double temperature_;
    /// ---------------------------------------------------------------------------
    /// @brief Internal O2 reserve.
    /// ---------------------------------------------------------------------------
    unsigned o2_ = 0;
    /// ---------------------------------------------------------------------------
    /// @brief Internal CO2 reserve.
    /// ---------------------------------------------------------------------------
    unsigned co2_ = 0;
    /// ---------------------------------------------------------------------------
    /// @brief Lipid (fat) reserve — increases thermal insulation.
    /// ---------------------------------------------------------------------------
    unsigned lipids_ = 0;
    /// ---------------------------------------------------------------------------
    /// @brief Metabolism heat produced this physiology step (reset each cycle).
    /// ---------------------------------------------------------------------------
    float metabolism_heat_ = 0.0f;
    ///---------------------------------------------------------------------------
    /// @brief Updates the last movement direction based on new and old positions.
    /// @param new_pos The new position after movement.
    /// @param old_pos The old position before movement.
    ///---------------------------------------------------------------------------
    void update_last_direction(PositionT new_pos, PositionT old_pos);
    ///---------------------------------------------------------------------------
    /// @brief Emits a feromone of specified type and intensity at the Pop's position.
    /// @param type The type of feromone to emit.
    /// @param intensity The intensity of the feromone to emit.
    ///---------------------------------------------------------------------------
    void emit_feromone(FeromoneT type, int intensity);

    ///---------------------------------------------------------------------------
    /// @brief Gets the strength of a specific feromone type at a given position.
    /// @param type The type of feromone.
    /// @param pos The position to check for feromone strength.
    /// @return The strength of the specified feromone type at the given position.
    ///---------------------------------------------------------------------------
    [[nodiscard]] float get_feromone_strength(FeromoneT type, PositionT pos) const;

    ///----------------------------------------------------------------------
    /// @brief      Get the temperature at a given position
    /// @param      pos Position to check
    /// @return     temperature at the given position
    ///----------------------------------------------------------------------
    [[nodiscard]] double get_temperature(PositionT pos) const;

    ///----------------------------------------------------------------------
    /// @brief      Get the elevation at a given position
    /// @param      pos Position to check
    /// @return     elevation at the given position
    ///----------------------------------------------------------------------
    [[nodiscard]] double get_elevation(PositionT pos) const;

    ///----------------------------------------------------------------------
    /// @brief      Update the physiology of the Pop based on energy cost of actions
    ///----------------------------------------------------------------------
    void update_physiology();

    ///----------------------------------------------------------------------
    /// @brief Run photosynthesis: CO2 + H2O -> C6H12O6 + O2
    ///----------------------------------------------------------------------
    void run_chloroplasts();
    ///----------------------------------------------------------------------
    /// @brief Run cellular respiration: C6H12O6 + O2 -> energy + CO2
    ///----------------------------------------------------------------------
    void run_mitochondrions();
    ///----------------------------------------------------------------------
    /// @brief Update body temperature via thermal exchange with environment.
    ///----------------------------------------------------------------------
    void update_temperature();
    ///----------------------------------------------------------------------
    /// @brief Compute thermal exchange coefficient alpha based on lipid reserves.
    ///----------------------------------------------------------------------
    [[nodiscard]] double compute_alpha() const;

    ///----------------------------------------------------------------------
    /// @brief      Calculate the reward for the current cycle based on the Pop's state and actions
    /// @return     The calculated reward value
    ///----------------------------------------------------------------------
    [[nodiscard]] float calculate_reward() const;
    // ----------------------------------------------------------------------
    /// @brief     Apply various learning rules (Hebbian, reinforcement, etc.) to update the brain
    /// @param     reward The reward value to use for learning
    ///----------------------------------------------------------------------
    void learn(float reward);
};

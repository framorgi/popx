#pragma once

#include "common.h"
#include "i_cell.h"
#include "i_entity.h"
#include "render_state.h"

#include <memory>

///----------------------------------------------
// Type definitions for cell attributes
///----------------------------------------------

class Cell : public ICell {
  public:
    Cell(std::weak_ptr<IEntity> occupant = {});

    ~Cell() = default;
    void set_occupant(std::weak_ptr<IEntity> occupant) override;

    void set_temperature(double temperature) override;

    [[nodiscard]] double get_temperature() const override;
    void set_elevation(double elevation) override;

    [[nodiscard]] double get_elevation() const override;

    void set_humidity(double humidity) override;
    [[nodiscard]] double get_humidity() const override;

    [[nodiscard]] std::weak_ptr<IEntity> get_occupant() const override;

    [[nodiscard]] RenderState get_render_state() const override;

    void set_feromone(FeromoneT type, float value) override;

    [[nodiscard]] FeromoneMapT get_feromone_map() const override;

    void set_glucose(unsigned int glucose) override;

    [[nodiscard]] unsigned int get_glucose() const override;
    [[nodiscard]] unsigned int get_water() const override;
    [[nodiscard]] unsigned int get_calcium() const override;
    [[nodiscard]] unsigned int get_carbon() const override;
    unsigned int take_glucose(unsigned int amount) override;
    unsigned int give_glucose(unsigned int amount) override;
    unsigned int take_water(unsigned int amount) override;
    unsigned int give_water(unsigned int amount) override;
    unsigned int take_calcium(unsigned int amount) override;
    unsigned int give_calcium(unsigned int amount) override;
    unsigned int take_o2(unsigned int amount) override;
    unsigned int give_o2(unsigned int amount) override;
    unsigned int take_co2(unsigned int amount) override;
    unsigned int give_co2(unsigned int amount) override;
    unsigned int take_lipids(unsigned int amount) override;
    unsigned int give_lipids(unsigned int amount) override;
    [[nodiscard]] OrganicsT get_organics() const override {
        return organics_;
    }
    void update() override;

  private:
    ///----------------------------------------------
    // Helper method to decay feromones over time
    ///----------------------------------------------
    void decay_feromones();

    ///----------------------------------------------
    // Helper method to regenerate glucose over time
    ///----------------------------------------------
    void regen_glucose();

    ///----------------------------------------------
    // Occupant entity in the cell
    ///----------------------------------------------
    std::weak_ptr<IEntity>
        occupant_; // Weak pointer - observes but doesn't own the entity - An empty weak_ptr means no occupant

    ///----------------------------------------------
    // Environmental attributes of the cell

    ///----------------------------------------------
    /// Temperature in Celsius
    ///----------------------------------------------
    float temperature_;
    ///----------------------------------------------
    /// Humidity percentage (0-100)
    ///----------------------------------------------
    float humidity_;
    ///----------------------------------------------
    /// Elevation in meters
    ///----------------------------------------------
    float elevation_;

    ///----------------------------------------------
    // Organic compounds present in the cell
    ///----------------------------------------------
    OrganicsT organics_;

    ///----------------------------------------------
    // Feromones present in the cell
    ///----------------------------------------------
    FeromoneMapT feromones_;

    ///----------------------------------------------
    // Cell state attributes

    ///----------------------------------------------
    /// Flag to indicate if the cell is reserved for future use
    ///----------------------------------------------
    bool reserved_;

    ///----------------------------------------------
    /// Tick counter for glucose regeneration timing
    ///----------------------------------------------
    unsigned regen_tick_;
};
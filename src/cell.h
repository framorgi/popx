#pragma once

#include "common.h"
#include "i_cell.h"
#include "i_entity.h"

#include <map>
#include <memory>

///----------------------------------------------
// Type definitions for cell attributes
///----------------------------------------------
using FeromoneMap = std::map<Feromone_t, float>;

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

  private:
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
    Organics organics_;

    ///----------------------------------------------
    // Feromones present in the cell
    ///----------------------------------------------
    FeromoneMap feromones_;

    ///----------------------------------------------
    // Cell state attributes

    ///----------------------------------------------
    /// Flag to indicate if the cell is reserved for future use
    ///----------------------------------------------
    bool reserved_;
};
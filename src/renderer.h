#pragma once
#include "grid_world.h"
#include "i_graphic_engine.h"
#include "i_renderer.h"
#include "i_world.h"
#include "sfml_graphic_engine.h"

#include <array>

struct Level {
    double height;
    IGraphicEngine::Color color; // RGB
};

static const std::array<Level, 10> ground_color_levels = {{
    {0.0, {210, 190, 140}},  // sand
    {10.0, {180, 170, 100}}, // dry grass
    {20.0, {120, 180, 90}},  // plain
    {30.0, {90, 160, 80}},   // prairie
    {40.0, {60, 140, 70}},   // sparse forest
    {50.0, {40, 120, 60}},   // dense forest
    {60.0, {110, 120, 70}},  // hill
    {70.0, {120, 120, 120}}, // rock
    {80.0, {180, 180, 180}}, // high mountain
    {90.0, {245, 245, 245}}  // snow
}};

///--------------------------------------------------------------------------
/// @brief    Renderer class that implements the IRenderer interface and its methods and wraps around a graphic engine
///--------------------------------------------------------------------------
class Renderer : public IRenderer {
  public:
    ///--------------------------------------------------------------------------
    /// @brief    Constructor of Renderer
    /// @param    gfx Shared pointer to the IGraphicEngine instance
    /// @param    world Shared pointer to the IWorld instance
    ///--------------------------------------------------------------------------
    // TODO: check if we need to pass the implemented objects or the interfaces
    Renderer(std::shared_ptr<IGraphicEngine> gfx, std::shared_ptr<IWorld> world);
    ///--------------------------------------------------------------------------
    /// @brief    Initializes the renderer
    ///--------------------------------------------------------------------------
    void init() override;
    ///--------------------------------------------------------------------------
    /// @brief    Draws the current frame
    ///--------------------------------------------------------------------------
    void draw() override;
    ///--------------------------------------------------------------------------
    /// @brief    Saves the current frame to a file
    ///--------------------------------------------------------------------------
    void save_frame() override;

  private:
    ///--------------------------------------------------------------------------
    /// @brief    Shared pointer to the graphic engine  instance
    ///--------------------------------------------------------------------------
    std::shared_ptr<IGraphicEngine> gfx_;

    ///--------------------------------------------------------------------------
    /// @brief    Shared pointer to the world instance
    ///--------------------------------------------------------------------------
    std::shared_ptr<IWorld> world_;
    ///--------------------------------------------------------------------------
    /// @brief    Draws the simulation world
    ///--------------------------------------------------------------------------
    void draw_world();

    ///--------------------------------------------------------------------------
    /// @brief    Draws a single entity
    /// @param    entity Shared pointer to the entity to be drawn
    ///--------------------------------------------------------------------------
    void draw_entity(const std::shared_ptr<IEntity>& entity);

    ///--------------------------------------------------------------------------
    /// @brief    Draws a single cell
    /// @param    cell Pointer to the cell to be drawn
    /// @param    x The x-coordinate of the cell
    /// @param    y The y-coordinate of the cell
    ///--------------------------------------------------------------------------
    void draw_cell(const std::shared_ptr<ICell>& cell, int x, int y);

    ///--------------------------------------------------------------------------

    /// @brief    Evaluates color based on temperature
    /// @param    temperature The temperature value
    /// @return   Color corresponding to the temperature
    ///--------------------------------------------------------------------------
    static IGraphicEngine::Color evaluate_temperature_color(double temperature);

    ///--------------------------------------------------------------------------
    /// @brief    Evaluates color based on elevation
    /// @param    elevation The elevation value
    /// @return   Color corresponding to the elevation
    ///--------------------------------------------------------------------------
    static IGraphicEngine::Color evaluate_ground_color(double elevation);

    ///--------------------------------------------------------------------------
    /// @brief    Clamps an integer value to the range [0, 255]
    /// @param    value The integer value to clamp
    /// @return   Clamped unsigned char value
    ///--------------------------------------------------------------------------
    static uint8_t clamp_u8(int value);

    ///--------------------------------------------------------------------------
    /// @brief    Interpolates between two colors
    /// @param    a First color
    /// @param    b Second color
    /// @param    t Interpolation factor [0.0, 1.0]
    /// @return   Interpolated color
    ///--------------------------------------------------------------------------
    static IGraphicEngine::Color color_interpolate(const IGraphicEngine::Color& a, const IGraphicEngine::Color& b,
                                                   double t);

    ///--------------------------------------------------------------------------
    /// @brief    Blend two colors based on a factor
    /// @param    baseColor The base color
    /// @param    blendColor The color to blend with
    /// @param    factor Blend factor [0.0 - 1.0], where 0.0 is fully baseColor and 1.0 is fully blendColor
    /// @return   The blended color with clamped component values
    ///--------------------------------------------------------------------------
    static IGraphicEngine::Color blend_colors(const IGraphicEngine::Color& baseColor,
                                              const IGraphicEngine::Color& blendColor, double factor);

    ///--------------------------------------------------------------------------
    /// @brief    Apply highlighting effect to a color based on height
    /// @param    color The original color
    /// @param    height The height value influencing the highlighting
    /// @return   The color after applying the highlighting effect
    ///--------------------------------------------------------------------------
    static IGraphicEngine::Color apply_highlighting(const IGraphicEngine::Color& color, double height);
};
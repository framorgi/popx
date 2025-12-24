#pragma once
#include <vector>
#include <memory>
#include "i_world.h"
#include "i_entity.h"
#include "i_logger.h"
#include "slice.h"

class GridWorld : public IWorld  {
public:
    GridWorld( int w , int h,std::shared_ptr<ILogger> logger = nullptr );
  
    void init() override ;


    bool is_free(Position p) const override ;
    bool move_entity(std::shared_ptr<IEntity> entity, Position new_pos) override ;
    bool remove_entity(std::shared_ptr<IEntity> e) override ;
    bool add_entity(std::shared_ptr<IEntity> e) override ;
    bool update_cycle() override;

    int get_width() const override { return width_; }
    int get_height() const override { return height_; }

    std::vector<std::shared_ptr<Slice>> get_slices() const { return slices_; }
private:
    bool in_bounds(int x, int y) const ;



    int index(int x, int y) const ;

    int width_, height_;
    std::vector<std::shared_ptr<Slice>> slices_;
    std::shared_ptr<ILogger> logger_;
};
#include "grid_world.h"



 grid_world::grid_world( int w, int h,std::shared_ptr<i_logger> logger) : width_(w), height_(h), logger_(logger)  {
        slices_.resize(width_ * height_, std::make_shared<slice>());
        logger_->debug("Grid world created with size " + std::to_string(width_) + "x" + std::to_string(height_));
    }


    void grid_world::init()   {
        for (auto& cell : slices_)
            cell = nullptr;
    }


    bool grid_world::is_free(Position p) const {
        return in_bounds(p.x, p.y) && slices_[index(p.x, p.y)]->get_occupant() == nullptr      ;
    }

    bool grid_world::move_entity(std::shared_ptr<i_entity> entity, Position new_pos)   {
        if (!is_free(new_pos))
            return false;

        Position old_pos = entity->get_position();
        slices_[index(old_pos.x, old_pos.y)]->set_occupant(nullptr);
        slices_[index(new_pos.x, new_pos.y)]->set_occupant(entity);

        return true;
    }

    bool grid_world::remove_entity(std::shared_ptr<i_entity> e)   {
        Position pos = e->get_position();
        if (!in_bounds(pos.x, pos.y) || slices_[index(pos.x, pos.y)]->get_occupant() != e)
            return false;
        slices_[index(pos.x, pos.y)]->set_occupant(nullptr);
        return true;
    }   
    bool grid_world::add_entity(std::shared_ptr<i_entity> e)   {
        
        Position pos = e->get_position();
        logger_->debug("Adding entity to grid world at position (" + std::to_string(pos.x) + "," + std::to_string(pos.y) + ")");
        if (!is_free(pos))
            return false;
        slices_[index(pos.x, pos.y)] = std::make_shared<slice>(e);
        return true;

    }


    bool grid_world::in_bounds(int x, int y) const {
        return x >= 0 && y >= 0 && x < width_ && y < height_;
    }

    int grid_world::index(int x, int y) const { return y * width_ + x; }

    bool grid_world::update_cycle()
    {
        return true;
    }

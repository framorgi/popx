#pragma once
#include "i_agent.h"

#include "i_world.h"


class pop : public i_agent 
{
private:
    /* data */
public:
    pop(/* args */);
  
    void init() override;
    void die() override;
    void sense(std::shared_ptr<i_world> world) override;
    void think() override;
    void act() override;
    bool try_move(std::shared_ptr<i_world> world, Position p) override;
    Position get_position() const  override;
    bool is_alive() override;
    bool try_spawn(std::shared_ptr<i_world> world, Position p) override;
    void update(std::shared_ptr<i_world> world) override;
    void despawn(std::shared_ptr<i_world> world) override;
    

    private :
    Position pos_;
    bool alive_ = false;
};


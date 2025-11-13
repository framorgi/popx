#include "pop.h"

pop::pop(/* args */)
{
    alive_ = true;
    pos_.x = 0;
    pos_.y = 0;

}
void pop::init()
{
}

void pop::die()
{
}

bool pop::try_spawn(std::shared_ptr<i_world> world, Position p)

{
     pos_ = p;
    if (world->add_entity(std::make_shared<pop>(*this))) {
       
        return true;
    }
    return false;
}

void pop::update(std::shared_ptr<i_world> world)
{
}

void pop::despawn(std::shared_ptr<i_world> world)
{
    alive_ = false;
    world->remove_entity(std::make_shared<pop>(*this));
}

void pop::sense(std::shared_ptr<i_world> world)
{
}

void pop::think()
{
}


void pop::act()
{

}

bool pop::try_move(std::shared_ptr<i_world> world, Position p)
{
    return false;
}

Position pop::get_position() const
{
    return pos_;
}

bool pop::is_alive()
{
    return alive_;
}



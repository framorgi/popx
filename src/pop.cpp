#include "pop.h"

Pop::Pop(/* args */)
{
    alive_ = true;
    pos_.x = 0;
    pos_.y = 0;

}
void Pop::init()
{
}

void Pop::die()
{
}

bool Pop::try_spawn(std::shared_ptr<IWorld> world, Position p)
{
     pos_ = p;
    return world->add_entity(std::make_shared<Pop>(*this));
}

void Pop::update(std::shared_ptr<IWorld> world)
{
}

void Pop::despawn(std::shared_ptr<IWorld> world)
{
    alive_ = false;
    world->remove_entity(std::make_shared<Pop>(*this));
}

void Pop::sense(std::shared_ptr<IWorld> world)
{
}

void Pop::think()
{
}


void Pop::act()
{

}

bool Pop::try_move(std::shared_ptr<IWorld> world, Position p)
{
    return false;
}

Position Pop::get_position() const
{
    return pos_;
}

bool Pop::is_alive()
{
    return alive_;
}



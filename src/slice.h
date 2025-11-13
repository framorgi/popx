#pragma once

#include "i_agent.h"

class slice
{
public:
    slice(std::shared_ptr<i_agent> occupant_ = nullptr) : occupant_(occupant_) {}

    ~slice() = default;
    void set_occupant(std::shared_ptr<i_agent> occupant_) { occupant_ = occupant_; }
    std::shared_ptr<i_agent> get_occupant() const { return occupant_; }

private:
    std::shared_ptr<i_agent> occupant_;
};

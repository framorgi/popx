#pragma once

#include "i_agent.h"
#include "i_slice.h"

class Slice : ISlice
{
public:
    Slice(std::shared_ptr<IAgent> occupant_ = nullptr) : occupant_(occupant_) {}

    ~Slice() = default;
    void set_occupant(std::shared_ptr<IAgent> occupant_) { occupant_ = occupant_; }
    [[nodiscard]] std::shared_ptr<IAgent> get_occupant() const { return occupant_; }

private:
    std::shared_ptr<IAgent> occupant_;
};

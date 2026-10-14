#pragma once
#include <nlohmann/json.hpp>
#include <vector>

/// @brief Transport of immutable messages; never accesses simulation objects.
class ICommunicator {
  public:
    virtual ~ICommunicator() = default;
    virtual void send(nlohmann::json message) = 0;
    virtual std::vector<nlohmann::json> receive() = 0;
    virtual nlohmann::json status() const = 0;
    virtual bool flush(unsigned timeout_ms) = 0;
};

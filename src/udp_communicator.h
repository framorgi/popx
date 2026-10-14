#pragma once
#include "i_communicator.h"

#include <atomic>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <thread>

/// @brief Bounded, acknowledged loopback UDP transport with fragmented messages.
class UdpCommunicator final : public ICommunicator {
  public:
    explicit UdpCommunicator(unsigned port);
    ~UdpCommunicator() override;
    void send(nlohmann::json message) override;
    std::vector<nlohmann::json> receive() override;
    nlohmann::json status() const override;
    bool flush(unsigned timeout_ms) override;

  private:
    void work();
    void note_drop(const std::string& body);
    int socket_ = -1;
    std::string process_id_;
    mutable std::mutex mutex_;
    std::condition_variable wake_;
    nlohmann::json last_gap_;
    std::deque<std::string> out_;
    std::deque<nlohmann::json> in_;
    std::size_t queued_bytes_ = 0;
    std::atomic<bool> stopping_{false};
    std::atomic<unsigned> pending_{0};
    std::atomic<uint64_t> dropped_{0}, acknowledged_{0}, last_ack_ms_{0};
    std::thread worker_;
};

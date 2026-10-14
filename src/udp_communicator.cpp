#include "udp_communicator.h"

#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>

#include <chrono>
#include <map>
#include <random>
#include <stdexcept>

namespace {
using Clock = std::chrono::steady_clock;
uint64_t milliseconds() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now().time_since_epoch()).count();
}
constexpr std::size_t max_bytes = 32 * 1024 * 1024;
constexpr std::size_t chunk_size = 512;
} // namespace
UdpCommunicator::UdpCommunicator(unsigned port) {
    socket_ = socket(AF_INET, SOCK_DGRAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
    if (socket_ < 0)
        throw std::runtime_error("Cannot create UDP socket");
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    address.sin_port = htons(port);
    if (connect(socket_, reinterpret_cast<sockaddr*>(&address), sizeof(address)) != 0) {
        close(socket_);
        throw std::runtime_error("Cannot connect UDP socket");
    }
    process_id_ = std::to_string(getpid()) + "-" + std::to_string(std::random_device{}());
    worker_ = std::thread(&UdpCommunicator::work, this);
}
UdpCommunicator::~UdpCommunicator() {
    stopping_ = true;
    wake_.notify_all();
    worker_.join();
    close(socket_);
}
void UdpCommunicator::send(nlohmann::json message) {
    message["version"] = 1;
    message["process_id"] = process_id_;
    std::string body = message.dump(-1, ' ', true); // ASCII fragments never split a UTF-8 code point.
    std::lock_guard lock(mutex_);
    if (body.size() > max_bytes / 2) {
        note_drop(body);
        return;
    }
    while (!out_.empty() && (queued_bytes_ + body.size() > max_bytes || out_.size() >= 4096)) {
        queued_bytes_ -= out_.front().size();
        note_drop(out_.front());
        out_.pop_front();
    }
    queued_bytes_ += body.size();
    out_.push_back(std::move(body));
    wake_.notify_all();
}
std::vector<nlohmann::json> UdpCommunicator::receive() {
    std::lock_guard lock(mutex_);
    std::vector<nlohmann::json> result;
    while (!in_.empty()) {
        result.push_back(std::move(in_.front()));
        in_.pop_front();
    }
    return result;
}
nlohmann::json UdpCommunicator::status() const {
    std::lock_guard lock(mutex_);
    return {{"connected", last_ack_ms_ != 0 && milliseconds() - last_ack_ms_ < 5000},
            {"dropped_messages", dropped_.load()},
            {"acknowledged", acknowledged_.load()},
            {"queued", out_.size() + pending_.load()},
            {"last_gap", last_gap_}};
}
bool UdpCommunicator::flush(unsigned timeout_ms) {
    const auto until = Clock::now() + std::chrono::milliseconds(timeout_ms);
    do {
        {
            std::lock_guard lock(mutex_);
            if (out_.empty() && pending_ == 0)
                return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    } while (Clock::now() < until);
    return false;
}
void UdpCommunicator::work() {
    struct Pending {
        std::string body;
        uint64_t sent = 0;
        unsigned tries = 0;
    };
    std::map<uint64_t, Pending> pending;
    uint64_t sequence = 0;
    char buffer[65536];
    while (!stopping_) {
        {
            std::lock_guard lock(mutex_);
            std::size_t pending_bytes = 0;
            for (const auto& entry : pending)
                pending_bytes += entry.second.body.size();
            while (!out_.empty() && pending.size() < 32 && pending_bytes + out_.front().size() <= max_bytes) {
                pending_bytes += out_.front().size();
                queued_bytes_ -= out_.front().size();
                pending.emplace(++sequence, Pending{std::move(out_.front())});
                out_.pop_front();
            }
            pending_ = pending.size();
        }
        for (auto it = pending.begin(); it != pending.end();) {
            auto& p = it->second;
            if (milliseconds() - p.sent < 500) {
                ++it;
                continue;
            }
            if (p.tries++ >= 12) {
                {
                    std::lock_guard lock(mutex_);
                    note_drop(p.body);
                }
                it = pending.erase(it);
                continue;
            }
            const auto count = (p.body.size() + chunk_size - 1) / chunk_size;
            for (std::size_t i = 0; i < count; ++i) {
                const auto packet =
                    nlohmann::json{{"v", 1}, {"p", process_id_}, {"s", it->first},
                                   {"i", i}, {"n", count},       {"d", p.body.substr(i * chunk_size, chunk_size)}}
                        .dump();
                ::send(socket_, packet.data(), packet.size(), MSG_NOSIGNAL);
                // Large diagnostic records must not monopolise the receiver's socket buffer.
                if (i % 64 == 63)
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
            p.sent = milliseconds();
            ++it;
        }
        for (int budget = 0; budget < 128; ++budget) {
            const auto size = recv(socket_, buffer, sizeof(buffer), 0);
            if (size <= 0)
                break;
            auto data = nlohmann::json::parse(buffer, buffer + size, nullptr, false);
            if (!data.is_object())
                continue;
            try {
                if (data.contains("ack") && data.value("p", "") == process_id_) {
                    if (pending.erase(data.at("ack").get<uint64_t>()))
                        ++acknowledged_;
                    last_ack_ms_ = milliseconds();
                } else if (data.value("version", 0) == 1 && data.value("type", "") == "command" &&
                           data.value("process_id", "") == process_id_ && data.contains("request_id") &&
                           data["request_id"].is_string() &&
                           data["request_id"].get_ref<const std::string&>().size() <= 128) {
                    std::lock_guard lock(mutex_);
                    if (in_.size() < 256)
                        in_.push_back(std::move(data));
                }
            } catch (const nlohmann::json::exception&) { /* Reject malformed datagrams. */
            }
        }
        pending_ = pending.size();
        std::unique_lock lock(mutex_);
        wake_.wait_for(lock, std::chrono::milliseconds(10));
    }
}

void UdpCommunicator::note_drop(const std::string& body) {
    ++dropped_;
    auto message = nlohmann::json::parse(body, nullptr, false);
    if (!message.is_object())
        return;
    const auto tick = message.value("tick", uint64_t(0));
    const auto run = message.value("run_id", std::string{});
    if (!last_gap_.is_object() || last_gap_.value("run_id", std::string{}) != run)
        last_gap_ = {{"run_id", run}, {"from_tick", tick}, {"through_tick", tick}};
    else {
        last_gap_["from_tick"] = std::min(tick, last_gap_["from_tick"].get<uint64_t>());
        last_gap_["through_tick"] = std::max(tick, last_gap_["through_tick"].get<uint64_t>());
    }
}

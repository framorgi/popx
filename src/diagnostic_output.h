#pragma once
#include "stats.h"
#include "udp_communicator.h"

#include <cstdlib>
#include <stdexcept>

/// @brief Finite diagnostic capture; production sessions never retain all event records.
class DiagnosticCapture final : public ICommunicator {
  public:
    std::vector<nlohmann::json> records;
    void send(nlohmann::json message) override {
        bytes_ += message.dump().size();
        if (bytes_ > 64 * 1024 * 1024)
            throw std::runtime_error("Diagnostic capture budget exceeded");
        records.push_back(std::move(message));
    }
    std::vector<nlohmann::json> receive() override {
        return {};
    }
    nlohmann::json status() const override {
        return {{"connected", false}, {"dropped_messages", 0}};
    }
    bool flush(unsigned) override {
        return true;
    }

  private:
    std::size_t bytes_ = 0;
};

/// @brief Diagnostics use the same collector/transport; success requires committed backend acknowledgement.
inline void persist_diagnostic(const nlohmann::json& report, const std::string& kind) {
    const char* port = std::getenv("POPX_UDP_PORT");
    if (!port)
        throw std::runtime_error("Use scripts/run_*: POPX_UDP_PORT receiver required");
    Stats stats(std::make_unique<UdpCommunicator>(std::stoul(port)));
    stats.begin_session({{"kind", kind}, {"model", "reserve_regulated_v1"}, {"diagnostic", true}});
    auto persisted = report;
    if (report.contains("run") && report["run"].contains("central_records")) {
        for (const auto& record : report["run"]["central_records"]) {
            if (record["type"] == "session")
                continue;
            stats.set_tick(record["tick"].get<uint64_t>());
            stats.publish(record["type"].get<std::string>(), record["data"]);
        }
        persisted["run"].erase("central_records");
    }
    stats.record_diagnostic({std::move(persisted)});
    if (!stats.flush(15000) || stats.transport_status().value("dropped_messages", 0) != 0)
        throw std::runtime_error("Diagnostic persistence not acknowledged");
}

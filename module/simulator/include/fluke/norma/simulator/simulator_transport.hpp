#pragma once

#include <chrono>
#include <deque>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <fluke/norma/error.hpp>
#include <fluke/norma/transport.hpp>

#include "fluke/norma/simulator/norma_simulator.hpp"

namespace fluke::norma::sim {

/// An in-process Transport backed by a NormaSimulator.
///
/// Every written line goes straight to the simulator's parser and its response
/// (if any) is buffered for the next read_line(). Nothing is serialized over a
/// socket, so tests using it are fast and deterministic, and — because a query
/// that produces no response leaves the buffer empty — a read with nothing to
/// return raises TimeoutError, which is what the real transport does too.
///
/// The public fault switches inject the failures a real link produces, so the
/// library's error paths can be tested without unplugging anything.
class SimulatorTransport final : public Transport {
public:
    explicit SimulatorTransport(std::shared_ptr<NormaSimulator> simulator =
                                    std::make_shared<NormaSimulator>())
        : simulator_(std::move(simulator)) {}

    NormaSimulator& simulator() { return *simulator_; }
    const NormaSimulator& simulator() const { return *simulator_; }
    std::shared_ptr<NormaSimulator> shared_simulator() const { return simulator_; }

    // -- Transport ------------------------------------------------------------
    void open() override { open_ = true; }

    void close() override {
        open_ = false;
        responses_.clear();
    }

    bool is_open() const override { return open_; }

    void write(std::string_view data, std::chrono::milliseconds timeout) override {
        require_open();
        write_timeouts.push_back(timeout);
        if (fail_writes) {
            throw ConnectionError("simulated transport: write failed");
        }
        pending_.append(data);
        // A Transport is handed whole lines; anything without a terminator stays
        // buffered, which is also how a real socket behaves.
        std::size_t newline = pending_.find('\n');
        while (newline != std::string::npos) {
            std::string line = pending_.substr(0, newline);
            pending_.erase(0, newline + 1);
            if (!line.empty() && line.back() == '\r') {
                line.pop_back();
            }
            writes.push_back(line);
            if (auto response = simulator_->handle_line(line)) {
                responses_.push_back(std::move(*response));
            }
            newline = pending_.find('\n');
        }
    }

    std::string read_line(std::chrono::milliseconds timeout) override {
        require_open();
        read_timeouts.push_back(timeout);
        if (fail_reads) {
            throw ConnectionError("simulated transport: connection reset by peer");
        }
        if (stall_reads) {
            throw TimeoutError("simulated transport: read timed out after " +
                               std::to_string(timeout.count()) + " ms");
        }
        if (responses_.empty()) {
            // The instrument had nothing to say, so the client waits in vain.
            throw TimeoutError("simulated transport: no response pending, read timed out after " +
                               std::to_string(timeout.count()) + " ms");
        }
        std::string line = std::move(responses_.front());
        responses_.pop_front();
        return line;
    }

    std::string description() const override { return "simulator://in-process"; }

    // -- Recorded traffic, for assertions --------------------------------------
    std::vector<std::string> writes;                        ///< command lines sent, without '\n'
    std::vector<std::chrono::milliseconds> write_timeouts;  ///< timeout passed to each write
    std::vector<std::chrono::milliseconds> read_timeouts;   ///< timeout passed to each read

    /// Responses the simulator produced but the client has not read yet — a
    /// non-zero count after a sequence of calls means the protocol desynchronized.
    std::size_t pending_responses() const { return responses_.size(); }

    // -- Fault injection --------------------------------------------------------
    bool fail_writes = false;  ///< every write throws ConnectionError
    bool fail_reads = false;   ///< every read throws ConnectionError
    bool stall_reads = false;  ///< every read throws TimeoutError

private:
    void require_open() const {
        if (!open_) {
            throw ConnectionError("simulated transport: not connected");
        }
    }

    std::shared_ptr<NormaSimulator> simulator_;
    std::string pending_;             ///< bytes written without a terminator yet
    std::deque<std::string> responses_;
    bool open_ = false;
};

} // namespace fluke::norma::sim

#pragma once

#include <chrono>
#include <deque>
#include <string>
#include <vector>

#include <fluke/norma/error.hpp>
#include <fluke/norma/transport.hpp>

namespace fluke::norma::test {

/// Scripted transport for unit tests: records everything written and returns
/// pre-loaded responses, so the wire format and the parsing are testable
/// without an instrument and without a state machine.
///
/// Use this when the point of the test is the exact bytes on the wire. When the
/// point is a workflow, use fluke::norma::sim::SimulatorTransport instead — it
/// models instrument state rather than replaying a fixed script.
///
/// Starts closed, like a real transport, and refuses I/O until open() is called,
/// so "used before connecting" is a path the tests can cover.
class MockTransport final : public Transport {
public:
    /// Constructs a transport that is already open, for tests that are not
    /// about the connection lifecycle.
    static MockTransport opened() {
        MockTransport transport;
        transport.open();
        return transport;
    }

    void open() override {
        open_ = true;
        ++open_count;
    }

    void close() override {
        open_ = false;
        ++close_count;
    }

    bool is_open() const override { return open_; }

    void write(std::string_view data, std::chrono::milliseconds timeout) override {
        require_open();
        write_timeouts.push_back(timeout);
        writes.emplace_back(data);
    }

    std::string read_line(std::chrono::milliseconds timeout) override {
        require_open();
        read_timeouts.push_back(timeout);
        if (responses.empty()) {
            throw TimeoutError("mock: no scripted response left");
        }
        std::string line = responses.front();
        responses.pop_front();
        return line;
    }

    std::string description() const override { return "mock"; }

    /// The last line written, without its terminator.
    std::string last_command() const {
        if (writes.empty()) {
            return {};
        }
        std::string line = writes.back();
        while (!line.empty() && (line.back() == '\n' || line.back() == '\r')) {
            line.pop_back();
        }
        return line;
    }

    std::vector<std::string> writes;    ///< every line sent (incl. trailing \n)
    std::deque<std::string> responses;  ///< scripted response lines
    std::vector<std::chrono::milliseconds> write_timeouts;
    std::vector<std::chrono::milliseconds> read_timeouts;
    int open_count = 0;
    int close_count = 0;

private:
    void require_open() const {
        if (!open_) {
            throw ConnectionError("mock: not connected");
        }
    }

    bool open_ = false;
};

} // namespace fluke::norma::test

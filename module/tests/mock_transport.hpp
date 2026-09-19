#pragma once

#include <deque>
#include <string>
#include <vector>

#include <fluke/norma/error.hpp>
#include <fluke/norma/transport.hpp>

namespace fluke::norma::test {

/// Scripted transport for unit tests: records everything written and returns
/// pre-loaded responses, so the protocol logic is testable without hardware.
class MockTransport final : public Transport {
public:
    void open() override { open_ = true; }
    void close() override { open_ = false; }
    bool is_open() const override { return open_; }

    void write(std::string_view data, std::chrono::milliseconds) override {
        writes.emplace_back(data);
    }

    std::string read_line(std::chrono::milliseconds) override {
        if (responses.empty()) {
            throw TimeoutError("mock: no scripted response left");
        }
        std::string line = responses.front();
        responses.pop_front();
        return line;
    }

    std::string description() const override { return "mock"; }

    std::vector<std::string> writes;    ///< every line sent (incl. trailing \n)
    std::deque<std::string> responses;  ///< scripted response lines

private:
    bool open_ = true;
};

} // namespace fluke::norma::test

// ScpiClient against a scripted transport: line framing, timeouts, the error
// queue and the guarantees that keep a query's response attached to its query.

#include <atomic>
#include <chrono>
#include <memory>
#include <thread>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include <fluke/norma/error.hpp>
#include <fluke/norma/scpi_client.hpp>

#include "mock_transport.hpp"

using namespace fluke::norma;
using fluke::norma::test::MockTransport;
using namespace std::chrono_literals;

namespace {

struct Fixture {
    Fixture(std::chrono::milliseconds timeout = 5000ms) {
        auto transport = std::make_unique<MockTransport>();
        transport->open();
        mock = transport.get();
        client = std::make_unique<ScpiClient>(std::move(transport), timeout);
    }

    MockTransport* mock = nullptr;
    std::unique_ptr<ScpiClient> client;
};

} // namespace

TEST_CASE("send appends exactly one line feed") {
    Fixture f;
    f.client->send("*RST");
    REQUIRE(f.mock->writes.size() == 1);
    CHECK(f.mock->writes[0] == "*RST\n");
}

TEST_CASE("query writes the command and returns one response line") {
    Fixture f;
    f.mock->responses.push_back("Fluke,NORMA5000,KN1,01.05");

    CHECK(f.client->query("*IDN?") == "Fluke,NORMA5000,KN1,01.05");
    CHECK(f.mock->writes.back() == "*IDN?\n");
}

TEST_CASE("the default timeout is passed to both halves of a query") {
    Fixture f(1234ms);
    f.mock->responses.push_back("1");
    f.client->query("*OPC?");

    REQUIRE(f.mock->write_timeouts.size() == 1);
    REQUIRE(f.mock->read_timeouts.size() == 1);
    CHECK(f.mock->write_timeouts[0] == 1234ms);
    CHECK(f.mock->read_timeouts[0] == 1234ms);
}

TEST_CASE("a per-query timeout overrides the default") {
    Fixture f(1000ms);
    f.mock->responses.push_back("1");
    f.client->query("*OPC?", 30000ms);

    CHECK(f.mock->write_timeouts.back() == 30000ms);
    CHECK(f.mock->read_timeouts.back() == 30000ms);
}

TEST_CASE("set_default_timeout takes effect for later calls") {
    Fixture f(1000ms);
    f.client->set_default_timeout(250ms);
    CHECK(f.client->default_timeout() == 250ms);

    f.client->send("*CLS");
    CHECK(f.mock->write_timeouts.back() == 250ms);
}

// A command carrying its own terminator would put a second command on the wire
// whose response nobody reads, so every later query would return the previous
// query's answer. Rejecting it locally is the only way to keep the session in
// step; this is why the check exists.
TEST_CASE("a command containing a line terminator is rejected, not sent") {
    Fixture f;

    CHECK_THROWS_AS(f.client->send("*RST\nDATA?"), std::invalid_argument);
    CHECK_THROWS_AS(f.client->send("*RST\r\nDATA?"), std::invalid_argument);
    CHECK_THROWS_AS(f.client->query("*IDN?\nDATA?"), std::invalid_argument);
    CHECK_THROWS_AS(f.client->send("trailing\n"), std::invalid_argument);

    CHECK(f.mock->writes.empty());
    CHECK(ScpiClient::has_line_terminator("a\nb"));
    CHECK(ScpiClient::has_line_terminator("a\rb"));
    CHECK_FALSE(ScpiClient::has_line_terminator("INP1:SHUN EXT;GAIN 25.0"));
}

TEST_CASE("a ';'-separated command line is allowed through") {
    Fixture f;
    f.client->send("INP1:SHUN EXT;GAIN 25.0");
    CHECK(f.mock->writes.back() == "INP1:SHUN EXT;GAIN 25.0\n");
}

TEST_CASE("using a closed transport reports a connection error") {
    auto transport = std::make_unique<MockTransport>();
    MockTransport* mock = transport.get();
    ScpiClient client(std::move(transport));

    REQUIRE_FALSE(client.is_open());
    CHECK_THROWS_AS(client.send("*RST"), ConnectionError);

    client.open();
    CHECK(client.is_open());
    CHECK(mock->open_count == 1);
    CHECK_NOTHROW(client.send("*RST"));

    client.close();
    CHECK(mock->close_count == 1);
    CHECK_THROWS_AS(client.send("*RST"), ConnectionError);
}

TEST_CASE("query_int accepts the forms SCPI may answer with") {
    Fixture f;
    f.mock->responses = {"3", "+3", "-113", " 3 "};
    CHECK(f.client->query_int("FUNC:COUN?") == 3);
    CHECK(f.client->query_int("FUNC:COUN?") == 3);
    CHECK(f.client->query_int("FUNC:COUN?") == -113);
    CHECK(f.client->query_int("FUNC:COUN?") == 3);
}

TEST_CASE("query_int rejects a non-numeric response") {
    Fixture f;
    f.mock->responses.push_back("ASC,6");
    CHECK_THROWS_AS(f.client->query_int("FUNC:COUN?"), ProtocolError);
}

TEST_CASE("query_bool reads both the numeric and the keyword forms") {
    Fixture f;
    f.mock->responses = {"1", "0", "ON", "OFF"};
    CHECK(f.client->query_bool("INIT:CONT?"));
    CHECK_FALSE(f.client->query_bool("INIT:CONT?"));
    CHECK(f.client->query_bool("INIT:CONT?"));
    CHECK_FALSE(f.client->query_bool("INIT:CONT?"));
}

TEST_CASE("query_strings unquotes every field") {
    Fixture f;
    f.mock->responses.push_back("\"VOLT1\",\"CURR1\",\"POW1:ACT\"");
    const auto functions = f.client->query_strings("FUNC?");
    REQUIRE(functions.size() == 3);
    CHECK(functions[0] == "VOLT1");
    CHECK(functions[2] == "POW1:ACT");
}

TEST_CASE("read_all_errors drains the queue and stops at 0,\"No error\"") {
    Fixture f;
    f.mock->responses = {"-113,\"Undefined header\"", "-222,\"Data out of range\"",
                         "0,\"No error\""};

    const auto errors = f.client->read_all_errors();

    REQUIRE(errors.size() == 2);
    CHECK(errors[0].code == -113);
    CHECK(errors[1].code == -222);
    CHECK(f.mock->writes.size() == 3); // one SYST:ERR? per read, including the last
    CHECK(f.mock->writes.back() == "SYST:ERR?\n");
}

// A queue that never reports "no error" means the link is out of step. Returning
// silently would hide that, so it has to surface as a protocol error.
TEST_CASE("read_all_errors gives up instead of looping on a queue that never drains") {
    Fixture f;
    for (int i = 0; i < ScpiClient::kMaxErrorQueueDrain + 5; ++i) {
        f.mock->responses.push_back("-113,\"Undefined header\"");
    }

    CHECK_THROWS_AS(f.client->read_all_errors(), ProtocolError);
    CHECK(f.mock->writes.size() == ScpiClient::kMaxErrorQueueDrain);
}

TEST_CASE("throw_if_error carries the whole queue, not just the first entry") {
    Fixture f;
    f.mock->responses = {"-113,\"Undefined header\"", "-222,\"Data out of range\"",
                         "0,\"No error\""};

    try {
        f.client->throw_if_error();
        FAIL("expected ScpiError");
    } catch (const ScpiError& e) {
        CHECK(e.code() == -113);
        CHECK(e.message() == "Undefined header");
        REQUIRE(e.all().size() == 2);
        CHECK(e.all()[1].code == -222);
        CHECK(std::string(e.what()).find("and 1 more") != std::string::npos);
    }
}

TEST_CASE("throw_if_error is silent on an empty queue") {
    Fixture f;
    f.mock->responses.push_back("0,\"No error\"");
    CHECK_NOTHROW(f.client->throw_if_error());
}

// Without the mutex, two threads could interleave write and read so that one
// thread reads the other's response. The test drives both concurrently and
// checks that every response still matches its own query.
TEST_CASE("concurrent queries keep each response attached to its query") {
    // A transport that answers from the command it was given, so a mismatch is
    // detectable rather than merely likely.
    class EchoTransport final : public Transport {
    public:
        void open() override {}
        void close() override {}
        bool is_open() const override { return true; }

        void write(std::string_view data, std::chrono::milliseconds) override {
            std::string line(data);
            while (!line.empty() && (line.back() == '\n' || line.back() == '\r')) {
                line.pop_back();
            }
            pending_ = line;
            // Widen the window in which an unsynchronized reader could grab the
            // wrong response.
            std::this_thread::sleep_for(std::chrono::microseconds(50));
        }

        std::string read_line(std::chrono::milliseconds) override { return pending_; }

        std::string description() const override { return "echo"; }

    private:
        std::string pending_;
    };

    ScpiClient client(std::make_unique<EchoTransport>());
    std::atomic<int> mismatches{0};

    const auto hammer = [&client, &mismatches](const std::string& command, int iterations) {
        for (int i = 0; i < iterations; ++i) {
            if (client.query(command) != command) {
                ++mismatches;
            }
        }
    };

    std::thread a(hammer, "*IDN?", 400);
    std::thread b(hammer, "APER?", 400);
    a.join();
    b.join();

    CHECK(mismatches.load() == 0);
}

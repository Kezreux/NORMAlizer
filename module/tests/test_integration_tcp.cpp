// The library over a real TCP socket, against the simulator served on loopback.
//
// This is the only place TcpTransport itself is exercised: connect, the line
// framing on a real stream, the timeout machinery, and what happens when the
// peer goes away. Everything else about the protocol is covered more cheaply in
// test_integration_sim.cpp, so this file concentrates on the transport.

#include <atomic>
#include <chrono>
#include <cmath>
#include <memory>
#include <string>
#include <thread>
#include <type_traits>
#include <vector>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <fluke/norma/instrument.hpp>
#include <fluke/norma/simulator/norma_simulator.hpp>
#include <fluke/norma/simulator/simulator_server.hpp>
#include <fluke/norma/tcp_transport.hpp>

using namespace fluke::norma;
using fluke::norma::sim::NormaSimulator;
using fluke::norma::sim::SimulatorServer;
using namespace std::chrono_literals;

// TcpTransport owns a live socket and an io_context behind a pimpl. A defaulted
// move would leave the moved-from object with a null impl_ that every method
// dereferences, so the operations are deleted outright; this pins that decision
// at compile time, where the mistake would otherwise reappear silently.
static_assert(!std::is_move_constructible_v<TcpTransport>,
              "TcpTransport must not be movable: a moved-from pimpl is a null dereference");
static_assert(!std::is_move_assignable_v<TcpTransport>,
              "TcpTransport must not be move-assignable");
static_assert(!std::is_copy_constructible_v<TcpTransport>,
              "TcpTransport must not be copyable: it owns a socket");

namespace {

/// A simulator on loopback plus a connected client, for one test.
struct Link {
    explicit Link(std::chrono::milliseconds timeout = 2000ms)
        : simulator(std::make_shared<NormaSimulator>()), server(simulator) {
        norma = std::make_unique<NormaInstrument>(
            NormaInstrument::connect(server.host(), server.port(), timeout));
    }

    NormaInstrument* operator->() { return norma.get(); }

    /// Opens a second client after the first one is gone.
    NormaInstrument reconnect(std::chrono::milliseconds timeout = 2000ms) {
        return NormaInstrument::connect(server.host(), server.port(), timeout);
    }

    std::shared_ptr<NormaSimulator> simulator;
    SimulatorServer server;
    std::unique_ptr<NormaInstrument> norma;
};

} // namespace

TEST_CASE("the server reports the port it bound", "[tcp]") {
    SimulatorServer server;
    CHECK(server.port() != 0);
    CHECK(server.host() == "127.0.0.1");
    CHECK(server.connections() == 0);
}

TEST_CASE("connecting over TCP identifies the instrument", "[tcp]") {
    Link link;

    const auto id = link->identify();
    CHECK(id.manufacturer == "Fluke");
    CHECK(id.model == "NORMA5000");
    CHECK_FALSE(id.serial_number.empty());
    CHECK(link.server.connections() == 1);
    CHECK(link->is_open());
}

TEST_CASE("the measurement workflow runs over a real socket", "[tcp]") {
    Link link;
    link.server.with_simulator([](NormaSimulator& sim) {
        sim.signal().frequency = 50.0;
        sim.signal().phases[0] = {230.0, 5.0, 0.0, 0.0, 0.0, 0.0, 0.0};
    });

    link->prepare();
    link->reset();
    link->wait_operation_complete();
    link->sync_to_voltage(1);
    link->set_voltage_autorange(1, true);
    link->set_current_autorange(1, true);
    link->set_aperture(0.1);
    link->set_functions({fn::voltage(1), fn::current(1), fn::active_power(1)});
    link->set_continuous(true);
    CHECK_NOTHROW(link->check_errors());

    const auto values = link->data();
    REQUIRE(values.size() == 3);
    CHECK_THAT(values[0], Catch::Matchers::WithinRel(230.0, 1e-4));
    CHECK_THAT(values[1], Catch::Matchers::WithinRel(5.0, 1e-4));
    CHECK_THAT(values[2], Catch::Matchers::WithinRel(1150.0, 1e-4));

    const Reading reading = link->data_with_status();
    REQUIRE(reading.values.size() == 3);
    CHECK(reading.is_valid(0));
    CHECK_NOTHROW(link->check_errors());
}

TEST_CASE("the error queue round-trips over TCP", "[tcp]") {
    Link link;
    link->write("SYSTEM:THIS:DOES:NOT:EXIST");

    const auto errors = link->read_errors();
    REQUIRE(errors.size() == 1);
    CHECK(errors[0].code == -113);
    CHECK_NOTHROW(link->check_errors());
}

TEST_CASE("a connection can be closed and a new one opened", "[tcp]") {
    // The instrument serves one client at a time, so a client that disconnects
    // has to leave the socket usable for the next one.
    Link link;
    CHECK(link->is_open());
    link->set_aperture(0.25);

    link->close();
    CHECK_FALSE(link->is_open());
    CHECK_THROWS_AS(link->identify(), ConnectionError);

    auto again = link.reconnect();
    CHECK(again.is_open());
    CHECK_FALSE(again.query("*IDN?").empty());
    // The instrument kept its settings across the reconnect.
    CHECK_THAT(again.aperture(), Catch::Matchers::WithinRel(0.25, 1e-9));
    CHECK(link.server.connections() == 2);
}

TEST_CASE("connecting to a port nobody listens on fails cleanly", "[tcp]") {
    // Bind a server only to learn a free port, then let it go.
    std::uint16_t port = 0;
    {
        SimulatorServer probe;
        port = probe.port();
    }

    CHECK_THROWS_AS(NormaInstrument::connect("127.0.0.1", port, 1000ms), ConnectionError);
}

TEST_CASE("an unresolvable host is reported as a connection error", "[tcp]") {
    CHECK_THROWS_AS(
        NormaInstrument::connect("no-such-host.invalid", kDefaultPort, 1000ms),
        ConnectionError);
}

TEST_CASE("a peer that disappears mid-session is reported, not hung on", "[tcp]") {
    Link link;
    CHECK_NOTHROW(link->identify());

    link.server.disconnect_client();

    // Either the write fails or the read finds the stream at end of file;
    // both are ConnectionError rather than a silent wrong answer.
    CHECK_THROWS_AS(
        [&] {
            for (int attempt = 0; attempt < 5; ++attempt) {
                link->identify();
            }
        }(),
        ConnectionError);
}

TEST_CASE("an instrument that never answers produces a TimeoutError", "[tcp]") {
    Link link(300ms);
    link.server.set_stalled(true);

    const auto started = std::chrono::steady_clock::now();
    CHECK_THROWS_AS(link->identify(), TimeoutError);
    const auto elapsed = std::chrono::steady_clock::now() - started;

    // It waited for the timeout, and not much longer.
    CHECK(elapsed >= 250ms);
    CHECK(elapsed < 5s);
}

// After a timeout the socket is closed on purpose: a late response must never be
// mistaken for the answer to the next query. The session is then dead, which is
// the honest outcome.
TEST_CASE("a timed-out session reports itself as closed rather than desynchronized",
          "[tcp]") {
    Link link(300ms);
    link.server.set_stalled(true);
    CHECK_THROWS_AS(link->identify(), TimeoutError);

    CHECK_FALSE(link->is_open());
    link.server.set_stalled(false);
    CHECK_THROWS_AS(link->identify(), ConnectionError);

    // Reconnecting gives a working session again.
    auto again = link.reconnect();
    CHECK(again.identify().model == "NORMA5000");
}

TEST_CASE("a response larger than one TCP segment is read as one line", "[tcp]") {
    Link link(5000ms);
    // 200 functions of ~13 characters each: comfortably more than one segment,
    // and the reply must still arrive as a single line.
    std::vector<std::string> functions;
    for (int i = 0; i < 200; ++i) {
        functions.push_back(fn::active_power(1 + (i % 3)));
    }
    link->set_functions(functions);

    const auto values = link->data();
    CHECK(values.size() == functions.size());

    const std::string raw = link->query("FUNC?");
    CHECK(raw.size() > 2048);
    CHECK(raw.find('\n') == std::string::npos); // the terminator was stripped
}

TEST_CASE("two responses arriving together are handed out one at a time",
          "[tcp]") {
    // The instrument answers a compound query with one line, and the buffered
    // remainder of a segment must not leak into the next read.
    Link link;
    link->set_aperture(0.5);

    const std::string response = link->query("APER?;:FUNC:CONC?");
    const auto parts = ScpiClient::split_semicolons(response);
    REQUIRE(parts.size() == 2);
    CHECK_THAT(ScpiClient::to_double(parts[0]), Catch::Matchers::WithinRel(0.5, 1e-9));

    // The next query gets its own answer, not a leftover.
    CHECK_THAT(link->aperture(), Catch::Matchers::WithinRel(0.5, 1e-9));
}

TEST_CASE("concurrent queries over one socket keep their answers apart", "[tcp]") {
    // ScpiClient serializes each write/read pair; without that, two threads
    // sharing the socket would read each other's responses.
    Link link(5000ms);
    link->set_aperture(0.75);

    std::atomic<int> mismatches{0};
    const auto hammer = [&link, &mismatches](int iterations, bool ask_aperture) {
        for (int i = 0; i < iterations; ++i) {
            if (ask_aperture) {
                if (std::fabs(link->aperture() - 0.75) > 1e-9) {
                    ++mismatches;
                }
            } else if (link->identify().model != "NORMA5000") {
                ++mismatches;
            }
        }
    };

    std::thread a(hammer, 60, true);
    std::thread b(hammer, 60, false);
    a.join();
    b.join();

    CHECK(mismatches.load() == 0);
    CHECK_NOTHROW(link->check_errors());
}

TEST_CASE("the transport describes its endpoint", "[tcp]") {
    SimulatorServer server;
    TcpTransport transport("127.0.0.1", server.port());
    CHECK(transport.description() == "tcp://127.0.0.1:" + std::to_string(server.port()));
    CHECK_FALSE(transport.is_open());

    transport.set_connect_timeout(1500ms);
    CHECK(transport.connect_timeout() == 1500ms);

    transport.open();
    CHECK(transport.is_open());
    // A round-trip proves the server accepted before the count is checked: the
    // kernel completes the handshake from the listen backlog, so connect()
    // returning does not yet mean the accept handler has run.
    transport.write("*IDN?\n", 2000ms);
    CHECK_FALSE(transport.read_line(2000ms).empty());
    CHECK(server.connections() == 1);

    // Opening an already open transport is a no-op, not a second connection.
    transport.open();
    CHECK(server.connections() == 1);

    transport.close();
    CHECK_FALSE(transport.is_open());
}

TEST_CASE("the transport refuses I/O before it is opened", "[tcp]") {
    TcpTransport transport("127.0.0.1", 1);
    CHECK_THROWS_AS(transport.write("*IDN?\n", 100ms), ConnectionError);
    CHECK_THROWS_AS(transport.read_line(100ms), ConnectionError);
}

TEST_CASE("a transport can be reopened after being closed", "[tcp]") {
    SimulatorServer server;
    TcpTransport transport("127.0.0.1", server.port());

    for (int round = 0; round < 3; ++round) {
        transport.open();
        transport.write("*IDN?\n", 2000ms);
        CHECK(transport.read_line(2000ms) == "Fluke,NORMA5000,KN34512BA,01.05");
        transport.close();
    }
    CHECK(server.connections() == 3);
}

TEST_CASE("stopping the server twice is safe", "[tcp]") {
    SimulatorServer server;
    server.stop();
    CHECK_NOTHROW(server.stop());
}

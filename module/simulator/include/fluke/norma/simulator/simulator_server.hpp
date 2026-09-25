#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <string>

#include "fluke/norma/simulator/norma_simulator.hpp"

namespace fluke::norma::sim {

/// A TCP server that serves a NormaSimulator on a real socket.
///
/// This is what gives TcpTransport genuine test coverage: the client connects,
/// writes and reads over loopback exactly as it would against an instrument.
/// Like the real NORMA, the server accepts one client at a time.
///
/// The server runs its own thread. Because NormaSimulator is not thread-safe,
/// reach into it through with_simulator(), which holds the same lock the
/// connection handler does.
class SimulatorServer {
public:
    /// Binds and starts listening. Port 0 (the default) picks a free port,
    /// reported by port().
    explicit SimulatorServer(std::shared_ptr<NormaSimulator> simulator =
                                 std::make_shared<NormaSimulator>(),
                             std::uint16_t port = 0,
                             std::string address = "127.0.0.1");

    /// Stops the server and joins its thread.
    ~SimulatorServer();

    SimulatorServer(const SimulatorServer&) = delete;
    SimulatorServer& operator=(const SimulatorServer&) = delete;

    /// The port actually bound.
    std::uint16_t port() const;

    /// The address listened on, e.g. "127.0.0.1".
    const std::string& host() const;

    /// Stops accepting and closes any open connection. Idempotent.
    void stop();

    /// Number of clients accepted since the server started.
    std::uint64_t connections() const;

    /// Closes the current client connection without answering, so the client
    /// sees the link drop mid-session.
    void disconnect_client();

    /// While set, received commands are parsed but no response is sent, so a
    /// client's query hits its read timeout.
    void set_stalled(bool stalled);

    /// Runs `action` on the simulator while holding the server's lock.
    void with_simulator(const std::function<void(NormaSimulator&)>& action);

    /// The shared simulator. Safe to configure before the first connection;
    /// use with_simulator() once a client may be connected.
    std::shared_ptr<NormaSimulator> simulator() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace fluke::norma::sim

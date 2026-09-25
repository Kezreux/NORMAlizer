// A loopback TCP server in front of the simulator, so the library's real
// TcpTransport can be exercised end to end.
//
// One io_context on one thread runs an accept loop; each accepted socket is read
// line by line and every line handed to the simulator under a mutex. Only one
// client is served at a time, matching the instrument's single-client socket.

#include "fluke/norma/simulator/simulator_server.hpp"

#include <atomic>
#include <future>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <thread>
#include <utility>

#include <asio.hpp>

namespace fluke::norma::sim {

struct SimulatorServer::Impl {
    std::shared_ptr<NormaSimulator> simulator;
    std::string address;

    asio::io_context io;
    asio::ip::tcp::acceptor acceptor{io};
    std::optional<asio::ip::tcp::socket> client;
    std::string rx;

    std::thread thread;
    std::mutex mutex;              ///< guards `simulator` and `client`
    std::atomic<bool> stalled{false};
    std::atomic<bool> stopping{false};
    std::atomic<std::uint64_t> connections{0};
    std::uint16_t port = 0;

    void accept_next() {
        acceptor.async_accept([this](const asio::error_code& ec, asio::ip::tcp::socket socket) {
            if (ec) {
                return; // the acceptor was closed
            }
            {
                std::lock_guard<std::mutex> lock(mutex);
                asio::error_code ignored;
                socket.set_option(asio::ip::tcp::no_delay(true), ignored);
                client.emplace(std::move(socket));
                rx.clear();
            }
            ++connections;
            read_next();
        });
    }

    void read_next() {
        std::lock_guard<std::mutex> lock(mutex);
        if (!client.has_value()) {
            accept_next();
            return;
        }
        asio::async_read_until(
            *client, asio::dynamic_buffer(rx), '\n',
            [this](const asio::error_code& ec, std::size_t length) { on_line(ec, length); });
    }

    void on_line(const asio::error_code& ec, std::size_t length) {
        std::string response;
        {
            std::lock_guard<std::mutex> lock(mutex);
            if (ec) {
                // The client closed, or we closed on it: wait for the next one.
                if (client.has_value()) {
                    asio::error_code ignored;
                    client->close(ignored);
                    client.reset();
                }
                rx.clear();
                if (!stopping) {
                    accept_next();
                }
                return;
            }

            std::string line = rx.substr(0, length);
            rx.erase(0, length);
            while (!line.empty() && (line.back() == '\n' || line.back() == '\r')) {
                line.pop_back();
            }
            if (auto answer = simulator->handle_line(line)) {
                response = std::move(*answer);
                response.push_back('\n');
            }
        }

        if (!response.empty() && !stalled) {
            std::lock_guard<std::mutex> lock(mutex);
            if (client.has_value()) {
                asio::error_code write_ec;
                asio::write(*client, asio::buffer(response), write_ec);
                if (write_ec) {
                    asio::error_code ignored;
                    client->close(ignored);
                    client.reset();
                    if (!stopping) {
                        accept_next();
                    }
                    return;
                }
            }
        }
        read_next();
    }
};

SimulatorServer::SimulatorServer(std::shared_ptr<NormaSimulator> simulator, std::uint16_t port,
                                 std::string address)
    : impl_(std::make_unique<Impl>()) {
    impl_->simulator = std::move(simulator);
    impl_->address = std::move(address);

    asio::error_code ec;
    const auto ip = asio::ip::make_address(impl_->address, ec);
    if (ec) {
        throw std::invalid_argument("simulator server: bad listen address \"" + impl_->address +
                                    "\": " + ec.message());
    }
    const asio::ip::tcp::endpoint endpoint(ip, port);
    impl_->acceptor.open(endpoint.protocol(), ec);
    if (!ec) {
        impl_->acceptor.set_option(asio::ip::tcp::acceptor::reuse_address(true), ec);
    }
    if (!ec) {
        impl_->acceptor.bind(endpoint, ec);
    }
    if (!ec) {
        impl_->acceptor.listen(asio::socket_base::max_listen_connections, ec);
    }
    if (ec) {
        throw std::runtime_error("simulator server: cannot listen on " + impl_->address + ":" +
                                 std::to_string(port) + ": " + ec.message());
    }
    impl_->port = impl_->acceptor.local_endpoint().port();

    impl_->accept_next();
    impl_->thread = std::thread([impl = impl_.get()] {
        // A work guard is unnecessary: the acceptor always has a pending
        // async_accept, so run() only returns once everything is closed.
        impl->io.run();
    });
}

SimulatorServer::~SimulatorServer() {
    stop();
}

std::uint16_t SimulatorServer::port() const { return impl_->port; }

const std::string& SimulatorServer::host() const { return impl_->address; }

void SimulatorServer::stop() {
    if (impl_->stopping.exchange(true)) {
        if (impl_->thread.joinable()) {
            impl_->thread.join();
        }
        return;
    }
    // Close from inside the io_context thread so no handler runs concurrently
    // with the teardown.
    asio::post(impl_->io, [impl = impl_.get()] {
        asio::error_code ignored;
        impl->acceptor.close(ignored);
        std::lock_guard<std::mutex> lock(impl->mutex);
        if (impl->client.has_value()) {
            impl->client->close(ignored);
            impl->client.reset();
        }
    });
    if (impl_->thread.joinable()) {
        impl_->thread.join();
    }
}

std::uint64_t SimulatorServer::connections() const { return impl_->connections.load(); }

void SimulatorServer::disconnect_client() {
    // Closing happens on the io_context thread, and the caller waits for it, so
    // a test can rely on the connection being gone when this returns.
    std::promise<void> closed;
    std::future<void> wait = closed.get_future();
    asio::post(impl_->io, [impl = impl_.get(), &closed] {
        {
            std::lock_guard<std::mutex> lock(impl->mutex);
            if (impl->client.has_value()) {
                asio::error_code ignored;
                impl->client->shutdown(asio::ip::tcp::socket::shutdown_both, ignored);
                impl->client->close(ignored);
            }
        }
        closed.set_value();
    });
    wait.wait();
}

void SimulatorServer::set_stalled(bool stalled) { impl_->stalled = stalled; }

void SimulatorServer::with_simulator(const std::function<void(NormaSimulator&)>& action) {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    action(*impl_->simulator);
}

std::shared_ptr<NormaSimulator> SimulatorServer::simulator() const { return impl_->simulator; }

} // namespace fluke::norma::sim

#include "fluke/norma/tcp_transport.hpp"

#include <utility>

#include <asio.hpp>

#include "fluke/norma/error.hpp"

namespace fluke::norma {

struct TcpTransport::Impl {
    std::string host;
    std::uint16_t port;
    std::chrono::milliseconds connect_timeout{5000};

    asio::io_context io;
    asio::ip::tcp::socket socket{io};
    std::string rx; // bytes received beyond the last returned line

    /// Runs the io_context until the pending async operation completes or the
    /// timeout expires. On timeout the socket is closed (which cancels the
    /// operation), the handlers are drained, and TimeoutError is thrown.
    void run_operation(std::chrono::milliseconds timeout, const char* what) {
        io.restart();
        io.run_for(timeout);
        if (!io.stopped()) {
            asio::error_code ignored;
            socket.close(ignored);
            io.run();
            throw TimeoutError(std::string(what) + " timed out after " +
                               std::to_string(timeout.count()) + " ms (" + endpoint() + ")");
        }
    }

    std::string endpoint() const { return "tcp://" + host + ":" + std::to_string(port); }
};

TcpTransport::TcpTransport(std::string host, std::uint16_t port)
    : impl_(std::make_unique<Impl>()) {
    impl_->host = std::move(host);
    impl_->port = port;
}

TcpTransport::~TcpTransport() {
    if (impl_ && impl_->socket.is_open()) {
        asio::error_code ignored;
        impl_->socket.close(ignored);
    }
}

void TcpTransport::open() {
    auto& im = *impl_;
    if (im.socket.is_open()) {
        return;
    }

    asio::ip::tcp::resolver resolver(im.io);
    asio::error_code resolve_ec;
    const auto endpoints =
        resolver.resolve(im.host, std::to_string(im.port), resolve_ec);
    if (resolve_ec) {
        throw ConnectionError("cannot resolve host \"" + im.host +
                              "\": " + resolve_ec.message());
    }

    asio::error_code ec = asio::error::would_block;
    asio::async_connect(im.socket, endpoints,
                        [&](const asio::error_code& e, const asio::ip::tcp::endpoint&) {
                            ec = e;
                        });
    im.run_operation(im.connect_timeout, "connect");
    if (ec) {
        asio::error_code ignored;
        im.socket.close(ignored);
        throw ConnectionError("cannot connect to " + im.endpoint() + ": " + ec.message());
    }

    // Commands are short lines; disable Nagle for snappy request/response.
    asio::error_code opt_ec;
    im.socket.set_option(asio::ip::tcp::no_delay(true), opt_ec);
}

void TcpTransport::close() {
    auto& im = *impl_;
    if (im.socket.is_open()) {
        asio::error_code ignored;
        im.socket.shutdown(asio::ip::tcp::socket::shutdown_both, ignored);
        im.socket.close(ignored);
    }
    im.rx.clear();
}

bool TcpTransport::is_open() const {
    return impl_->socket.is_open();
}

void TcpTransport::write(std::string_view data, std::chrono::milliseconds timeout) {
    auto& im = *impl_;
    if (!im.socket.is_open()) {
        throw ConnectionError("not connected (" + im.endpoint() + ")");
    }

    asio::error_code ec = asio::error::would_block;
    asio::async_write(im.socket, asio::buffer(data.data(), data.size()),
                      [&](const asio::error_code& e, std::size_t) { ec = e; });
    im.run_operation(timeout, "write");
    if (ec) {
        throw ConnectionError("write to " + im.endpoint() + " failed: " + ec.message());
    }
}

std::string TcpTransport::read_line(std::chrono::milliseconds timeout) {
    auto& im = *impl_;
    if (!im.socket.is_open()) {
        throw ConnectionError("not connected (" + im.endpoint() + ")");
    }

    asio::error_code ec = asio::error::would_block;
    std::size_t length = 0;
    asio::async_read_until(im.socket, asio::dynamic_buffer(im.rx), '\n',
                           [&](const asio::error_code& e, std::size_t n) {
                               ec = e;
                               length = n;
                           });
    im.run_operation(timeout, "read");
    if (ec) {
        throw ConnectionError("read from " + im.endpoint() + " failed: " + ec.message());
    }

    std::string line = im.rx.substr(0, length);
    im.rx.erase(0, length);
    while (!line.empty() && (line.back() == '\n' || line.back() == '\r')) {
        line.pop_back();
    }
    return line;
}

std::string TcpTransport::description() const {
    return impl_->endpoint();
}

void TcpTransport::set_connect_timeout(std::chrono::milliseconds timeout) {
    impl_->connect_timeout = timeout;
}

std::chrono::milliseconds TcpTransport::connect_timeout() const {
    return impl_->connect_timeout;
}

} // namespace fluke::norma

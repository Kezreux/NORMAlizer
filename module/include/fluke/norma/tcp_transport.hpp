#pragma once

#include <chrono>
#include <cstdint>
#include <memory>
#include <string>

#include "fluke/norma/transport.hpp"
#include "fluke/norma/types.hpp"

namespace fluke::norma {

/// TCP/Ethernet transport (Asio-based) for the NORMA's socket interface.
///
/// The instrument listens on the fixed TCP port 23; commands are ASCII lines
/// terminated with `\n`. Asio is kept out of this header (pimpl) so that
/// consumers of the installed library do not need the Asio headers.
class TcpTransport final : public Transport {
public:
    explicit TcpTransport(std::string host, std::uint16_t port = kDefaultPort);
    ~TcpTransport() override;

    // Neither copyable nor movable: the pimpl owns a live socket and an
    // io_context, and a moved-from object would leave every method with a null
    // impl_ to dereference. Hold it by pointer (as NormaInstrument does, via
    // std::unique_ptr<Transport>) when it needs to change hands.
    TcpTransport(TcpTransport&&) = delete;
    TcpTransport& operator=(TcpTransport&&) = delete;
    TcpTransport(const TcpTransport&) = delete;
    TcpTransport& operator=(const TcpTransport&) = delete;

    void open() override;
    void close() override;
    bool is_open() const override;

    void write(std::string_view data, std::chrono::milliseconds timeout) override;
    std::string read_line(std::chrono::milliseconds timeout) override;

    std::string description() const override;

    void set_connect_timeout(std::chrono::milliseconds timeout);
    std::chrono::milliseconds connect_timeout() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace fluke::norma

#pragma once

#include <chrono>
#include <string>
#include <string_view>

namespace fluke::norma {

/// Abstraction of the byte transport carrying SCPI lines.
///
/// The NORMA speaks the same SCPI command set over TCP (port 23), RS-232, USB
/// (VCP) and GPIB. This library ships a TCP implementation (TcpTransport);
/// other transports can be added by implementing this interface, and tests use
/// a scripted mock.
class Transport {
public:
    virtual ~Transport() = default;

    virtual void open() = 0;
    virtual void close() = 0;
    virtual bool is_open() const = 0;

    /// Sends raw bytes. `data` must already include the `\n` terminator.
    virtual void write(std::string_view data, std::chrono::milliseconds timeout) = 0;

    /// Reads one response line, i.e. until `\n`, and returns it without the
    /// trailing `\r`/`\n`.
    virtual std::string read_line(std::chrono::milliseconds timeout) = 0;

    /// Human-readable endpoint description, e.g. "tcp://192.168.1.100:23".
    virtual std::string description() const = 0;
};

} // namespace fluke::norma

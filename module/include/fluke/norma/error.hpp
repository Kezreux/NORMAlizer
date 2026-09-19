#pragma once

#include <stdexcept>
#include <string>

namespace fluke::norma {

/// Base class for all errors thrown by this library.
class Error : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

/// TCP connect/read/write failed (host unreachable, connection reset, ...).
class ConnectionError : public Error {
public:
    using Error::Error;
};

/// An I/O operation did not complete within the configured timeout.
class TimeoutError : public Error {
public:
    using Error::Error;
};

/// The instrument answered, but the response could not be parsed.
class ProtocolError : public Error {
public:
    using Error::Error;
};

/// An error reported by the instrument itself via `SYSTem:ERRor?`
/// (negative codes are SCPI standard errors, e.g. -113 "Undefined header").
class ScpiError : public Error {
public:
    ScpiError(int code, std::string message)
        : Error("SCPI error " + std::to_string(code) + ": " + message),
          code_(code),
          message_(std::move(message)) {}

    int code() const noexcept { return code_; }
    const std::string& message() const noexcept { return message_; }

private:
    int code_;
    std::string message_;
};

} // namespace fluke::norma

#pragma once

#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "fluke/norma/types.hpp"

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
///
/// One command line can queue several errors, so the whole drained queue is
/// carried along in `all()`; `code()`/`message()` describe the first (oldest)
/// entry, which is the one that usually explains the rest.
class ScpiError : public Error {
public:
    ScpiError(int code, std::string message)
        : Error(format(code, message, 1)), errors_{ScpiErrorInfo{code, std::move(message)}} {}

    explicit ScpiError(std::vector<ScpiErrorInfo> errors)
        : Error(errors.empty() ? "SCPI error (empty error queue)"
                               : format(errors.front().code, errors.front().message, errors.size())),
          errors_(std::move(errors)) {
        if (errors_.empty()) {
            errors_.push_back(ScpiErrorInfo{});
        }
    }

    /// Code of the first queued error (negative for SCPI standard errors).
    int code() const noexcept { return errors_.front().code; }

    /// Message of the first queued error, without its surrounding quotes.
    const std::string& message() const noexcept { return errors_.front().message; }

    /// Every error drained from the queue, oldest first.
    const std::vector<ScpiErrorInfo>& all() const noexcept { return errors_; }

private:
    static std::string format(int code, const std::string& message, std::size_t count) {
        std::string text = "SCPI error " + std::to_string(code) + ": " + message;
        if (count > 1) {
            text += " (and " + std::to_string(count - 1) + " more in the queue)";
        }
        return text;
    }

    std::vector<ScpiErrorInfo> errors_;
};

} // namespace fluke::norma

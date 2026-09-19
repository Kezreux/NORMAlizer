#pragma once

#include <chrono>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

#include "fluke/norma/error.hpp"
#include "fluke/norma/transport.hpp"
#include "fluke/norma/types.hpp"

namespace fluke::norma {

/// Line-oriented SCPI client on top of a Transport.
///
/// Implements the wire protocol described in Fluke-NORMA-TCP-API.md:
/// commands are ASCII lines terminated with `\n`; queries (ending in `?`)
/// return exactly one line terminated with `\n` (a leading `\r` is stripped).
///
/// All commands from the manual are reachable through send()/query(); the
/// typed helpers only add parsing. A mutex makes each send/query atomic so a
/// query's response cannot interleave with another thread's command.
class ScpiClient {
public:
    explicit ScpiClient(std::unique_ptr<Transport> transport,
                        std::chrono::milliseconds default_timeout = std::chrono::milliseconds(5000));

    void open();
    void close();
    bool is_open() const;

    /// Sends a setting command (no response expected), e.g. "*RST".
    void send(std::string_view command);

    /// Sends a query and reads one response line (stripped of `\r\n`).
    std::string query(std::string_view command);
    std::string query(std::string_view command, std::chrono::milliseconds timeout);

    /// Query helpers with parsing.
    double query_value(std::string_view command);
    std::vector<double> query_values(std::string_view command);
    int query_int(std::string_view command);
    bool query_bool(std::string_view command);

    /// Drains the instrument error queue (`SYST:ERR?` until 0,"No error").
    std::vector<ScpiErrorInfo> read_all_errors();

    /// Throws ScpiError for the first queued instrument error, if any.
    void throw_if_error();

    void set_default_timeout(std::chrono::milliseconds timeout);
    std::chrono::milliseconds default_timeout() const;

    Transport& transport() { return *transport_; }
    const Transport& transport() const { return *transport_; }

    // -- Formatting/parsing helpers (pure functions, unit-tested) ------------

    /// Wraps a string parameter in double quotes, e.g. VOLT1 -> "VOLT1".
    static std::string quote(std::string_view s);

    /// Strips one pair of surrounding quotes (single or double), if present.
    static std::string unquote(std::string_view s);

    /// Splits a comma-separated response line; commas inside quoted strings
    /// do not split. Fields are trimmed of surrounding whitespace.
    static std::vector<std::string> split_csv(std::string_view line);

    /// Parses one numeric field. SCPI NaN (|x| >= 9.91e37) becomes NaN.
    static double to_double(std::string_view field);

    /// Parses a comma-separated list of numeric values.
    static std::vector<double> parse_values(std::string_view line);

    /// Parses a `SYST:ERR?` response such as `-113,"Undefined header"`.
    static ScpiErrorInfo parse_error(std::string_view line);

    /// Formats a double for use as an SCPI parameter.
    static std::string format_double(double value);

private:
    std::unique_ptr<Transport> transport_;
    std::chrono::milliseconds timeout_;
    mutable std::mutex mutex_;
};

} // namespace fluke::norma

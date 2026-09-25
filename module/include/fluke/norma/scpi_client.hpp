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
    ///
    /// Throws std::invalid_argument if the command contains a line terminator:
    /// that would smuggle a second command onto the wire whose response nobody
    /// reads, leaving every later query one response behind.
    void send(std::string_view command);

    /// Sends a query and reads one response line (stripped of `\r\n`).
    std::string query(std::string_view command);
    std::string query(std::string_view command, std::chrono::milliseconds timeout);

    /// Query helpers with parsing.
    double query_value(std::string_view command);
    std::vector<double> query_values(std::string_view command);
    int query_int(std::string_view command);
    bool query_bool(std::string_view command);

    /// Splits a comma-separated response into unquoted fields.
    std::vector<std::string> query_strings(std::string_view command);

    /// Drains the instrument error queue (`SYST:ERR?` until 0,"No error").
    /// Throws ProtocolError if the queue does not drain within
    /// kMaxErrorQueueDrain reads, which means the link is misbehaving rather
    /// than the queue being long.
    std::vector<ScpiErrorInfo> read_all_errors();

    /// Throws ScpiError (carrying the whole queue) if any error was queued.
    void throw_if_error();

    void set_default_timeout(std::chrono::milliseconds timeout);
    std::chrono::milliseconds default_timeout() const;

    Transport& transport() { return *transport_; }
    const Transport& transport() const { return *transport_; }

    /// Upper bound on the reads read_all_errors() performs before giving up.
    static constexpr int kMaxErrorQueueDrain = 64;

    // -- Formatting/parsing helpers (pure functions, unit-tested) ------------

    /// Wraps a string parameter in double quotes, e.g. VOLT1 -> "VOLT1".
    static std::string quote(std::string_view s);

    /// Strips one pair of surrounding quotes (single or double), if present.
    static std::string unquote(std::string_view s);

    /// Comma-joins values, each wrapped in double quotes:
    /// {"VOLT1","CURR1"} -> `"VOLT1","CURR1"`.
    static std::string quote_join(const std::vector<std::string>& values);

    /// Comma-joins values verbatim.
    static std::string join(const std::vector<std::string>& values);

    /// Splits a comma-separated response line; commas inside quoted strings
    /// do not split. Fields are trimmed of surrounding whitespace.
    static std::vector<std::string> split_csv(std::string_view line);

    /// Splits the response to a compound query (`A?;:B?`) on its semicolons;
    /// semicolons inside quoted strings do not split.
    static std::vector<std::string> split_semicolons(std::string_view line);

    /// Parses one numeric field. SCPI NaN (|x| >= kScpiNanThreshold) becomes NaN.
    static double to_double(std::string_view field);

    /// Parses one integer field (SCPI returns booleans as 0/1).
    static int to_int(std::string_view field);

    /// Parses a boolean response: 0/1 as well as the ON/OFF keywords.
    static bool to_bool(std::string_view field);

    /// Parses a comma-separated list of numeric values.
    static std::vector<double> parse_values(std::string_view line);

    /// Parses a `SYST:ERR?` response such as `-113,"Undefined header"`.
    static ScpiErrorInfo parse_error(std::string_view line);

    /// Parses a `SYST:ERR:ALL?` response: several `<code>,"<message>"` pairs on
    /// one line.
    static std::vector<ScpiErrorInfo> parse_errors(std::string_view line);

    /// Formats a double for use as an SCPI parameter.
    static std::string format_double(double value);

    /// Formats a boolean as the ON/OFF keyword the instrument echoes back.
    static std::string format_bool(bool value);

    /// True if the command contains a `\r` or `\n`, i.e. would put more than
    /// one line on the wire.
    static bool has_line_terminator(std::string_view command);

private:
    void write_line(std::string_view command, std::chrono::milliseconds timeout);

    std::unique_ptr<Transport> transport_;
    std::chrono::milliseconds timeout_;
    mutable std::mutex mutex_;
};

} // namespace fluke::norma

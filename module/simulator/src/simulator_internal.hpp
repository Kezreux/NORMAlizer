#pragma once

// Shared internals of the simulator: the string helpers, the header token type
// and the parsed-command type the subsystem handlers operate on. Internal to
// the simulator target — this header is not installed.

#include <cctype>
#include <cstdlib>
#include <initializer_list>
#include <optional>
#include <utility>
#include <string>
#include <string_view>
#include <vector>

#include "fluke/norma/simulator/norma_simulator.hpp"

namespace fluke::norma::sim {

namespace detail {

std::string_view trim(std::string_view s);
std::string upper(std::string_view s);

/// Splits on `separator`, ignoring separators inside quoted strings.
std::vector<std::string> split_unquoted(std::string_view text, char separator);

/// Strips one pair of surrounding quotes, if present.
std::string unquote(std::string_view s);

/// True when `token` is an accepted spelling of `mnemonic`, which is written in
/// the manual's convention (upper-case prefix = short form), e.g. "VOLTage".
bool mnemonic_matches(const std::string& token, std::string_view mnemonic);

/// Formats a numeric setting for a query response: compact, no unit, with the
/// uppercase exponent the instrument uses.
std::string format_setting(double value);

/// Formats a boolean response, which SCPI defines as 0 or 1.
std::string format_bool(bool value);

/// Comma-joins quoted <function> names for a FUNCtion? style response.
std::string quote_join(const std::vector<std::string>& values);

/// Comma-joins numbers for a list response such as RANGe:LIST?.
std::string join_values(const std::vector<double>& values);

/// Parses a numeric parameter, rejecting trailing garbage (the manual states
/// that no unit is accepted). Returns false and leaves `out` alone on failure.
bool parse_number(const std::string& text, double& out);

/// Parses a boolean parameter: ON/OFF or any number (0 = off).
bool parse_boolean(const std::string& text, bool& out);

} // namespace detail

/// One node of a received header: its mnemonic text and its numeric suffix.
struct Token {
    std::string text;   ///< uppercased mnemonic, suffix stripped
    int suffix = 0;
    bool has_suffix = false;
};

/// Splits the trailing digits of a header node into a suffix: "VOLT1" -> VOLT/1.
Token make_token(std::string_view node);

/// One command out of a command line, after the header was matched against the
/// simulator's command table.
struct NormaSimulator::Command {
    std::vector<Token> tokens;         ///< the received header, node by node
    std::string key;                   ///< canonical key from the command table
    std::vector<int> suffixes;         ///< suffixes of the nodes that were present
    bool query = false;
    std::vector<std::string> args;     ///< parameters, trimmed, quotes kept
    std::optional<std::string> response;
    bool failed = false;               ///< an error was queued; abort the line

    /// Suffix of the n-th suffixable node, defaulting to 1 as the manual does
    /// ("if the channel suffix is omitted, the command applies to input 1").
    int suffix(std::size_t index = 0) const {
        if (index >= suffixes.size() || suffixes[index] == 0) {
            return 1;
        }
        return suffixes[index];
    }

    /// Suffix of the n-th suffixable node, defaulting to 0 (the aggregate form).
    int aggregate_suffix(std::size_t index = 0) const {
        return index < suffixes.size() ? suffixes[index] : 0;
    }

    /// The single expected parameter, unquoted, or std::nullopt when absent.
    std::optional<std::string> argument(std::size_t index = 0) const {
        if (index >= args.size() || args[index].empty()) {
            return std::nullopt;
        }
        return detail::unquote(args[index]);
    }
};

/// Shared read/write plumbing for the subsystem handlers.
///
/// Every setting in the manual follows the same shape — a query returns the
/// current value, a setting command validates its parameter — so the handlers
/// express a command as one call on this helper. A rejected parameter is queued
/// as the SCPI error a real instrument would report and marks the command
/// failed, which aborts the rest of the command line just as the firmware does.
struct Access {
    NormaSimulator& sim;
    NormaSimulator::Command& command;

    /// The command's single parameter, or nullopt after queuing -109.
    std::optional<std::string> parameter();

    /// Queues an error and marks the command failed.
    void fail(int code, std::string message);

    void respond(std::string text) { command.response = std::move(text); }

    /// Reads or writes a double, rejecting values outside [min, max].
    void number(double& field, double min, double max);

    /// Reads or writes an integer, rejecting values outside [min, max].
    void integer(int& field, int min, int max);

    /// Reads or writes a boolean (ON/OFF on the way in, 0/1 on the way out).
    void boolean(bool& field);

    /// Reads or writes a quoted-string list of <function> names.
    void function_list(std::vector<std::string>& field);

    /// Parses a quoted-string list of <function> names out of the parameters,
    /// whether or not this is a query — the data queries take such a list too.
    /// Returns false after queuing an error for a malformed list.
    bool read_function_list(std::vector<std::string>& out);

    /// True when this is a query; otherwise queues -113, because a query-only
    /// node has no setting form.
    bool query_only();

    /// True when this is a setting command; otherwise queues -113, because an
    /// event command such as *RST or ABORt has no query form.
    bool command_only();

    /// Reads or writes character data. `spellings` maps each enum value to its
    /// accepted spellings; the first is the short form returned on a query.
    template <typename Enum>
    void keyword(
        Enum& field,
        std::initializer_list<std::pair<Enum, std::initializer_list<const char*>>> spellings) {
        if (command.query) {
            for (const auto& [value, names] : spellings) {
                if (value == field) {
                    respond(*names.begin());
                    return;
                }
            }
            respond("");
            return;
        }
        auto text = parameter();
        if (!text) {
            return;
        }
        const std::string wanted = detail::upper(*text);
        for (const auto& [value, names] : spellings) {
            for (const char* name : names) {
                if (wanted == detail::upper(name)) {
                    field = value;
                    return;
                }
            }
        }
        fail(error_code::kIllegalParameterValue, "Illegal parameter value;" + *text);
    }
};

} // namespace fluke::norma::sim

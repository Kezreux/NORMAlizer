#include "fluke/norma/scpi_client.hpp"

#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <stdexcept>
#include <utility>

namespace fluke::norma {

namespace {

std::string_view trim(std::string_view s) {
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.front()))) {
        s.remove_prefix(1);
    }
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back()))) {
        s.remove_suffix(1);
    }
    return s;
}

/// Splits `line` on every occurrence of `separator` that is not inside a
/// quoted string, trimming each field. An empty (or blank) line yields no
/// fields, matching "no values defined" responses.
std::vector<std::string> split_unquoted(std::string_view line, char separator) {
    std::vector<std::string> fields;
    if (trim(line).empty()) {
        return fields;
    }

    std::string current;
    char quote_char = 0;
    for (const char c : line) {
        if (quote_char != 0) {
            current.push_back(c);
            if (c == quote_char) {
                quote_char = 0;
            }
        } else if (c == '"' || c == '\'') {
            quote_char = c;
            current.push_back(c);
        } else if (c == separator) {
            fields.emplace_back(trim(current));
            current.clear();
        } else {
            current.push_back(c);
        }
    }
    fields.emplace_back(trim(current));
    return fields;
}

/// Uppercases an ASCII keyword so ON/on/On compare equal.
std::string upper(std::string_view s) {
    std::string out(trim(s));
    for (char& c : out) {
        c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    }
    return out;
}

} // namespace

ScpiClient::ScpiClient(std::unique_ptr<Transport> transport,
                       std::chrono::milliseconds default_timeout)
    : transport_(std::move(transport)), timeout_(default_timeout) {}

void ScpiClient::open() { transport_->open(); }
void ScpiClient::close() { transport_->close(); }
bool ScpiClient::is_open() const { return transport_->is_open(); }

bool ScpiClient::has_line_terminator(std::string_view command) {
    return command.find('\n') != std::string_view::npos ||
           command.find('\r') != std::string_view::npos;
}

void ScpiClient::write_line(std::string_view command, std::chrono::milliseconds timeout) {
    if (has_line_terminator(command)) {
        throw std::invalid_argument(
            "SCPI command must be a single line (it contains a carriage return or line "
            "feed); use separate calls, or a ';'-separated command line");
    }
    std::string line;
    line.reserve(command.size() + 1);
    line.append(command);
    line.push_back('\n');
    transport_->write(line, timeout);
}

void ScpiClient::send(std::string_view command) {
    std::lock_guard<std::mutex> lock(mutex_);
    write_line(command, timeout_);
}

std::string ScpiClient::query(std::string_view command) {
    return query(command, timeout_);
}

std::string ScpiClient::query(std::string_view command, std::chrono::milliseconds timeout) {
    std::lock_guard<std::mutex> lock(mutex_);
    write_line(command, timeout);
    return transport_->read_line(timeout);
}

double ScpiClient::query_value(std::string_view command) {
    return to_double(query(command));
}

std::vector<double> ScpiClient::query_values(std::string_view command) {
    return parse_values(query(command));
}

int ScpiClient::query_int(std::string_view command) {
    return to_int(query(command));
}

bool ScpiClient::query_bool(std::string_view command) {
    return to_bool(query(command));
}

std::vector<std::string> ScpiClient::query_strings(std::string_view command) {
    std::vector<std::string> values;
    for (const auto& field : split_csv(query(command))) {
        values.push_back(unquote(field));
    }
    return values;
}

std::vector<ScpiErrorInfo> ScpiClient::read_all_errors() {
    std::vector<ScpiErrorInfo> errors;
    for (int i = 0; i < kMaxErrorQueueDrain; ++i) {
        ScpiErrorInfo entry = parse_error(query("SYST:ERR?"));
        if (entry.code == 0) {
            return errors;
        }
        errors.push_back(std::move(entry));
    }
    // The queue is finite, so still reporting errors after this many reads means
    // the link is out of step rather than the queue being long. Reporting that
    // as "no more errors" would hide a broken connection.
    throw ProtocolError("SYST:ERR? still reported errors after " +
                        std::to_string(kMaxErrorQueueDrain) +
                        " reads; the error queue is not draining");
}

void ScpiClient::throw_if_error() {
    auto errors = read_all_errors();
    if (!errors.empty()) {
        throw ScpiError(std::move(errors));
    }
}

void ScpiClient::set_default_timeout(std::chrono::milliseconds timeout) {
    timeout_ = timeout;
}

std::chrono::milliseconds ScpiClient::default_timeout() const {
    return timeout_;
}

// -- Formatting/parsing helpers ----------------------------------------------

std::string ScpiClient::quote(std::string_view s) {
    std::string out;
    out.reserve(s.size() + 2);
    out.push_back('"');
    out.append(s);
    out.push_back('"');
    return out;
}

std::string ScpiClient::unquote(std::string_view s) {
    s = trim(s);
    if (s.size() >= 2 && (s.front() == '"' || s.front() == '\'') && s.back() == s.front()) {
        s = s.substr(1, s.size() - 2);
    }
    return std::string(s);
}

std::string ScpiClient::quote_join(const std::vector<std::string>& values) {
    std::string out;
    for (std::size_t i = 0; i < values.size(); ++i) {
        if (i != 0) {
            out += ',';
        }
        out += quote(values[i]);
    }
    return out;
}

std::string ScpiClient::join(const std::vector<std::string>& values) {
    std::string out;
    for (std::size_t i = 0; i < values.size(); ++i) {
        if (i != 0) {
            out += ',';
        }
        out += values[i];
    }
    return out;
}

std::vector<std::string> ScpiClient::split_csv(std::string_view line) {
    return split_unquoted(line, ',');
}

std::vector<std::string> ScpiClient::split_semicolons(std::string_view line) {
    return split_unquoted(line, ';');
}

double ScpiClient::to_double(std::string_view field) {
    const std::string text(trim(field));
    if (text.empty()) {
        throw ProtocolError("expected numeric value, got empty field");
    }
    char* end = nullptr;
    const double value = std::strtod(text.c_str(), &end);
    if (end == text.c_str()) {
        throw ProtocolError("expected numeric value, got \"" + text + "\"");
    }
    // SCPI encodes NaN as 9.91e37 (measurement status Undefined / Not available).
    if (std::fabs(value) >= kScpiNanThreshold) {
        return std::numeric_limits<double>::quiet_NaN();
    }
    return value;
}

int ScpiClient::to_int(std::string_view field) {
    const std::string text(trim(field));
    char* end = nullptr;
    const long value = std::strtol(text.c_str(), &end, 10);
    if (end == text.c_str()) {
        throw ProtocolError("expected integer response, got \"" + text + "\"");
    }
    return static_cast<int>(value);
}

bool ScpiClient::to_bool(std::string_view field) {
    const std::string text = upper(field);
    if (text == "ON" || text == "TRUE") {
        return true;
    }
    if (text == "OFF" || text == "FALSE") {
        return false;
    }
    return to_int(field) != 0;
}

std::vector<double> ScpiClient::parse_values(std::string_view line) {
    std::vector<double> values;
    for (const auto& field : split_csv(line)) {
        values.push_back(to_double(field));
    }
    return values;
}

ScpiErrorInfo ScpiClient::parse_error(std::string_view line) {
    const auto fields = split_csv(line);
    if (fields.empty()) {
        throw ProtocolError("empty response to SYST:ERR?");
    }
    ScpiErrorInfo info;
    char* end = nullptr;
    info.code = static_cast<int>(std::strtol(fields[0].c_str(), &end, 10));
    if (end == fields[0].c_str()) {
        throw ProtocolError("malformed SYST:ERR? response: \"" + std::string(line) + "\"");
    }
    if (fields.size() > 1) {
        info.message = unquote(fields[1]);
    }
    return info;
}

std::vector<ScpiErrorInfo> ScpiClient::parse_errors(std::string_view line) {
    const auto fields = split_csv(line);
    std::vector<ScpiErrorInfo> errors;
    // SYST:ERR:ALL? answers with <code>,"<message>" pairs on a single line.
    for (std::size_t i = 0; i + 1 < fields.size(); i += 2) {
        ScpiErrorInfo info;
        char* end = nullptr;
        info.code = static_cast<int>(std::strtol(fields[i].c_str(), &end, 10));
        if (end == fields[i].c_str()) {
            throw ProtocolError("malformed SYST:ERR:ALL? response: \"" + std::string(line) + "\"");
        }
        info.message = unquote(fields[i + 1]);
        if (info.code == 0) {
            continue; // 0,"No error" — the queue was empty
        }
        errors.push_back(std::move(info));
    }
    if (errors.empty() && fields.size() % 2 != 0) {
        throw ProtocolError("malformed SYST:ERR:ALL? response: \"" + std::string(line) + "\"");
    }
    return errors;
}

std::string ScpiClient::format_double(double value) {
    char buffer[64];
    std::snprintf(buffer, sizeof(buffer), "%.9g", value);
    return buffer;
}

std::string ScpiClient::format_bool(bool value) {
    return value ? "ON" : "OFF";
}

} // namespace fluke::norma

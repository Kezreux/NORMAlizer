#include "fluke/norma/scpi_client.hpp"

#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <limits>
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

} // namespace

ScpiClient::ScpiClient(std::unique_ptr<Transport> transport,
                       std::chrono::milliseconds default_timeout)
    : transport_(std::move(transport)), timeout_(default_timeout) {}

void ScpiClient::open() { transport_->open(); }
void ScpiClient::close() { transport_->close(); }
bool ScpiClient::is_open() const { return transport_->is_open(); }

void ScpiClient::send(std::string_view command) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::string line;
    line.reserve(command.size() + 1);
    line.append(command);
    line.push_back('\n');
    transport_->write(line, timeout_);
}

std::string ScpiClient::query(std::string_view command) {
    return query(command, timeout_);
}

std::string ScpiClient::query(std::string_view command, std::chrono::milliseconds timeout) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::string line;
    line.reserve(command.size() + 1);
    line.append(command);
    line.push_back('\n');
    transport_->write(line, timeout);
    return transport_->read_line(timeout);
}

double ScpiClient::query_value(std::string_view command) {
    return to_double(query(command));
}

std::vector<double> ScpiClient::query_values(std::string_view command) {
    return parse_values(query(command));
}

int ScpiClient::query_int(std::string_view command) {
    const std::string response = query(command);
    const std::string field(trim(response));
    char* end = nullptr;
    const long value = std::strtol(field.c_str(), &end, 10);
    if (end == field.c_str()) {
        throw ProtocolError("expected integer response, got \"" + response + "\"");
    }
    return static_cast<int>(value);
}

bool ScpiClient::query_bool(std::string_view command) {
    return query_int(command) != 0;
}

std::vector<ScpiErrorInfo> ScpiClient::read_all_errors() {
    std::vector<ScpiErrorInfo> errors;
    // Safety cap: the queue is finite, but never loop forever on a
    // misbehaving connection.
    for (int i = 0; i < 64; ++i) {
        ScpiErrorInfo entry = parse_error(query("SYST:ERR?"));
        if (entry.code == 0) {
            break;
        }
        errors.push_back(std::move(entry));
    }
    return errors;
}

void ScpiClient::throw_if_error() {
    const auto errors = read_all_errors();
    if (!errors.empty()) {
        throw ScpiError(errors.front().code, errors.front().message);
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

std::vector<std::string> ScpiClient::split_csv(std::string_view line) {
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
        } else if (c == ',') {
            fields.emplace_back(trim(current));
            current.clear();
        } else {
            current.push_back(c);
        }
    }
    fields.emplace_back(trim(current));
    return fields;
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
    // SCPI encodes NaN as 9.91e37 (e.g. status Undefined / Not available).
    if (std::fabs(value) >= 9.9e37) {
        return std::numeric_limits<double>::quiet_NaN();
    }
    return value;
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

std::string ScpiClient::format_double(double value) {
    char buffer[64];
    std::snprintf(buffer, sizeof(buffer), "%.9g", value);
    return buffer;
}

} // namespace fluke::norma

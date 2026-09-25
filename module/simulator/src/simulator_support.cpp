// String, formatting and parameter-parsing helpers shared by the simulator's
// parser and its subsystem handlers, plus the Access plumbing the handlers use
// to express one setting or query.

#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>

#include "simulator_internal.hpp"

namespace fluke::norma::sim {

namespace detail {

std::string_view trim(std::string_view s) {
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.front()))) {
        s.remove_prefix(1);
    }
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back()))) {
        s.remove_suffix(1);
    }
    return s;
}

std::string upper(std::string_view s) {
    std::string out(s);
    for (char& c : out) {
        c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    }
    return out;
}

std::vector<std::string> split_unquoted(std::string_view text, char separator) {
    std::vector<std::string> parts;
    std::string current;
    char quote = 0;
    for (char c : text) {
        if (quote != 0) {
            current.push_back(c);
            if (c == quote) {
                quote = 0;
            }
        } else if (c == '"' || c == '\'') {
            quote = c;
            current.push_back(c);
        } else if (c == separator) {
            parts.push_back(current);
            current.clear();
        } else {
            current.push_back(c);
        }
    }
    parts.push_back(current);
    return parts;
}

std::string unquote(std::string_view s) {
    s = trim(s);
    if (s.size() >= 2 && (s.front() == '"' || s.front() == '\'') && s.back() == s.front()) {
        s = s.substr(1, s.size() - 2);
    }
    return std::string(s);
}

bool mnemonic_matches(const std::string& token, std::string_view mnemonic) {
    // The manual writes mnemonics with the short form in upper case
    // ("VOLTage"), and SCPI accepts exactly the short form or the whole long
    // form — nothing in between.
    std::string short_form;
    for (char c : mnemonic) {
        if (std::isupper(static_cast<unsigned char>(c)) ||
            std::isdigit(static_cast<unsigned char>(c))) {
            short_form.push_back(c);
        } else {
            break;
        }
    }
    return token == short_form || token == upper(mnemonic);
}

std::string format_setting(double value) {
    char buffer[64];
    std::snprintf(buffer, sizeof(buffer), "%.9g", value);
    std::string out(buffer);
    for (char& c : out) {
        if (c == 'e') {
            c = 'E';
        }
    }
    return out;
}

std::string format_bool(bool value) { return value ? "1" : "0"; }

std::string quote_join(const std::vector<std::string>& values) {
    std::string out;
    for (std::size_t i = 0; i < values.size(); ++i) {
        if (i != 0) {
            out += ',';
        }
        out += '"' + values[i] + '"';
    }
    return out;
}

std::string join_values(const std::vector<double>& values) {
    std::string out;
    for (std::size_t i = 0; i < values.size(); ++i) {
        if (i != 0) {
            out += ',';
        }
        out += format_setting(values[i]);
    }
    return out;
}

bool parse_number(const std::string& text, double& out) {
    if (text.empty()) {
        return false;
    }
    char* end = nullptr;
    const double value = std::strtod(text.c_str(), &end);
    if (end == text.c_str()) {
        return false;
    }
    // No unit is accepted, so anything after the number is an error.
    if (!trim(std::string_view(end)).empty()) {
        return false;
    }
    out = value;
    return true;
}

bool parse_boolean(const std::string& text, bool& out) {
    const std::string value = upper(trim(text));
    if (value == "ON") {
        out = true;
        return true;
    }
    if (value == "OFF") {
        out = false;
        return true;
    }
    double number = 0.0;
    if (parse_number(text, number)) {
        out = number != 0.0;
        return true;
    }
    return false;
}

} // namespace detail

Token make_token(std::string_view node) {
    Token token;
    std::string text = detail::upper(detail::trim(node));
    std::size_t end = text.size();
    while (end > 0 && std::isdigit(static_cast<unsigned char>(text[end - 1]))) {
        --end;
    }
    // A node is a mnemonic followed by an optional suffix, so digits only count
    // as a suffix when something precedes them.
    if (end < text.size() && end > 0) {
        token.suffix = std::atoi(text.c_str() + end);
        token.has_suffix = true;
        text.resize(end);
    }
    token.text = std::move(text);
    return token;
}

// -- Access ------------------------------------------------------------------------

std::optional<std::string> Access::parameter() {
    auto value = command.argument(0);
    if (!value.has_value()) {
        fail(error_code::kMissingParameter, "Missing parameter");
    }
    return value;
}

void Access::fail(int code, std::string message) {
    sim.push_error(code, std::move(message));
    command.failed = true;
}

void Access::number(double& field, double min, double max) {
    if (command.query) {
        respond(detail::format_setting(field));
        return;
    }
    auto text = parameter();
    if (!text) {
        return;
    }
    double value = 0.0;
    if (!detail::parse_number(*text, value)) {
        fail(error_code::kIllegalParameterValue, "Illegal parameter value;" + *text);
        return;
    }
    if (value < min || value > max) {
        fail(error_code::kDataOutOfRange, "Data out of range;" + *text);
        return;
    }
    field = value;
}

void Access::integer(int& field, int min, int max) {
    if (command.query) {
        respond(std::to_string(field));
        return;
    }
    auto text = parameter();
    if (!text) {
        return;
    }
    double value = 0.0;
    if (!detail::parse_number(*text, value)) {
        fail(error_code::kIllegalParameterValue, "Illegal parameter value;" + *text);
        return;
    }
    // "Values exceeding the instrument's resolution are rounded up or down."
    const int rounded = static_cast<int>(std::lround(value));
    if (rounded < min || rounded > max) {
        fail(error_code::kDataOutOfRange, "Data out of range;" + *text);
        return;
    }
    field = rounded;
}

void Access::boolean(bool& field) {
    if (command.query) {
        respond(detail::format_bool(field));
        return;
    }
    auto text = parameter();
    if (!text) {
        return;
    }
    bool value = false;
    if (!detail::parse_boolean(*text, value)) {
        fail(error_code::kIllegalParameterValue, "Illegal parameter value;" + *text);
        return;
    }
    field = value;
}

bool Access::read_function_list(std::vector<std::string>& out) {
    if (command.args.empty()) {
        fail(error_code::kMissingParameter, "Missing parameter");
        return false;
    }
    std::vector<std::string> functions;
    for (const std::string& argument : command.args) {
        const std::string_view raw = detail::trim(argument);
        // <function> is STRING PROGRAM DATA: it has to be quoted.
        if (raw.size() < 2 || (raw.front() != '"' && raw.front() != '\'')) {
            fail(error_code::kIllegalParameterValue, "Illegal parameter value;" + std::string(raw));
            return false;
        }
        functions.push_back(detail::upper(detail::unquote(raw)));
    }
    out = std::move(functions);
    return true;
}

void Access::function_list(std::vector<std::string>& field) {
    if (command.query) {
        respond(detail::quote_join(field));
        return;
    }
    read_function_list(field);
}

bool Access::query_only() {
    if (!command.query) {
        fail(error_code::kUndefinedHeader, "Undefined header;" + command.key);
        return false;
    }
    return true;
}

bool Access::command_only() {
    if (command.query) {
        fail(error_code::kUndefinedHeader, "Undefined header;" + command.key + "?");
        return false;
    }
    return true;
}

} // namespace fluke::norma::sim

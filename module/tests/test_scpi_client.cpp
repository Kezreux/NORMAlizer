// The pure formatting and parsing helpers of ScpiClient, at their edges.
// Everything here is a static function, so these tests need no transport.

#include <cmath>
#include <limits>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <fluke/norma/scpi_client.hpp>
#include <fluke/norma/types.hpp>

using fluke::norma::kScpiNan;
using fluke::norma::ProtocolError;
using fluke::norma::ScpiClient;

TEST_CASE("quote wraps in double quotes") {
    CHECK(ScpiClient::quote("VOLT1") == "\"VOLT1\"");
    CHECK(ScpiClient::quote("") == "\"\"");
}

TEST_CASE("unquote strips one pair of quotes") {
    CHECK(ScpiClient::unquote("\"3W\"") == "3W");
    CHECK(ScpiClient::unquote("'3W'") == "3W");
    CHECK(ScpiClient::unquote("  \"3W\"  ") == "3W");
    CHECK(ScpiClient::unquote("3W") == "3W");
    CHECK(ScpiClient::unquote("\"unterminated") == "\"unterminated");
    CHECK(ScpiClient::unquote("\"\"").empty());
    CHECK(ScpiClient::unquote("\"") == "\"");
    // Only the outermost pair comes off.
    CHECK(ScpiClient::unquote("\"'3W'\"") == "'3W'");
}

TEST_CASE("quote_join and join build parameter lists") {
    CHECK(ScpiClient::quote_join({"VOLT1", "CURR1"}) == "\"VOLT1\",\"CURR1\"");
    CHECK(ScpiClient::quote_join({}).empty());
    CHECK(ScpiClient::quote_join({"VOLT1"}) == "\"VOLT1\"");
    CHECK(ScpiClient::join({"1", "2", "3"}) == "1,2,3");
    CHECK(ScpiClient::join({}).empty());
}

TEST_CASE("split_csv splits on commas outside quotes") {
    const auto fields = ScpiClient::split_csv("221.56, 1.056,230.65");
    REQUIRE(fields.size() == 3);
    CHECK(fields[0] == "221.56");
    CHECK(fields[1] == "1.056");
    CHECK(fields[2] == "230.65");
}

TEST_CASE("split_csv keeps commas inside quoted strings") {
    const auto fields = ScpiClient::split_csv("-113,\"Undefined header, extra info\"");
    REQUIRE(fields.size() == 2);
    CHECK(fields[0] == "-113");
    CHECK(fields[1] == "\"Undefined header, extra info\"");
}

TEST_CASE("split_csv of empty line is empty") {
    CHECK(ScpiClient::split_csv("").empty());
    CHECK(ScpiClient::split_csv("   ").empty());
}

TEST_CASE("split_csv keeps empty fields inside a non-empty line") {
    // A trailing separator means the instrument sent an empty field; dropping it
    // would silently change the number of values a caller sees.
    const auto fields = ScpiClient::split_csv("1,,3");
    REQUIRE(fields.size() == 3);
    CHECK(fields[1].empty());

    const auto trailing = ScpiClient::split_csv("1,");
    REQUIRE(trailing.size() == 2);
    CHECK(trailing[1].empty());
}

TEST_CASE("split_semicolons splits a compound query response") {
    // "With several queries in the same command line, the responses are returned
    // in the same order as the queries, separated by semicolons."
    const auto parts = ScpiClient::split_semicolons("1;1.0E+04");
    REQUIRE(parts.size() == 2);
    CHECK(parts[0] == "1");
    CHECK(parts[1] == "1.0E+04");

    const auto quoted = ScpiClient::split_semicolons("\"a;b\";2");
    REQUIRE(quoted.size() == 2);
    CHECK(quoted[0] == "\"a;b\"");
    CHECK(quoted[1] == "2");

    CHECK(ScpiClient::split_semicolons("").empty());
}

TEST_CASE("to_double parses scientific notation") {
    CHECK_THAT(ScpiClient::to_double("+1.23456E+02"),
               Catch::Matchers::WithinRel(123.456, 1e-9));
    CHECK_THAT(ScpiClient::to_double("-3.0"), Catch::Matchers::WithinRel(-3.0, 1e-12));
    CHECK_THAT(ScpiClient::to_double("  3.0e5  "), Catch::Matchers::WithinRel(3.0e5, 1e-12));
    CHECK_THAT(ScpiClient::to_double("0"), Catch::Matchers::WithinAbs(0.0, 1e-18));
}

TEST_CASE("to_double maps the SCPI NaN encoding to NaN") {
    CHECK(std::isnan(ScpiClient::to_double("9.91E+37")));
    CHECK(std::isnan(ScpiClient::to_double("-9.91e37")));
    CHECK(std::isnan(ScpiClient::to_double("+9.910000E+37")));
}

TEST_CASE("to_double keeps values below the NaN encoding as numbers") {
    // The threshold has to sit just under the nominal 9.91e37 so a rounded
    // mantissa still reads as NaN, without swallowing real values.
    CHECK(std::isnan(ScpiClient::to_double("9.905e37")));
    CHECK_FALSE(std::isnan(ScpiClient::to_double("9.8e37")));
    CHECK_FALSE(std::isnan(ScpiClient::to_double("1e30")));
    CHECK(std::isnan(ScpiClient::to_double(std::to_string(kScpiNan))));
}

TEST_CASE("to_double rejects garbage") {
    CHECK_THROWS_AS(ScpiClient::to_double("abc"), ProtocolError);
    CHECK_THROWS_AS(ScpiClient::to_double(""), ProtocolError);
    CHECK_THROWS_AS(ScpiClient::to_double("   "), ProtocolError);
}

TEST_CASE("to_int and to_bool parse the documented response forms") {
    CHECK(ScpiClient::to_int("3") == 3);
    CHECK(ScpiClient::to_int("+3") == 3);
    CHECK(ScpiClient::to_int("-113") == -113);
    CHECK(ScpiClient::to_int(" 42 ") == 42);
    CHECK_THROWS_AS(ScpiClient::to_int("nope"), ProtocolError);

    CHECK(ScpiClient::to_bool("1"));
    CHECK_FALSE(ScpiClient::to_bool("0"));
    CHECK(ScpiClient::to_bool("ON"));
    CHECK(ScpiClient::to_bool("on"));
    CHECK_FALSE(ScpiClient::to_bool("OFF"));
    CHECK_THROWS_AS(ScpiClient::to_bool("maybe"), ProtocolError);
}

TEST_CASE("parse_values parses a DATA? response") {
    const auto values = ScpiClient::parse_values("221.56,1.056,230.65");
    REQUIRE(values.size() == 3);
    CHECK_THAT(values[2], Catch::Matchers::WithinRel(230.65, 1e-9));
}

TEST_CASE("parse_values of an empty response yields no values") {
    // *RST leaves the function list empty, and DATA? then has nothing to report.
    CHECK(ScpiClient::parse_values("").empty());
}

TEST_CASE("parse_error parses SYST:ERR? responses") {
    const auto no_error = ScpiClient::parse_error("0,\"No error\"");
    CHECK(no_error.code == 0);
    CHECK(no_error.message == "No error");

    const auto error = ScpiClient::parse_error("-113,\"Undefined header\"");
    CHECK(error.code == -113);
    CHECK(error.message == "Undefined header");

    // The firmware appends the offending text after a semicolon.
    const auto detailed = ScpiClient::parse_error("-113,\"Undefined header;SYST:NOPE\"");
    CHECK(detailed.code == -113);
    CHECK(detailed.message == "Undefined header;SYST:NOPE");
}

TEST_CASE("parse_error rejects a malformed response") {
    CHECK_THROWS_AS(ScpiClient::parse_error("no code here"), ProtocolError);
    CHECK_THROWS_AS(ScpiClient::parse_error(""), ProtocolError);
}

TEST_CASE("parse_errors reads the pairs of a SYST:ERR:ALL? response") {
    const auto errors =
        ScpiClient::parse_errors("-113,\"Undefined header\",-222,\"Data out of range\"");
    REQUIRE(errors.size() == 2);
    CHECK(errors[0].code == -113);
    CHECK(errors[1].message == "Data out of range");

    CHECK(ScpiClient::parse_errors("0,\"No error\"").empty());
    CHECK_THROWS_AS(ScpiClient::parse_errors("-113"), ProtocolError);
}

TEST_CASE("format_double is compact") {
    CHECK(ScpiClient::format_double(300.0) == "300");
    CHECK(ScpiClient::format_double(1.0) == "1");
    CHECK(ScpiClient::format_double(0.015) == "0.015");
    CHECK(ScpiClient::format_double(-1.5) == "-1.5");
    CHECK(ScpiClient::format_double(0.0) == "0");
}

TEST_CASE("format_double survives the extremes a caller can pass") {
    // The instrument accepts an exponent between -307 and 307; the formatter
    // must not truncate the mantissa into a different number, and must never
    // overrun its buffer.
    CHECK_THAT(ScpiClient::to_double(ScpiClient::format_double(1.0e-300)),
               Catch::Matchers::WithinRel(1.0e-300, 1e-9));
    CHECK_THAT(ScpiClient::to_double(ScpiClient::format_double(123456.789)),
               Catch::Matchers::WithinRel(123456.789, 1e-9));
    CHECK(ScpiClient::format_double(std::numeric_limits<double>::infinity()).size() < 32);
}

TEST_CASE("format_bool uses the keywords the instrument echoes") {
    CHECK(ScpiClient::format_bool(true) == "ON");
    CHECK(ScpiClient::format_bool(false) == "OFF");
}

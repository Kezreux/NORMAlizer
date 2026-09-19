#include <cmath>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <fluke/norma/scpi_client.hpp>

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

TEST_CASE("to_double parses scientific notation") {
    CHECK_THAT(ScpiClient::to_double("+1.23456E+02"),
               Catch::Matchers::WithinRel(123.456, 1e-9));
    CHECK_THAT(ScpiClient::to_double("-3.0"), Catch::Matchers::WithinRel(-3.0, 1e-12));
}

TEST_CASE("to_double maps SCPI NaN (9.91e37) to NaN") {
    CHECK(std::isnan(ScpiClient::to_double("9.91E+37")));
    CHECK(std::isnan(ScpiClient::to_double("-9.91e37")));
}

TEST_CASE("to_double rejects garbage") {
    CHECK_THROWS_AS(ScpiClient::to_double("abc"), ProtocolError);
    CHECK_THROWS_AS(ScpiClient::to_double(""), ProtocolError);
}

TEST_CASE("parse_values parses a DATA? response") {
    const auto values = ScpiClient::parse_values("221.56,1.056,230.65");
    REQUIRE(values.size() == 3);
    CHECK_THAT(values[2], Catch::Matchers::WithinRel(230.65, 1e-9));
}

TEST_CASE("parse_error parses SYST:ERR? responses") {
    const auto no_error = ScpiClient::parse_error("0,\"No error\"");
    CHECK(no_error.code == 0);
    CHECK(no_error.message == "No error");

    const auto error = ScpiClient::parse_error("-113,\"Undefined header\"");
    CHECK(error.code == -113);
    CHECK(error.message == "Undefined header");
}

TEST_CASE("format_double is compact") {
    CHECK(ScpiClient::format_double(300.0) == "300");
    CHECK(ScpiClient::format_double(1.0) == "1");
    CHECK(ScpiClient::format_double(0.015) == "0.015");
}

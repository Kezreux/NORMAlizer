#include <cmath>
#include <memory>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <fluke/norma/instrument.hpp>
#include <fluke/norma/types.hpp>

#include "mock_transport.hpp"

using namespace fluke::norma;
using fluke::norma::test::MockTransport;

namespace {

struct Fixture {
    Fixture() {
        auto transport = std::make_unique<MockTransport>();
        mock = transport.get();
        instrument = std::make_unique<NormaInstrument>(std::move(transport));
    }

    MockTransport* mock = nullptr;
    std::unique_ptr<NormaInstrument> instrument;
};

} // namespace

TEST_CASE("identify parses *IDN?") {
    Fixture f;
    f.mock->responses.push_back("Fluke,NORMA4000,KN34512BA,01.00");

    const auto id = f.instrument->identify();

    CHECK(f.mock->writes.back() == "*IDN?\n");
    CHECK(id.manufacturer == "Fluke");
    CHECK(id.model == "NORMA4000");
    CHECK(id.serial_number == "KN34512BA");
    CHECK(id.firmware_version == "01.00");
}

TEST_CASE("configuration commands are formatted per the manual") {
    Fixture f;

    f.instrument->reset();
    f.instrument->set_wiring_system(WiringSystem::ThreeWattmeter);
    f.instrument->sync_to_voltage(1);
    f.instrument->set_voltage_range(1, 300.0);
    f.instrument->set_current_autorange(1, true);
    f.instrument->set_aperture(1.0);
    f.instrument->set_functions({fn::voltage(1), fn::current(1), fn::active_power(1)});
    f.instrument->set_continuous(true);

    REQUIRE(f.mock->writes.size() == 8);
    CHECK(f.mock->writes[0] == "*RST\n");
    CHECK(f.mock->writes[1] == "ROUT:SYST \"3W\"\n");
    CHECK(f.mock->writes[2] == "SYNC:SOUR VOLT1\n");
    CHECK(f.mock->writes[3] == "VOLT1:RANG 300\n");
    CHECK(f.mock->writes[4] == "CURR1:RANG:AUTO ON\n");
    CHECK(f.mock->writes[5] == "APER 1\n");
    CHECK(f.mock->writes[6] == "FUNC \"VOLT1\",\"CURR1\",\"POW1:ACT\"\n");
    CHECK(f.mock->writes[7] == "INIT:CONT ON\n");
}

TEST_CASE("phase suffix is validated") {
    Fixture f;
    CHECK_THROWS_AS(f.instrument->set_voltage_range(0, 300.0), std::invalid_argument);
    CHECK_THROWS_AS(f.instrument->set_voltage_range(7, 300.0), std::invalid_argument);
    CHECK_THROWS_AS(f.instrument->sync_to_current(0), std::invalid_argument);
}

TEST_CASE("data queries the given functions") {
    Fixture f;
    f.mock->responses.push_back("+2.2156E+02,+1.056E+00,+2.3065E+02");

    const auto values = f.instrument->data({"VOLT1", "CURR1", "POW1:ACT"});

    CHECK(f.mock->writes.back() == "DATA? \"VOLT1\",\"CURR1\",\"POW1:ACT\"\n");
    REQUIRE(values.size() == 3);
    CHECK_THAT(values[0], Catch::Matchers::WithinRel(221.56, 1e-9));
    CHECK_THAT(values[2], Catch::Matchers::WithinRel(230.65, 1e-9));
}

TEST_CASE("data without functions uses the configured list") {
    Fixture f;
    f.mock->responses.push_back("+1.23456E+02");

    const auto values = f.instrument->data();

    CHECK(f.mock->writes.back() == "DATA?\n");
    REQUIRE(values.size() == 1);
}

TEST_CASE("data_with_status splits values and status flags") {
    Fixture f;
    f.mock->responses.push_back("221.56,1.056,230.65,0,1,0");

    const auto reading = f.instrument->data_with_status();

    CHECK(f.mock->writes.back() == "DATA:STAT?\n");
    REQUIRE(reading.values.size() == 3);
    REQUIRE(reading.status.size() == 3);
    CHECK(reading.status[1] == measurement_status::kUnderrange);
}

TEST_CASE("SCPI NaN in measurements becomes NaN") {
    Fixture f;
    f.mock->responses.push_back("9.91E+37,8");

    const auto reading = f.instrument->data_with_status();

    REQUIRE(reading.values.size() == 1);
    CHECK(std::isnan(reading.values[0]));
    CHECK(reading.status[0] == measurement_status::kUndefined);
}

TEST_CASE("wiring_system parses the quoted response") {
    Fixture f;
    f.mock->responses.push_back("\"3W\"");
    CHECK(f.instrument->wiring_system() == WiringSystem::ThreeWattmeter);
}

TEST_CASE("check_errors drains the queue and throws the first error") {
    Fixture f;
    f.mock->responses.push_back("-113,\"Undefined header\"");
    f.mock->responses.push_back("0,\"No error\"");

    CHECK_THROWS_AS(f.instrument->check_errors(), ScpiError);
}

TEST_CASE("check_errors is silent when the queue is empty") {
    Fixture f;
    f.mock->responses.push_back("0,\"No error\"");
    CHECK_NOTHROW(f.instrument->check_errors());
}

TEST_CASE("read_errors returns every queued error") {
    Fixture f;
    f.mock->responses.push_back("-113,\"Undefined header\"");
    f.mock->responses.push_back("-222,\"Data out of range\"");
    f.mock->responses.push_back("0,\"No error\"");

    const auto errors = f.instrument->read_errors();

    REQUIRE(errors.size() == 2);
    CHECK(errors[0].code == -113);
    CHECK(errors[1].message == "Data out of range");
}

TEST_CASE("fn helpers build manual-style function names") {
    CHECK(fn::voltage(1) == "VOLT1");
    CHECK(fn::voltage() == "VOLT");
    CHECK(fn::current(3) == "CURR3");
    CHECK(fn::active_power(1) == "POW1:ACT");
    CHECK(fn::active_power(460) == "POW460:ACT");
    CHECK(fn::power_factor() == "POW:FACT");
    CHECK(fn::frequency() == "FREQ");
}

// NormaInstrument: response parsing, argument validation and the fn:: function
// name grammar. The exact command strings are covered in
// test_command_conformance.cpp; this file is about what the facade does with the
// answers, and about the arguments it refuses to put on the wire at all.

#include <cmath>
#include <memory>
#include <stdexcept>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <fluke/norma/error.hpp>
#include <fluke/norma/instrument.hpp>
#include <fluke/norma/types.hpp>

#include "mock_transport.hpp"

using namespace fluke::norma;
using fluke::norma::test::MockTransport;

namespace {

struct Fixture {
    Fixture() {
        auto transport = std::make_unique<MockTransport>();
        transport->open();
        mock = transport.get();
        instrument = std::make_unique<NormaInstrument>(std::move(transport));
    }

    MockTransport* mock = nullptr;
    std::unique_ptr<NormaInstrument> instrument;
};

} // namespace

// -- Response parsing ----------------------------------------------------------

TEST_CASE("identify parses *IDN?") {
    Fixture f;
    f.mock->responses.push_back("Fluke,NORMA4000,KN34512BA,01.00");

    const auto id = f.instrument->identify();

    CHECK(f.mock->last_command() == "*IDN?");
    CHECK(id.manufacturer == "Fluke");
    CHECK(id.model == "NORMA4000");
    CHECK(id.serial_number == "KN34512BA");
    CHECK(id.firmware_version == "01.00");
}

TEST_CASE("identify tolerates a short *IDN? response") {
    Fixture f;
    f.mock->responses.push_back("Fluke,NORMA5000");

    const auto id = f.instrument->identify();

    CHECK(id.model == "NORMA5000");
    CHECK(id.serial_number.empty());
    CHECK(id.firmware_version.empty());
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

TEST_CASE("data queries the given functions") {
    Fixture f;
    f.mock->responses.push_back("+2.2156E+02,+1.056E+00,+2.3065E+02");

    const auto values = f.instrument->data({"VOLT1", "CURR1", "POW1:ACT"});

    CHECK(f.mock->last_command() == "DATA? \"VOLT1\",\"CURR1\",\"POW1:ACT\"");
    REQUIRE(values.size() == 3);
    CHECK_THAT(values[0], Catch::Matchers::WithinRel(221.56, 1e-9));
    CHECK_THAT(values[2], Catch::Matchers::WithinRel(230.65, 1e-9));
}

TEST_CASE("data without functions uses the configured list") {
    Fixture f;
    f.mock->responses.push_back("+1.23456E+02");

    const auto values = f.instrument->data();

    CHECK(f.mock->last_command() == "DATA?");
    REQUIRE(values.size() == 1);
}

TEST_CASE("data of an empty function list returns no values") {
    // *RST leaves the function list empty, and the instrument then has no values
    // to report rather than an error to raise.
    Fixture f;
    f.mock->responses.push_back("");
    CHECK(f.instrument->data().empty());
}

TEST_CASE("data_with_status splits values and status flags") {
    Fixture f;
    f.mock->responses.push_back("221.56,1.056,230.65,0,1,0");

    const auto reading = f.instrument->data_with_status();

    CHECK(f.mock->last_command() == "DATA:STAT?");
    REQUIRE(reading.values.size() == 3);
    REQUIRE(reading.status.size() == 3);
    CHECK_THAT(reading.values[0], Catch::Matchers::WithinRel(221.56, 1e-9));
    CHECK(reading.status[1] == measurement_status::kUnderrange);
    CHECK(reading.is_valid(0));
    CHECK_FALSE(reading.is_valid(1));
    CHECK_FALSE(reading.is_valid(99)); // out of range index is not "valid"
}

TEST_CASE("data_with_status rejects an odd field count") {
    // Half values, half status flags — an odd count means the response was
    // truncated, and silently dropping a field would misalign every value.
    Fixture f;
    f.mock->responses.push_back("221.56,1.056,0");
    CHECK_THROWS_AS(f.instrument->data_with_status(), ProtocolError);
}

TEST_CASE("SCPI NaN in measurements becomes NaN") {
    Fixture f;
    f.mock->responses.push_back("9.91E+37,8");

    const auto reading = f.instrument->data_with_status();

    REQUIRE(reading.values.size() == 1);
    CHECK(std::isnan(reading.values[0]));
    CHECK(reading.status[0] == measurement_status::kUndefined);
    CHECK_FALSE(reading.is_valid(0));
}

TEST_CASE("wiring_system parses the quoted response") {
    Fixture f;
    f.mock->responses.push_back("\"3W\"");
    CHECK(f.instrument->wiring_system() == WiringSystem::ThreeWattmeter);

    f.mock->responses.push_back("2W"); // unquoted is accepted too
    CHECK(f.instrument->wiring_system() == WiringSystem::TwoWattmeter);
}

TEST_CASE("wiring_system reports an unexpected response") {
    Fixture f;
    f.mock->responses.push_back("\"4W\"");
    CHECK_THROWS_AS(f.instrument->wiring_system(), ProtocolError);
}

TEST_CASE("character-data responses are parsed in short and long form") {
    Fixture f;
    f.mock->responses = {"EXT", "EXTernal", "AC", "POS", "NEGative", "PCT", "REM"};
    CHECK(f.instrument->input_shunt(1) == Shunt::External);
    CHECK(f.instrument->input_shunt(1) == Shunt::External);
    CHECK(f.instrument->input_coupling(1) == Coupling::AC);
    CHECK(f.instrument->sync_slope() == Slope::Positive);
    CHECK(f.instrument->sync_slope() == Slope::Negative);
    CHECK(f.instrument->sync_level_unit() == LevelUnit::Percent);
    CHECK(f.instrument->key_lock() == KeyLock::Remote);
}

TEST_CASE("an unknown character-data response is a protocol error") {
    Fixture f;
    f.mock->responses.push_back("SIDEWAYS");
    CHECK_THROWS_AS(f.instrument->sync_slope(), ProtocolError);
}

TEST_CASE("data_format parses the type and its length") {
    Fixture f;
    f.mock->responses = {"ASC,6", "REAL,64", "INT"};

    auto setting = f.instrument->data_format();
    CHECK(setting.format == DataFormat::Ascii);
    CHECK(setting.length == 6);

    setting = f.instrument->data_format();
    CHECK(setting.format == DataFormat::Real);
    CHECK(setting.length == 64);

    setting = f.instrument->data_format();
    CHECK(setting.format == DataFormat::Integer);
    CHECK(setting.length == 0); // the instrument reported no length
}

TEST_CASE("date and time responses are parsed field by field") {
    Fixture f;
    f.mock->responses = {"2026,9,26", "12,30,5"};

    const Date date = f.instrument->date();
    CHECK(date.year == 2026);
    CHECK(date.month == 9);
    CHECK(date.day == 26);

    const Time time = f.instrument->time_of_day();
    CHECK(time.hours == 12);
    CHECK(time.minutes == 30);
    CHECK(time.seconds == 5);
}

TEST_CASE("a truncated date response is reported, not silently zero-filled") {
    Fixture f;
    f.mock->responses.push_back("2026,9");
    CHECK_THROWS_AS(f.instrument->date(), ProtocolError);
}

TEST_CASE("wait_operation_complete insists on the documented answer") {
    Fixture f;
    f.mock->responses.push_back("1");
    CHECK_NOTHROW(f.instrument->wait_operation_complete());

    f.mock->responses.push_back("0");
    CHECK_THROWS_AS(f.instrument->wait_operation_complete(), ProtocolError);
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

TEST_CASE("read_errors_at_once parses the whole queue from one response") {
    Fixture f;
    f.mock->responses.push_back("-113,\"Undefined header\",-222,\"Data out of range\"");

    const auto errors = f.instrument->read_errors_at_once();

    CHECK(f.mock->writes.size() == 1); // one round-trip, unlike read_errors()
    REQUIRE(errors.size() == 2);
    CHECK(errors[1].code == -222);
}

// -- Argument validation -------------------------------------------------------
// Every check here keeps a request off the wire that the instrument would answer
// with a SCPI error, so the mistake surfaces at the call site instead of in the
// error queue.

TEST_CASE("phase suffix is validated") {
    Fixture f;
    CHECK_THROWS_AS(f.instrument->set_voltage_range(0, 300.0), std::invalid_argument);
    CHECK_THROWS_AS(f.instrument->set_voltage_range(7, 300.0), std::invalid_argument);
    CHECK_THROWS_AS(f.instrument->sync_to_current(0), std::invalid_argument);
    CHECK_THROWS_AS(f.instrument->current_ranges(-1), std::invalid_argument);
    CHECK(f.mock->writes.empty());
}

TEST_CASE("input channel is validated against the 12-channel maximum") {
    Fixture f;
    CHECK_THROWS_AS(f.instrument->set_input_coupling(0, Coupling::AC), std::invalid_argument);
    CHECK_THROWS_AS(f.instrument->set_input_coupling(13, Coupling::AC), std::invalid_argument);
    CHECK_NOTHROW(f.instrument->set_input_coupling(12, Coupling::AC));
}

TEST_CASE("aperture is validated against the documented range") {
    Fixture f;
    CHECK_THROWS_AS(f.instrument->set_aperture(0.0), std::invalid_argument);
    CHECK_THROWS_AS(f.instrument->set_aperture(0.014), std::invalid_argument);
    CHECK_THROWS_AS(f.instrument->set_aperture(3601.0), std::invalid_argument);
    CHECK_NOTHROW(f.instrument->set_aperture(0.015));
    CHECK_NOTHROW(f.instrument->set_aperture(3600.0));
}

TEST_CASE("an empty function list is refused") {
    Fixture f;
    CHECK_THROWS_AS(f.instrument->set_functions({}), std::invalid_argument);
    CHECK_THROWS_AS(f.instrument->set_display_functions({}), std::invalid_argument);
    CHECK_THROWS_AS(f.instrument->set_transform_functions({}), std::invalid_argument);
    CHECK_THROWS_AS(f.instrument->set_integral_functions({}), std::invalid_argument);
    CHECK_THROWS_AS(f.instrument->set_sweep_functions(SweepBlock::Block1, {}),
                    std::invalid_argument);
    CHECK(f.mock->writes.empty());
}

TEST_CASE("setup slots are validated") {
    Fixture f;
    CHECK_THROWS_AS(f.instrument->save_setup(1), std::invalid_argument);  // read-only slot
    CHECK_THROWS_AS(f.instrument->save_setup(25), std::invalid_argument);
    CHECK_NOTHROW(f.instrument->save_setup(24));
    CHECK_THROWS_AS(f.instrument->recall_setup(3), std::invalid_argument);
    CHECK_NOTHROW(f.instrument->recall_setup(2));
}

TEST_CASE("transform cycles accept only the documented values") {
    Fixture f;
    CHECK_THROWS_AS(f.instrument->set_transform_cycles(5), std::invalid_argument);
    CHECK_NOTHROW(f.instrument->set_transform_cycles(12));
}

TEST_CASE("sweep count and sparsing are validated") {
    Fixture f;
    CHECK_THROWS_AS(f.instrument->set_sweep_count(SweepBlock::Block1, 0), std::invalid_argument);
    CHECK_THROWS_AS(f.instrument->set_sweep_sparsing(SweepBlock::Block1, 0), std::invalid_argument);
    CHECK_THROWS_AS(f.instrument->set_sweep_sparsing(SweepBlock::Block1, 65536),
                    std::invalid_argument);
}

TEST_CASE("the read-only parts of a status register cannot be written") {
    Fixture f;
    CHECK_THROWS_AS(f.instrument->set_status(StatusRegister::Operation, RegisterPart::Condition, 1),
                    std::invalid_argument);
    CHECK_THROWS_AS(f.instrument->set_status(StatusRegister::Operation, RegisterPart::Event, 1),
                    std::invalid_argument);
    CHECK_THROWS_AS(
        f.instrument->set_status(StatusRegister::Operation, RegisterPart::Enable, 70000),
        std::invalid_argument);
    CHECK(f.mock->writes.empty());
}

TEST_CASE("the efficiency reference is only defined for the aggregate suffixes") {
    Fixture f;
    CHECK_THROWS_AS(f.instrument->set_efficiency_reference("POW1:ACT", "POW2:ACT", 1),
                    std::invalid_argument);
    CHECK_NOTHROW(f.instrument->set_efficiency_reference("POW1:ACT", "POW2:ACT", 0));
    CHECK_NOTHROW(f.instrument->set_efficiency_reference("POW1:ACT", "POW2:ACT", 460));
}

TEST_CASE("a raw command carrying a second line is refused") {
    // The desktop app's SCPI console hands arbitrary text to write(); a smuggled
    // second command would leave an unread response behind and put every later
    // query one answer out of step.
    Fixture f;
    CHECK_THROWS_AS(f.instrument->write("*CLS\nDATA?"), std::invalid_argument);
    CHECK(f.mock->writes.empty());
}

// -- fn:: function names -------------------------------------------------------

TEST_CASE("fn helpers build manual-style function names") {
    CHECK(fn::voltage(1) == "VOLT1");
    CHECK(fn::voltage() == "VOLT");
    CHECK(fn::voltage(460) == "VOLT460");
    CHECK(fn::current(3) == "CURR3");
    CHECK(fn::voltage_ac(2) == "VOLT2:AC");
    CHECK(fn::voltage_mean() == "VOLT:MEAN");
    CHECK(fn::active_power(1) == "POW1:ACT");
    CHECK(fn::active_power(460) == "POW460:ACT");
    CHECK(fn::apparent_power(2) == "POW2:APP");
    CHECK(fn::power_factor() == "POW:FACT");
    CHECK(fn::corrected_power(1) == "POW1:CORR");
    CHECK(fn::phase_angle(1) == "PHAS1");
    CHECK(fn::frequency() == "FREQ");
    CHECK(fn::time_interval() == "TIME");
    CHECK(fn::time_relative() == "TIME:REL");
}

// POWer:REACtive shortens to REAC while the REACTance quantity shortens to
// REACT. Sending one where the other belongs is accepted by neither firmware
// nor reviewer, so both spellings are pinned here.
TEST_CASE("reactive power and reactance keep their distinct short forms") {
    CHECK(fn::reactive_power(1) == "POW1:REAC");
    CHECK(fn::series_reactance(1) == "REACT1:SER");
    CHECK(fn::parallel_reactance() == "REACT:PAR");
    CHECK(fn::series_resistance(1) == "RES1:SER");
    CHECK(fn::parallel_resistance(1) == "RES1:PAR");
    CHECK(fn::apparent_impedance(1) == "IMP1:APP");
}

TEST_CASE("fn helpers cover the per-channel quantities") {
    CHECK(fn::voltage_rectified_mean(1) == "VOLT1:RMEAN");
    CHECK(fn::voltage_rectified_mean_corrected(1) == "VOLT1:RMCORR");
    CHECK(fn::voltage_peak_to_peak(1) == "VOLT1:PTP");
    CHECK(fn::voltage_peak_high(1) == "VOLT1:PHIGH");
    CHECK(fn::voltage_peak_low(1) == "VOLT1:PLOW");
    CHECK(fn::voltage_crest_factor(1) == "VOLT1:CFAC");
    CHECK(fn::voltage_form_factor(1) == "VOLT1:FFAC");
    CHECK(fn::voltage_thd(1) == "VOLT1:THD");
    CHECK(fn::voltage_harmonic_content(1) == "VOLT1:HCONT");
    CHECK(fn::voltage_fundamental_content(1) == "VOLT1:FCONT");
    CHECK(fn::voltage_phase(1) == "VOLT1:PHAS");
    CHECK(fn::current_thd(3) == "CURR3:THD");
    CHECK(fn::current_peak_to_peak(2) == "CURR2:PTP");
}

TEST_CASE("fn helpers build phase-to-phase names") {
    CHECK(fn::voltage_line(12) == "VOLT12");
    CHECK(fn::voltage_line(31) == "VOLT31");
    CHECK(fn::voltage_line(123) == "VOLT123");
    CHECK(fn::voltage_line_rectified_mean(23) == "VOLT23:RMEAN");
    CHECK(fn::voltage_line_mean(456) == "VOLT456:MEAN");
    CHECK(fn::voltage_line_thd(12) == "VOLT12:THD");
}

TEST_CASE("fn modifiers compose onto any function name") {
    CHECK(fn::harmonic(fn::active_power(1)) == "POW1:ACT:HAR");
    CHECK(fn::minimum(fn::voltage(1)) == "VOLT1:MIN");
    CHECK(fn::maximum(fn::current()) == "CURR:MAX");
    CHECK(fn::integral(fn::active_power()) == "POW:ACT:INT");
    CHECK(fn::integral_positive(fn::active_power(1)) == "POW1:ACT:IPOS");
    CHECK(fn::integral_negative(fn::active_power(1)) == "POW1:ACT:INEG");
    CHECK(fn::minimum(fn::harmonic(fn::voltage(1))) == "VOLT1:HAR:MIN");
}

// A typo in a suffix would otherwise travel to the instrument and come back as
// SCPI error -113, far away from the code that produced it.
TEST_CASE("fn helpers reject suffixes the manual does not define") {
    CHECK_THROWS_AS(fn::voltage(7), std::invalid_argument);
    CHECK_THROWS_AS(fn::voltage(99), std::invalid_argument);
    CHECK_THROWS_AS(fn::current(-1), std::invalid_argument);
    // Phase-to-phase suffixes belong to the phase-to-phase helpers only.
    CHECK_THROWS_AS(fn::voltage(12), std::invalid_argument);
    CHECK_THROWS_AS(fn::voltage_line(1), std::invalid_argument);
    // Peak quantities exist per channel, not as an aggregate.
    CHECK_THROWS_AS(fn::voltage_peak_to_peak(0), std::invalid_argument);
    CHECK_THROWS_AS(fn::voltage_peak_to_peak(460), std::invalid_argument);
    // Efficiency is only defined for the aggregate forms.
    CHECK_THROWS_AS(fn::efficiency(1), std::invalid_argument);
    CHECK(fn::efficiency(460) == "POW460:EFF");
}

TEST_CASE("fn suffix predicates match the manual's table") {
    CHECK(fn::is_phase(1));
    CHECK(fn::is_phase(6));
    CHECK_FALSE(fn::is_phase(0));
    CHECK_FALSE(fn::is_phase(7));
    CHECK(fn::is_phase_to_phase(12));
    CHECK(fn::is_phase_to_phase(64));
    CHECK_FALSE(fn::is_phase_to_phase(123));
    CHECK(fn::is_average_phase_to_phase(123));
    CHECK(fn::is_average_phase_to_phase(456));
    CHECK(fn::is_aggregate(0));
    CHECK(fn::is_aggregate(460));
    CHECK_FALSE(fn::is_valid_suffix(7));
    CHECK(fn::is_valid_suffix(460));
}

TEST_CASE("timeout is readable and settable") {
    Fixture f;
    f.instrument->set_timeout(std::chrono::milliseconds(2500));
    CHECK(f.instrument->timeout() == std::chrono::milliseconds(2500));
    CHECK(f.instrument->scpi().default_timeout() == std::chrono::milliseconds(2500));
}

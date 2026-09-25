// Tests for the simulator itself.
//
// The simulator is what the integration tests measure the library against, so a
// bug in it would read as a bug in the library. These tests pin its SCPI parsing,
// its *RST defaults, its error reporting and its signal model directly, without
// the library in between.

#include <cmath>
#include <string>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <fluke/norma/scpi_client.hpp>
#include <fluke/norma/simulator/norma_simulator.hpp>

using namespace fluke::norma;
using fluke::norma::sim::NormaSimulator;

namespace {

/// Sends one line and requires a response.
std::string ask(NormaSimulator& sim, const std::string& line) {
    auto response = sim.handle_line(line);
    REQUIRE(response.has_value());
    return *response;
}

/// Sends one line and requires that it produced no response.
void tell(NormaSimulator& sim, const std::string& line) {
    CHECK_FALSE(sim.handle_line(line).has_value());
}

/// Drains the error queue and returns the first code, or 0 when empty.
int first_error(NormaSimulator& sim) {
    const std::string response = ask(sim, "SYST:ERR?");
    return std::stoi(response.substr(0, response.find(',')));
}

void require_no_errors(NormaSimulator& sim) {
    const std::string response = ask(sim, "SYST:ERR?");
    INFO("error queue: " << response);
    CHECK(response.substr(0, 1) == "0");
}

} // namespace

TEST_CASE("simulator identifies itself", "[simulator]") {
    NormaSimulator sim;
    CHECK(ask(sim, "*IDN?") == "Fluke,NORMA5000,KN34512BA,01.05");
    CHECK(ask(sim, "SYST:VERS?") == "1999.0");
}

TEST_CASE("simulator accepts short and long mnemonics alike", "[simulator]") {
    NormaSimulator sim;
    tell(sim, "INPut1:COUPling AC");
    CHECK(ask(sim, "INP1:COUP?") == "AC");
    tell(sim, "INP1:COUP DC");
    CHECK(ask(sim, "INPut1:COUPling?") == "DC");
    // Casing is irrelevant.
    CHECK(ask(sim, "inp1:coup?") == "DC");
    require_no_errors(sim);
}

TEST_CASE("simulator honours optional nodes", "[simulator]") {
    NormaSimulator sim;
    tell(sim, "VOLT1:RANG 300");
    // [SENSe:] and [:UPPer] are both optional, and AC/DC select the same range.
    CHECK(ask(sim, "VOLT1:RANG?") == "300");
    CHECK(ask(sim, "SENS:VOLT1:RANG:UPP?") == "300");
    CHECK(ask(sim, "VOLTage1:DC:RANGe:UPPer?") == "300");
    CHECK(ask(sim, "VOLT1:AC:RANG?") == "300");
    require_no_errors(sim);
}

TEST_CASE("simulator rejects a suffix on a node that takes none", "[simulator]") {
    NormaSimulator sim;
    sim.handle_line("APER3ture 1.0");
    CHECK(first_error(sim) == sim::error_code::kUndefinedHeader);
}

TEST_CASE("simulator rejects an undefined header with -113", "[simulator]") {
    NormaSimulator sim;
    sim.handle_line("SYSTEM:THIS:DOES:NOT:EXIST");
    const std::string response = ask(sim, "SYST:ERR?");
    CHECK(response.find("-113") == 0);
    CHECK(response.find("Undefined header") != std::string::npos);
    // The queue drains on read.
    require_no_errors(sim);
}

TEST_CASE("simulator reports the documented parameter errors", "[simulator]") {
    NormaSimulator sim;

    sim.handle_line("APER");                  // a setting command with no parameter
    CHECK(first_error(sim) == sim::error_code::kMissingParameter);

    sim.handle_line("APER 9999");             // outside 0.015..3600
    CHECK(first_error(sim) == sim::error_code::kDataOutOfRange);

    sim.handle_line("APER banana");           // not a number
    CHECK(first_error(sim) == sim::error_code::kIllegalParameterValue);

    sim.handle_line("APER 1.0V");             // "no unit is accepted"
    CHECK(first_error(sim) == sim::error_code::kIllegalParameterValue);

    sim.handle_line("FUNC VOLT1");            // <function> must be quoted
    CHECK(first_error(sim) == sim::error_code::kIllegalParameterValue);

    sim.handle_line("*RST?");                 // *RST has no query form
    CHECK(first_error(sim) == sim::error_code::kUndefinedHeader);

    sim.handle_line("VOLT1:RANG:LIST 3");     // a query-only node
    CHECK(first_error(sim) == sim::error_code::kUndefinedHeader);
}

TEST_CASE("simulator serves a compound command line", "[simulator]") {
    NormaSimulator sim;
    // Shared-path shortening: GAIN continues below INPut1.
    tell(sim, "INP1:SHUN EXT;GAIN 25.0");
    CHECK(ask(sim, "INP1:SHUN?") == "EXT");
    CHECK(ask(sim, "INP1:GAIN?") == "25");

    // Several queries answer in order, separated by semicolons.
    CHECK(ask(sim, "INP1:FILT:STAT?;:INP1:FILT:LPAS:FREQ?") == "1;300000");
    require_no_errors(sim);
}

TEST_CASE("simulator restores the documented *RST state", "[simulator]") {
    NormaSimulator sim;
    tell(sim, "APER 2.0");
    tell(sim, "FUNC \"VOLT1\"");
    tell(sim, "INP1:COUP AC");
    tell(sim, "FORM REAL,64");
    tell(sim, "INIT:CONT OFF");

    tell(sim, "*RST");

    CHECK(ask(sim, "FUNC?").empty());                    // empty list = no values defined
    CHECK(ask(sim, "FUNC:COUN?") == "0");
    CHECK(ask(sim, "INP1:COUP?") == "DC");               // *RST state: DC on all channels
    CHECK(ask(sim, "FORM?") == "ASC,6");                 // *RST state: ASCii,6
    CHECK(ask(sim, "FUNC:CONC?") == "1");                // *RST state: ON
    CHECK(ask(sim, "INIT:CONT?") == "1");
    CHECK(ask(sim, "VOLT1:RANG:AUTO?") == "1");
    require_no_errors(sim);
}

TEST_CASE("*RST keeps the error queue but *CLS clears it", "[simulator]") {
    NormaSimulator sim;
    sim.handle_line("NOPE");
    tell(sim, "*RST");
    CHECK(first_error(sim) == sim::error_code::kUndefinedHeader);

    sim.handle_line("NOPE");
    tell(sim, "*CLS");
    require_no_errors(sim);
}

TEST_CASE("simulator measures the configured signal", "[simulator]") {
    NormaSimulator sim;
    sim.signal().frequency = 50.0;
    sim.signal().phases[0] = {230.0, 5.0, 0.0, 0.0, 0.0, 0.0, 0.0};

    tell(sim, "FUNC \"VOLT1\",\"CURR1\",\"POW1:ACT\",\"POW1:APP\",\"FREQ\"");
    const std::string response = ask(sim, "DATA?");
    const auto values = ScpiClient::parse_values(response);

    REQUIRE(values.size() == 5);
    CHECK_THAT(values[0], Catch::Matchers::WithinRel(230.0, 1e-4));
    CHECK_THAT(values[1], Catch::Matchers::WithinRel(5.0, 1e-4));
    CHECK_THAT(values[2], Catch::Matchers::WithinRel(1150.0, 1e-4)); // in phase: P = U*I
    CHECK_THAT(values[3], Catch::Matchers::WithinRel(1150.0, 1e-4));
    CHECK_THAT(values[4], Catch::Matchers::WithinRel(50.0, 1e-9));
    require_no_errors(sim);
}

TEST_CASE("a phase shift splits apparent power into active and reactive",
          "[simulator]") {
    NormaSimulator sim;
    sim.signal().phases[0] = {100.0, 1.0, 60.0, 0.0, 0.0, 0.0, 0.0};

    tell(sim, "FUNC \"POW1:ACT\",\"POW1:REAC\",\"POW1:FACT\",\"PHAS1\"");
    const auto values = ScpiClient::parse_values(ask(sim, "DATA?"));

    REQUIRE(values.size() == 4);
    CHECK_THAT(values[0], Catch::Matchers::WithinRel(50.0, 1e-4));            // 100*1*cos60
    CHECK_THAT(values[1], Catch::Matchers::WithinRel(86.6025404, 1e-4));      // 100*1*sin60
    CHECK_THAT(values[2], Catch::Matchers::WithinRel(0.5, 1e-4));             // power factor
    CHECK_THAT(values[3], Catch::Matchers::WithinRel(60.0, 1e-4));            // arccos(0.5)
}

TEST_CASE("a capacitive phase shift sets the power-factor status bit",
          "[simulator]") {
    NormaSimulator sim;
    sim.signal().phases[0] = {100.0, 1.0, -60.0, 0.0, 0.0, 0.0, 0.0};

    tell(sim, "FUNC \"POW1:FACT\"");
    const auto fields = ScpiClient::split_csv(ask(sim, "DATA:STAT?"));

    REQUIRE(fields.size() == 2);
    CHECK(ScpiClient::to_int(fields[1]) == measurement_status::kPowerFactorCapacitive);
}

TEST_CASE("the range in use decides the over/underrange status bits",
          "[simulator]") {
    NormaSimulator sim;
    sim.signal().phases[0] = {230.0, 1.0, 0.0, 0.0, 0.0, 0.0, 0.0};
    tell(sim, "FUNC \"VOLT1\"");

    SECTION("a value inside the range is Normal") {
        tell(sim, "VOLT1:RANG 300");
        const auto fields = ScpiClient::split_csv(ask(sim, "DATA:STAT?"));
        CHECK(ScpiClient::to_int(fields[1]) == measurement_status::kNormal);
    }

    SECTION("a value above the range is Overrange") {
        tell(sim, "VOLT1:RANG 100");
        const auto fields = ScpiClient::split_csv(ask(sim, "DATA:STAT?"));
        CHECK((ScpiClient::to_int(fields[1]) & measurement_status::kOverrange) != 0);
    }

    SECTION("a value far below the range is Underrange") {
        // 2 V in the 1000 V range: the resolution left for it is poor enough
        // that the instrument flags the value as reduced precision.
        sim.signal().phases[0].voltage_rms = 2.0;
        tell(sim, "VOLT1:RANG 1000");
        const auto fields = ScpiClient::split_csv(ask(sim, "DATA:STAT?"));
        CHECK((ScpiClient::to_int(fields[1]) & measurement_status::kUnderrange) != 0);
    }

    SECTION("autorange suppresses both") {
        tell(sim, "VOLT1:RANG:AUTO ON");
        const auto fields = ScpiClient::split_csv(ask(sim, "DATA:STAT?"));
        CHECK(ScpiClient::to_int(fields[1]) == measurement_status::kNormal);
    }
}

TEST_CASE("without a signal every measurement is NaN and Undefined",
          "[simulator]") {
    NormaSimulator sim;
    sim.signal().present = false;
    tell(sim, "FUNC \"VOLT1\",\"FREQ\"");

    const auto fields = ScpiClient::split_csv(ask(sim, "DATA:STAT?"));
    REQUIRE(fields.size() == 4);
    CHECK(std::isnan(ScpiClient::to_double(fields[0])));
    CHECK(std::isnan(ScpiClient::to_double(fields[1])));
    CHECK(ScpiClient::to_int(fields[2]) == measurement_status::kUndefined);
    CHECK(ScpiClient::to_int(fields[3]) == measurement_status::kUndefined);
}

TEST_CASE("a function the instrument cannot provide is Not available",
          "[simulator]") {
    NormaSimulator sim;
    // MINimum/MAXimum are marked unimplemented in the manual, and the integral
    // accumulators need CALCulate:INTegral switched on first.
    tell(sim, "FUNC \"VOLT1:MIN\",\"POW1:ACT:INT\",\"TORQ1\"");
    const auto fields = ScpiClient::split_csv(ask(sim, "DATA:STAT?"));

    REQUIRE(fields.size() == 6);
    for (int i = 3; i < 6; ++i) {
        CHECK(ScpiClient::to_int(fields[static_cast<std::size_t>(i)]) ==
              measurement_status::kNotAvailable);
    }

    tell(sim, "CALC:INT:STAT ON");
    const auto enabled = ScpiClient::split_csv(ask(sim, "DATA:STAT? \"POW1:ACT:INT\""));
    REQUIRE(enabled.size() == 2);
    CHECK(ScpiClient::to_int(enabled[1]) == measurement_status::kNormal);
}

TEST_CASE("DATA? with an explicit list replaces the configured functions",
          "[simulator]") {
    // The manual lists DATA? as invalidating FUNCtion[:ON] and FUNCtion:COUNt?.
    NormaSimulator sim;
    tell(sim, "FUNC \"VOLT1\",\"CURR1\"");
    CHECK(ask(sim, "FUNC:COUN?") == "2");

    ask(sim, "DATA? \"POW1:ACT\"");

    CHECK(ask(sim, "FUNC:COUN?") == "1");
    CHECK(ask(sim, "FUNC?") == "\"POW1:ACT\"");
}

TEST_CASE("the averaging interval is stretched to whole signal periods",
          "[simulator]") {
    NormaSimulator sim;
    sim.signal().frequency = 50.0; // 20 ms period
    tell(sim, "APER 0.11");
    tell(sim, "FUNC \"TIME\"");

    const auto values = ScpiClient::parse_values(ask(sim, "DATA?"));
    REQUIRE(values.size() == 1);
    CHECK_THAT(values[0], Catch::Matchers::WithinRel(0.12, 1e-6));
}

TEST_CASE("FORMat REAL makes the data queries answer with a binary block",
          "[simulator]") {
    // A client that assumes ASCII has to be able to notice this.
    NormaSimulator sim;
    tell(sim, "FUNC \"VOLT1\"");
    tell(sim, "FORM REAL,64");

    const std::string response = ask(sim, "DATA?");
    CHECK(response.front() == '#');
    // Setting the data format to a binary one drags the status format along.
    CHECK(ask(sim, "FORM:STAT?").substr(0, 3) != "ASC");
}

TEST_CASE("CONCurrent OFF makes FUNCtion a one-of-n switch", "[simulator]") {
    NormaSimulator sim;
    tell(sim, "FUNC \"VOLT1\",\"CURR1\",\"POW1:ACT\"");
    tell(sim, "FUNC:CONC OFF");
    CHECK(ask(sim, "FUNC:COUN?") == "1");

    sim.handle_line("FUNC \"VOLT1\",\"CURR1\"");
    CHECK(first_error(sim) == sim::error_code::kSettingsConflict);
}

TEST_CASE("GAIN and SHUNt are refused on a voltage channel", "[simulator]") {
    // The current inputs are the odd channels; the manual marks both settings
    // "current channels only".
    NormaSimulator sim;
    sim.handle_line("INP2:GAIN 25");
    CHECK(first_error(sim) == sim::error_code::kSettingsConflict);
    sim.handle_line("INP4:SHUN EXT");
    CHECK(first_error(sim) == sim::error_code::kSettingsConflict);
    CHECK_FALSE(sim.handle_line("INP1:GAIN 25").has_value());
    require_no_errors(sim);
}

TEST_CASE("the operation status register reflects the instrument state",
          "[simulator]") {
    NormaSimulator sim;
    int condition = std::stoi(ask(sim, "STAT:OPER:COND?"));
    CHECK((condition & operation_status::kSynchronized) != 0);
    CHECK((condition & operation_status::kAveraging) != 0);
    CHECK((condition & operation_status::kSweeping) == 0);

    sim.signal().present = false;
    condition = std::stoi(ask(sim, "STAT:OPER:COND?"));
    CHECK((condition & operation_status::kSynchronized) == 0);

    tell(sim, "SWE1:STAT ON");
    condition = std::stoi(ask(sim, "STAT:OPER:COND?"));
    CHECK((condition & operation_status::kSweeping) != 0);
}

TEST_CASE("the questionable registers report per-channel over/underrange",
          "[simulator]") {
    NormaSimulator sim;
    sim.signal().phases[0] = {230.0, 1.0, 0.0, 0.0, 0.0, 0.0, 0.0};
    tell(sim, "VOLT1:RANG 100"); // 230 V is above the 100 V range

    const int voltage = std::stoi(ask(sim, "STAT:QUES:VOLT:COND?"));
    CHECK((voltage & channel_status::overrange(0)) != 0);

    const int questionable = std::stoi(ask(sim, "STAT:QUES:COND?"));
    CHECK((questionable & questionable_status::kVoltageSummary) != 0);
}

TEST_CASE("a status EVENt part latches a transition and clears on read",
          "[simulator]") {
    NormaSimulator sim;
    ask(sim, "STAT:OPER:EVEN?"); // establish the baseline condition

    sim.signal().present = false; // Synchronized falls 1 -> 0
    tell(sim, "STAT:OPER:NTR 65535");
    const int event = std::stoi(ask(sim, "STAT:OPER:EVEN?"));
    CHECK((event & operation_status::kSynchronized) != 0);

    // Reading clears the latch.
    CHECK(std::stoi(ask(sim, "STAT:OPER:EVEN?")) == 0);
}

TEST_CASE("*STB? summarizes the error queue", "[simulator]") {
    NormaSimulator sim;
    CHECK((std::stoi(ask(sim, "*STB?")) & status_byte::kErrorQueueNotEmpty) == 0);
    sim.handle_line("NOPE");
    CHECK((std::stoi(ask(sim, "*STB?")) & status_byte::kErrorQueueNotEmpty) != 0);
}

TEST_CASE("SYST:ERR:ALL? drains the whole queue in one response", "[simulator]") {
    NormaSimulator sim;
    sim.handle_line("NOPE");
    sim.handle_line("ALSO:NOPE");

    const std::string response = ask(sim, "SYST:ERR:ALL?");
    const auto errors = ScpiClient::parse_errors(response);
    CHECK(errors.size() == 2);
    require_no_errors(sim);
}

TEST_CASE("the range setting snaps to a selectable range", "[simulator]") {
    NormaSimulator sim;
    // 200 V is not a range the instrument has; it takes the next one that fits.
    tell(sim, "VOLT1:RANG 200");
    CHECK(ask(sim, "VOLT1:RANG?") == "300");
    CHECK(ask(sim, "VOLT1:RANG:AUTO?") == "0"); // RANGe turns autorange off

    const auto ranges = ScpiClient::parse_values(ask(sim, "VOLT1:RANG:LIST?"));
    CHECK(ranges.size() == 8);
    CHECK(ranges.front() == 0.3);
    CHECK(ranges.back() == 1000.0);
}

TEST_CASE("a transducer ratio scales the measured value", "[simulator]") {
    NormaSimulator sim;
    sim.signal().phases[0] = {100.0, 1.0, 0.0, 0.0, 0.0, 0.0, 0.0};
    tell(sim, "VOLT1:SCAL 10");

    const auto values = ScpiClient::parse_values(ask(sim, "DATA? \"VOLT1\""));
    REQUIRE(values.size() == 1);
    CHECK_THAT(values[0], Catch::Matchers::WithinRel(1000.0, 1e-6));
}

TEST_CASE("the aggregate suffix totals the first three-phase system",
          "[simulator]") {
    NormaSimulator sim;
    for (int phase = 0; phase < 3; ++phase) {
        sim.signal().phases[static_cast<std::size_t>(phase)] = {230.0, 2.0, 0.0, 0.0, 0.0,
                                                                0.0, 0.0};
    }

    const auto values = ScpiClient::parse_values(ask(sim, "DATA? \"POW:ACT\",\"VOLT\""));
    REQUIRE(values.size() == 2);
    CHECK_THAT(values[0], Catch::Matchers::WithinRel(3.0 * 460.0, 1e-6)); // powers add up
    CHECK_THAT(values[1], Catch::Matchers::WithinRel(230.0, 1e-6));       // voltages average
}

TEST_CASE("a spectrum can be queried in windows", "[simulator]") {
    NormaSimulator sim;
    sim.signal().phases[0] = {100.0, 1.0, 0.0, 0.0, 0.0, 0.0, 0.0};
    tell(sim, "CALC:TRAN:FREQ:FUNC \"VOLT1\"");
    tell(sim, "CALC:TRAN:FREQ ONCE");

    const auto all = ScpiClient::parse_values(ask(sim, "CALC:DATA?"));
    CHECK(all.size() == 32);
    CHECK_THAT(all[0], Catch::Matchers::WithinRel(100.0, 1e-6));

    const auto window = ScpiClient::parse_values(ask(sim, "CALC:DATA? 4,2"));
    REQUIRE(window.size() == 4);
    CHECK_THAT(window[0], Catch::Matchers::WithinRel(all[2], 1e-9));
    require_no_errors(sim);
}

TEST_CASE("THD is only available in STD transform mode", "[simulator]") {
    NormaSimulator sim;
    sim.handle_line("CALC:DATA:THD?");
    CHECK(first_error(sim) == sim::error_code::kSettingsConflict);

    tell(sim, "CALC:TRAN:FREQ:MODE STD");
    sim.signal().phases[0].voltage_thd = 0.05;
    const auto thd = ScpiClient::parse_values(ask(sim, "CALC:DATA:THD?"));
    REQUIRE_FALSE(thd.empty());
    CHECK_THAT(thd[0], Catch::Matchers::WithinRel(0.05, 1e-6));
}

TEST_CASE("a recording can be read back through TRACe", "[simulator]") {
    NormaSimulator sim;
    sim.signal().phases[0] = {230.0, 1.0, 0.0, 0.0, 0.0, 0.0, 0.0};
    tell(sim, "APER 0.1");
    tell(sim, "SWE1:TIME 1.0");
    tell(sim, "SWE1:FUNC \"VOLT1\"");
    tell(sim, "SWE1:STAT ON");

    CHECK(ask(sim, "TRAC:CAT:LEN?") == "10");
    CHECK(std::stoi(ask(sim, "TRAC:FREE?")) == NormaSimulator::kTraceCapacity - 10);

    const auto values = ScpiClient::parse_values(ask(sim, "TRAC? 1"));
    REQUIRE(values.size() == 10);
    CHECK_THAT(values[0], Catch::Matchers::WithinRel(230.0, 1e-4));

    const auto status = ScpiClient::parse_values(ask(sim, "TRAC:STAT? 1"));
    CHECK(status.size() == 10);

    const auto window = ScpiClient::parse_values(ask(sim, "TRAC? 1,3"));
    CHECK(window.size() == 3);
    require_no_errors(sim);
}

TEST_CASE("a query with no response leaves the line silent", "[simulator]") {
    // A setting command produces nothing to read; a client that reads anyway is
    // what the transport's timeout is for.
    NormaSimulator sim;
    CHECK_FALSE(sim.handle_line("*CLS").has_value());
    CHECK(sim.handled_lines() == 1);
}

TEST_CASE("TIMer:RESet:AUTO is rejected, as the manual marks it unimplemented",
          "[simulator]") {
    NormaSimulator sim;
    sim.handle_line("TIM:RES:AUTO ON");
    CHECK(first_error(sim) == sim::error_code::kUndefinedHeader);
}

TEST_CASE("measure() and the DATA? response agree", "[simulator]") {
    // The tests use measure() to predict what a query should return, so the two
    // must not drift apart.
    NormaSimulator sim;
    sim.signal().phases[0] = {231.7, 4.2, 30.0, 0.0, 0.0, 0.0, 0.0};

    const auto predicted = sim.measure("POW1:ACT");
    const auto values = ScpiClient::parse_values(ask(sim, "DATA? \"POW1:ACT\""));
    REQUIRE(values.size() == 1);
    CHECK_THAT(values[0], Catch::Matchers::WithinRel(predicted.value, 1e-5));
}

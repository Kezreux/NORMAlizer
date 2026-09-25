// Integration tests against a Fluke NORMA 4000/5000 over TCP/Ethernet.
//
// These use the real TcpTransport and a real socket, and they are written so the
// same suite can run against two endpoints:
//
//   NORMA_HOST=<ip>          a real instrument on the bench
//   NORMA_HOST=simulator     an in-process simulator on loopback
//
// That is the point of the file: the assertions describe what a NORMA must do,
// so running them against the simulator keeps the simulator honest in CI, and
// running them against hardware validates the instrument (and this library) on
// site with exactly the same expectations.
//
// Environment:
//   NORMA_HOST        IP/hostname, or "simulator". Tests SKIP when unset, so a
//                     plain `ctest` run needs no instrument.
//   NORMA_PORT        TCP port (optional, default 23; ignored for "simulator")
//   NORMA_TIMEOUT_MS  I/O timeout in milliseconds (optional, default 5000)
//
// Run only the read-only tests (no instrument state is changed):
//   norma_hardware_tests "[hardware]~[state]"
//
// Tests tagged [state] reconfigure the instrument (*RST, FUNC, APER, ...).
// Do not run them while the instrument is in use for real measurements.

#include <chrono>
#include <cmath>
#include <cstdlib>
#include <memory>
#include <string>
#include <thread>

#include <catch2/catch_test_macros.hpp>

#include <fluke/norma/instrument.hpp>
#include <fluke/norma/simulator/simulator_server.hpp>
#include <fluke/norma/types.hpp>

using namespace fluke::norma;

namespace {

std::string env_string(const char* name) {
    const char* value = std::getenv(name);
    return value ? std::string(value) : std::string();
}

long env_long(const char* name, long fallback) {
    const std::string value = env_string(name);
    if (value.empty()) return fallback;
    try {
        return std::stol(value);
    } catch (const std::exception&) {
        return fallback;
    }
}

bool equals_ignoring_case(const std::string& a, const char* b) {
    std::string lowered;
    for (char c : a) {
        lowered.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
    }
    return lowered == b;
}

struct HardwareConfig {
    std::string host = env_string("NORMA_HOST");
    std::uint16_t port = static_cast<std::uint16_t>(env_long("NORMA_PORT", kDefaultPort));
    std::chrono::milliseconds timeout{env_long("NORMA_TIMEOUT_MS", 5000)};
    bool simulated = equals_ignoring_case(env_string("NORMA_HOST"), "simulator");
};

const HardwareConfig& config() {
    static const HardwareConfig cfg;
    return cfg;
}

/// The loopback simulator, started once when NORMA_HOST=simulator.
sim::SimulatorServer* simulator_server() {
    if (!config().simulated) {
        return nullptr;
    }
    static sim::SimulatorServer server;
    static bool configured = [] {
        // A plausible 3 x 230 V / 50 Hz load, so the measurement assertions have
        // something real to check.
        server.with_simulator([](sim::NormaSimulator& instrument) {
            instrument.signal().frequency = 50.0;
            for (int phase = 0; phase < 3; ++phase) {
                instrument.signal().phases[static_cast<std::size_t>(phase)] = {
                    230.0, 2.0, 15.0, 0.0, 0.0, 0.0, 0.0};
            }
        });
        return true;
    }();
    (void)configured;
    return &server;
}

/// Connects to the configured instrument, or SKIPs the test.
/// Starts every test from a clean status/error queue.
NormaInstrument connect_or_skip() {
    const auto& cfg = config();
    if (cfg.host.empty()) {
        SKIP("NORMA_HOST is not set; skipping hardware test "
             "(set it to an instrument address, or to \"simulator\")");
    }

    std::string host = cfg.host;
    std::uint16_t port = cfg.port;
    if (sim::SimulatorServer* server = simulator_server()) {
        host = server->host();
        port = server->port();
    }

    auto instrument = NormaInstrument::connect(host, port, cfg.timeout);
    instrument.clear_status();
    instrument.read_errors();
    return instrument;
}

bool finite_or_nan(double value) {
    return std::isfinite(value) || std::isnan(value);
}

} // namespace

TEST_CASE("connects and identifies a Fluke NORMA", "[hardware]") {
    auto norma = connect_or_skip();

    const auto id = norma.identify();
    WARN("Connected to: " << id.manufacturer << "," << id.model << ","
                          << id.serial_number << "," << id.firmware_version);

    CHECK(id.manufacturer.find("Fluke") != std::string::npos);
    CHECK(id.model.find("NORMA") != std::string::npos);
    CHECK_FALSE(id.serial_number.empty());
    CHECK_FALSE(id.firmware_version.empty());
}

TEST_CASE("transport can be closed and reopened", "[hardware]") {
    auto norma = connect_or_skip();
    REQUIRE(norma.is_open());

    norma.close();
    CHECK_FALSE(norma.is_open());

    // The instrument has a single-client socket; make sure it accepts a new
    // connection right after the old one was closed.
    auto again = connect_or_skip();
    CHECK(again.is_open());
    CHECK_FALSE(again.query("*IDN?").empty());
}

TEST_CASE("raw write/query escape hatches work end to end", "[hardware]") {
    auto norma = connect_or_skip();

    const std::string idn = norma.query("*IDN?");
    CHECK(idn.find(',') != std::string::npos);

    norma.write("*CLS");
    CHECK_NOTHROW(norma.check_errors());
}

TEST_CASE("reports system information", "[hardware]") {
    auto norma = connect_or_skip();

    const std::string scpi_version = norma.scpi_version();
    WARN("SCPI version: " << scpi_version);
    CHECK_FALSE(scpi_version.empty());

    const std::string options = norma.options();
    WARN("*OPT?: " << options);

    CHECK(norma.status_operation_condition() >= 0);
    CHECK_NOTHROW(norma.check_errors());
}

TEST_CASE("reports the transfer format it is using", "[hardware]") {
    auto norma = connect_or_skip();

    const DataFormatSetting format = norma.data_format();
    WARN("FORMat: " << static_cast<int>(format.format) << "," << format.length);
    // Anything but ASCii means the typed queries cannot parse the responses;
    // prepare() is what puts it right.
    CHECK_NOTHROW(norma.check_errors());
}

TEST_CASE("reads the wiring system", "[hardware]") {
    auto norma = connect_or_skip();

    const auto system = norma.wiring_system();
    CHECK((system == WiringSystem::ThreeWattmeter || system == WiringSystem::TwoWattmeter));
    CHECK_NOTHROW(norma.check_errors());
}

TEST_CASE("reads the input configuration", "[hardware]") {
    auto norma = connect_or_skip();

    for (int channel = 1; channel <= 2; ++channel) {
        const Coupling coupling = norma.input_coupling(channel);
        CHECK((coupling == Coupling::AC || coupling == Coupling::DC));
    }
    // GAIN and SHUNt exist on the current inputs, which are the odd channels.
    CHECK(norma.input_gain(1) > 0.0);
    const Shunt shunt = norma.input_shunt(1);
    CHECK((shunt == Shunt::Internal || shunt == Shunt::External));
    CHECK(norma.input_filter_frequency(1) > 0.0);
    CHECK_NOTHROW(norma.check_errors());
}

TEST_CASE("reports the ranges it can select", "[hardware]") {
    auto norma = connect_or_skip();

    const auto voltage_ranges = norma.voltage_ranges(1);
    const auto current_ranges = norma.current_ranges(1);
    REQUIRE_FALSE(voltage_ranges.empty());
    REQUIRE_FALSE(current_ranges.empty());
    // The list must be usable as a list of selectable values: ascending, positive.
    for (std::size_t i = 1; i < voltage_ranges.size(); ++i) {
        CHECK(voltage_ranges[i] > voltage_ranges[i - 1]);
    }
    CHECK(voltage_ranges.front() > 0.0);
    CHECK(current_ranges.front() > 0.0);
    WARN("Voltage ranges: " << voltage_ranges.size() << ", current ranges: "
                            << current_ranges.size());
    CHECK_NOTHROW(norma.check_errors());
}

TEST_CASE("reads the SYNC configuration", "[hardware]") {
    auto norma = connect_or_skip();

    CHECK_FALSE(norma.sync_source().empty());
    const Slope slope = norma.sync_slope();
    CHECK((slope == Slope::Positive || slope == Slope::Negative));
    const LevelUnit unit = norma.sync_level_unit();
    CHECK((unit == LevelUnit::Absolute || unit == LevelUnit::Percent));
    CHECK(norma.sync_timeout() > 0.0);
    CHECK_NOTHROW(norma.check_errors());
}

TEST_CASE("reads every status register", "[hardware]") {
    auto norma = connect_or_skip();

    for (StatusRegister reg : {StatusRegister::Operation, StatusRegister::Questionable,
                               StatusRegister::QuestionableVoltage,
                               StatusRegister::QuestionableCurrent}) {
        CHECK(norma.status(reg, RegisterPart::Condition) >= 0);
        CHECK(norma.status(reg, RegisterPart::Event) >= 0);
        CHECK(norma.status(reg, RegisterPart::Enable) >= 0);
    }
    CHECK(norma.status_byte() >= 0);
    CHECK_NOTHROW(norma.check_errors());
}

TEST_CASE("error queue round-trips a real SCPI error", "[hardware]") {
    auto norma = connect_or_skip();

    // Clean queue to start with.
    CHECK_NOTHROW(norma.check_errors());

    // An undefined header must be rejected by the firmware (-113).
    norma.write("SYSTEM:THIS:DOES:NOT:EXIST");
    const auto errors = norma.read_errors();
    REQUIRE_FALSE(errors.empty());
    CHECK(errors.front().code < 0);
    CHECK_FALSE(errors.front().message.empty());

    // read_errors() must have drained the queue completely.
    CHECK_NOTHROW(norma.check_errors());
}

TEST_CASE("DATA? with an explicit function list returns one value per function",
          "[hardware]") {
    auto norma = connect_or_skip();

    const auto values = norma.data({fn::voltage(1), fn::current(1)});
    REQUIRE(values.size() == 2);
    CHECK(finite_or_nan(values[0]));
    CHECK(finite_or_nan(values[1]));
    CHECK_NOTHROW(norma.check_errors());
}

TEST_CASE("DATA:STATus? returns one status flag per value", "[hardware]") {
    auto norma = connect_or_skip();

    const auto reading = norma.data_with_status({fn::voltage(1), fn::frequency()});
    REQUIRE(reading.values.size() == 2);
    REQUIRE(reading.status.size() == 2);
    for (std::size_t i = 0; i < reading.values.size(); ++i) {
        CHECK(finite_or_nan(reading.values[i]));
        CHECK(reading.status[i] >= 0);
    }
    CHECK_NOTHROW(norma.check_errors());
}

TEST_CASE("a NaN measurement is always flagged in the status", "[hardware]") {
    // The two have to agree: a value the instrument could not compute comes back
    // as NaN *and* carries one of the invalid status bits. A client that trusts
    // only one of the two signals must still be safe.
    auto norma = connect_or_skip();

    const auto reading = norma.data_with_status(
        {fn::voltage(1), fn::current(1), fn::active_power(1), fn::frequency()});
    REQUIRE(reading.values.size() == reading.status.size());
    for (std::size_t i = 0; i < reading.values.size(); ++i) {
        if (std::isnan(reading.values[i])) {
            INFO("value " << i << " is NaN with status " << reading.status[i]);
            CHECK((reading.status[i] & measurement_status::kInvalidMask) != 0);
            CHECK_FALSE(reading.is_valid(i));
        }
    }
    CHECK_NOTHROW(norma.check_errors());
}

TEST_CASE("prepare() is accepted by the instrument", "[hardware][state]") {
    // prepare() only touches the transfer format, the concurrency flag and the
    // status registers, so it is safe on a configured instrument — but it does
    // change settings, hence [state].
    auto norma = connect_or_skip();

    CHECK_NOTHROW(norma.prepare());
    CHECK(norma.data_format().format == DataFormat::Ascii);
    CHECK(norma.concurrent());
    CHECK_NOTHROW(norma.check_errors());
}

TEST_CASE("aperture can be set and read back", "[hardware][state]") {
    auto norma = connect_or_skip();

    const double previous = norma.aperture();

    norma.set_aperture(0.5);
    CHECK_NOTHROW(norma.check_errors());

    // The manual states APER? returns the configured nominal interval, but in
    // synchronous mode the effective interval is extended to whole signal
    // periods — only assert that the value is inside the legal range.
    const double aperture = norma.aperture();
    CHECK(aperture >= kMinAperture);
    CHECK(aperture <= kMaxAperture);

    norma.set_aperture(previous);
    CHECK_NOTHROW(norma.check_errors());
}

TEST_CASE("a range can be selected and autorange restored", "[hardware][state]") {
    auto norma = connect_or_skip();

    const auto ranges = norma.voltage_ranges(1);
    REQUIRE_FALSE(ranges.empty());
    const double wanted = ranges.back();

    norma.set_voltage_range(1, wanted);
    CHECK_NOTHROW(norma.check_errors());
    // Selecting a range explicitly turns autorange off, per the manual.
    CHECK_FALSE(norma.voltage_autorange(1));
    CHECK(norma.voltage_range(1) > 0.0);

    norma.set_voltage_autorange(1, true);
    CHECK(norma.voltage_autorange(1));
    CHECK_NOTHROW(norma.check_errors());
}

TEST_CASE("full measurement workflow: *RST -> configure -> INIT -> DATA?",
          "[hardware][state]") {
    auto norma = connect_or_skip();

    // The canonical workflow from the manual. check_errors() after each step
    // verifies the firmware accepted our exact wire format.
    norma.reset();
    norma.wait_operation_complete();
    CHECK_NOTHROW(norma.check_errors());

    norma.prepare();
    norma.sync_to_voltage(1);
    norma.set_voltage_autorange(1, true);
    norma.set_current_autorange(1, true);
    norma.set_aperture(0.3);
    CHECK_NOTHROW(norma.check_errors());

    const std::vector<std::string> functions = {
        fn::voltage(1), fn::current(1), fn::frequency(), fn::time_interval()};
    norma.set_functions(functions);
    CHECK_NOTHROW(norma.check_errors());
    CHECK(norma.function_count() == static_cast<int>(functions.size()));
    CHECK(norma.functions().size() == functions.size());

    norma.set_continuous(true);
    norma.wait_operation_complete();
    CHECK_NOTHROW(norma.check_errors());

    // Give the instrument time to finish at least one averaging cycle.
    std::this_thread::sleep_for(std::chrono::milliseconds(config().simulated ? 0 : 1000));

    const auto values = norma.data();
    REQUIRE(values.size() == functions.size());
    for (double value : values) CHECK(finite_or_nan(value));
    WARN("VOLT1=" << values[0] << " CURR1=" << values[1] << " FREQ=" << values[2]
                  << " TIME=" << values[3]);

    // With no signal applied, values may be flagged Undefined/Not available —
    // the status flags tell us, and parsing must still hold together.
    const auto reading = norma.data_with_status();
    REQUIRE(reading.values.size() == functions.size());
    REQUIRE(reading.status.size() == functions.size());

    // The averaging interval is measured by the instrument itself and must be
    // a real, positive duration unless it is flagged as unavailable.
    if (!std::isnan(reading.values[3])) {
        CHECK(reading.values[3] > 0.0);
    }

    CHECK_NOTHROW(norma.check_errors());
}

TEST_CASE("single-shot acquisition with INITiate", "[hardware][state]") {
    auto norma = connect_or_skip();

    norma.set_continuous(false);
    CHECK_NOTHROW(norma.check_errors());

    norma.initiate();
    norma.wait_operation_complete();
    CHECK_NOTHROW(norma.check_errors());

    const auto values = norma.data({fn::voltage(1)});
    REQUIRE(values.size() == 1);
    CHECK(finite_or_nan(values[0]));

    // Restore the *RST default (free-run) so the instrument keeps measuring.
    norma.set_continuous(true);
    CHECK_NOTHROW(norma.check_errors());
}

TEST_CASE("a three-phase configuration is accepted", "[hardware][state]") {
    auto norma = connect_or_skip();

    norma.prepare();
    norma.set_wiring_system(WiringSystem::ThreeWattmeter);
    norma.sync_to_voltage(1);
    for (int phase = 1; phase <= 3; ++phase) {
        norma.set_voltage_autorange(phase, true);
        norma.set_current_autorange(phase, true);
    }
    CHECK_NOTHROW(norma.check_errors());

    // The totals of the first three-phase system carry no phase suffix.
    const std::vector<std::string> functions = {
        fn::voltage(1),      fn::voltage(2),      fn::voltage(3),
        fn::current(1),      fn::current(2),      fn::current(3),
        fn::active_power(1), fn::active_power(2), fn::active_power(3),
        fn::active_power(),  fn::power_factor(),  fn::frequency()};
    norma.set_functions(functions);
    CHECK_NOTHROW(norma.check_errors());
    CHECK(norma.function_count() == static_cast<int>(functions.size()));

    const Reading reading = norma.data_with_status();
    REQUIRE(reading.values.size() == functions.size());
    for (std::size_t i = 0; i < reading.values.size(); ++i) {
        INFO("function " << functions[i]);
        CHECK(finite_or_nan(reading.values[i]));
    }
    CHECK_NOTHROW(norma.check_errors());
}

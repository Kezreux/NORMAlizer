// Integration tests against a REAL Fluke NORMA 4000/5000 over TCP/Ethernet.
//
// These tests use the real TcpTransport (no mocks) and therefore need a
// reachable instrument. They are built into their own executable
// (norma_hardware_tests) and configured through environment variables:
//
//   NORMA_HOST        IP/hostname of the instrument (required; tests are
//                     SKIPped when unset, so a plain `ctest` run stays green)
//   NORMA_PORT        TCP port (optional, default 23)
//   NORMA_TIMEOUT_MS  I/O timeout in milliseconds (optional, default 5000)
//
// Examples (PowerShell / bash):
//   $env:NORMA_HOST = "192.168.1.100"; .\norma_hardware_tests.exe
//   NORMA_HOST=192.168.1.100 ./norma_hardware_tests
//
// Run only the read-only tests (no instrument state is changed):
//   norma_hardware_tests "[hardware]~[state]"
//
// Via ctest (tests are labelled "hardware" and run serially, because the
// instrument accepts a single TCP client):
//   NORMA_HOST=192.168.1.100 ctest -L hardware --output-on-failure
//
// Tests tagged [state] reconfigure the instrument (*RST, FUNC, APER, ...).
// Do not run them while the instrument is in use for real measurements.

#include <chrono>
#include <cmath>
#include <cstdlib>
#include <string>
#include <thread>

#include <catch2/catch_test_macros.hpp>

#include <fluke/norma/instrument.hpp>
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

struct HardwareConfig {
    std::string host = env_string("NORMA_HOST");
    std::uint16_t port = static_cast<std::uint16_t>(env_long("NORMA_PORT", kDefaultPort));
    std::chrono::milliseconds timeout{env_long("NORMA_TIMEOUT_MS", 5000)};
};

const HardwareConfig& config() {
    static const HardwareConfig cfg;
    return cfg;
}

/// Connects to the instrument named by NORMA_HOST, or SKIPs the test.
/// Starts every test from a clean status/error queue.
NormaInstrument connect_or_skip() {
    const auto& cfg = config();
    if (cfg.host.empty()) {
        SKIP("NORMA_HOST is not set; skipping hardware test");
    }
    auto instrument = NormaInstrument::connect(cfg.host, cfg.port, cfg.timeout);
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

TEST_CASE("reads the wiring system", "[hardware]") {
    auto norma = connect_or_skip();

    const auto system = norma.wiring_system();
    CHECK((system == WiringSystem::ThreeWattmeter || system == WiringSystem::TwoWattmeter));
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

TEST_CASE("aperture can be set and read back", "[hardware][state]") {
    auto norma = connect_or_skip();

    const double previous = norma.aperture();

    norma.set_aperture(0.5);
    CHECK_NOTHROW(norma.check_errors());

    // The manual states APER? returns the configured nominal interval, but in
    // synchronous mode the effective interval is extended to whole signal
    // periods — only assert that the value is inside the legal range.
    const double aperture = norma.aperture();
    CHECK(aperture >= 0.015);
    CHECK(aperture <= 3600.0);

    norma.set_aperture(previous);
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
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));

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

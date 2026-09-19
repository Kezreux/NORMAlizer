#include "fluke/norma/instrument.hpp"

#include <stdexcept>
#include <utility>

#include "fluke/norma/error.hpp"
#include "fluke/norma/tcp_transport.hpp"

namespace fluke::norma {

namespace {

std::string join_quoted(const std::vector<std::string>& functions) {
    std::string list;
    for (std::size_t i = 0; i < functions.size(); ++i) {
        if (i != 0) {
            list += ',';
        }
        list += ScpiClient::quote(functions[i]);
    }
    return list;
}

} // namespace

NormaInstrument::NormaInstrument(std::unique_ptr<Transport> transport,
                                 std::chrono::milliseconds timeout)
    : scpi_(std::make_unique<ScpiClient>(std::move(transport), timeout)) {}

NormaInstrument NormaInstrument::connect(std::string host, std::uint16_t port,
                                         std::chrono::milliseconds timeout) {
    auto transport = std::make_unique<TcpTransport>(std::move(host), port);
    transport->set_connect_timeout(timeout);
    NormaInstrument instrument(std::move(transport), timeout);
    instrument.scpi_->open();
    return instrument;
}

void NormaInstrument::close() { scpi_->close(); }
bool NormaInstrument::is_open() const { return scpi_->is_open(); }

// -- Raw SCPI ----------------------------------------------------------------

void NormaInstrument::write(std::string_view scpi) { scpi_->send(scpi); }
std::string NormaInstrument::query(std::string_view scpi) { return scpi_->query(scpi); }

// -- IEEE 488.2 common commands ----------------------------------------------

Identification NormaInstrument::identify() {
    // Response e.g.: Fluke,NORMA4000,KN34512BA,01.00
    const auto fields = ScpiClient::split_csv(scpi_->query("*IDN?"));
    Identification id;
    if (fields.size() > 0) id.manufacturer = ScpiClient::unquote(fields[0]);
    if (fields.size() > 1) id.model = ScpiClient::unquote(fields[1]);
    if (fields.size() > 2) id.serial_number = ScpiClient::unquote(fields[2]);
    if (fields.size() > 3) id.firmware_version = ScpiClient::unquote(fields[3]);
    return id;
}

void NormaInstrument::reset() { scpi_->send("*RST"); }
void NormaInstrument::clear_status() { scpi_->send("*CLS"); }
std::string NormaInstrument::options() { return scpi_->query("*OPT?"); }
std::string NormaInstrument::scpi_version() { return scpi_->query("SYST:VERS?"); }

void NormaInstrument::wait_operation_complete(std::chrono::milliseconds timeout) {
    const std::string response = scpi_->query("*OPC?", timeout);
    if (response != "1") {
        throw ProtocolError("unexpected *OPC? response: \"" + response + "\"");
    }
}

void NormaInstrument::trigger() { scpi_->send("*TRG"); }

// -- Configuration -------------------------------------------------------------

void NormaInstrument::set_wiring_system(WiringSystem system) {
    scpi_->send(system == WiringSystem::TwoWattmeter ? "ROUT:SYST \"2W\""
                                                     : "ROUT:SYST \"3W\"");
}

WiringSystem NormaInstrument::wiring_system() {
    const std::string response = ScpiClient::unquote(scpi_->query("ROUT:SYST?"));
    if (response == "2W") return WiringSystem::TwoWattmeter;
    if (response == "3W") return WiringSystem::ThreeWattmeter;
    throw ProtocolError("unexpected ROUT:SYST? response: \"" + response + "\"");
}

void NormaInstrument::set_sync_source(std::string_view source) {
    scpi_->send("SYNC:SOUR " + std::string(source));
}

void NormaInstrument::sync_to_voltage(int phase) {
    check_phase(phase);
    set_sync_source("VOLT" + std::to_string(phase));
}

void NormaInstrument::sync_to_current(int phase) {
    check_phase(phase);
    set_sync_source("CURR" + std::to_string(phase));
}

void NormaInstrument::sync_external() { set_sync_source("EXT"); }

void NormaInstrument::set_voltage_range(int phase, double volts) {
    check_phase(phase);
    scpi_->send("VOLT" + std::to_string(phase) + ":RANG " + ScpiClient::format_double(volts));
}

void NormaInstrument::set_voltage_autorange(int phase, bool on) {
    check_phase(phase);
    scpi_->send("VOLT" + std::to_string(phase) + ":RANG:AUTO " + (on ? "ON" : "OFF"));
}

void NormaInstrument::set_current_range(int phase, double amps) {
    check_phase(phase);
    scpi_->send("CURR" + std::to_string(phase) + ":RANG " + ScpiClient::format_double(amps));
}

void NormaInstrument::set_current_autorange(int phase, bool on) {
    check_phase(phase);
    scpi_->send("CURR" + std::to_string(phase) + ":RANG:AUTO " + (on ? "ON" : "OFF"));
}

void NormaInstrument::set_aperture(double seconds) {
    scpi_->send("APER " + ScpiClient::format_double(seconds));
}

double NormaInstrument::aperture() { return scpi_->query_value("APER?"); }

void NormaInstrument::set_functions(const std::vector<std::string>& functions) {
    if (functions.empty()) {
        throw std::invalid_argument("set_functions: the function list must not be empty "
                                    "(use clear_functions() to turn all off)");
    }
    scpi_->send("FUNC " + join_quoted(functions));
}

std::vector<std::string> NormaInstrument::functions() {
    std::vector<std::string> result;
    for (const auto& field : ScpiClient::split_csv(scpi_->query("FUNC?"))) {
        result.push_back(ScpiClient::unquote(field));
    }
    return result;
}

int NormaInstrument::function_count() { return scpi_->query_int("FUNC:COUN?"); }

void NormaInstrument::clear_functions() { scpi_->send("FUNC:OFF:ALL"); }

// -- Acquisition ----------------------------------------------------------------

void NormaInstrument::set_continuous(bool on) {
    scpi_->send(on ? "INIT:CONT ON" : "INIT:CONT OFF");
}

void NormaInstrument::initiate() { scpi_->send("INIT"); }
void NormaInstrument::abort() { scpi_->send("ABOR"); }

std::vector<double> NormaInstrument::data(const std::vector<std::string>& functions) {
    if (functions.empty()) {
        return scpi_->query_values("DATA?");
    }
    return scpi_->query_values("DATA? " + join_quoted(functions));
}

Reading NormaInstrument::data_with_status(const std::vector<std::string>& functions) {
    const std::string command =
        functions.empty() ? std::string("DATA:STAT?") : "DATA:STAT? " + join_quoted(functions);
    const auto raw = scpi_->query_values(command);
    if (raw.size() % 2 != 0) {
        throw ProtocolError("DATA:STAT? returned an odd number of fields (" +
                            std::to_string(raw.size()) + ")");
    }

    Reading reading;
    const std::size_t count = raw.size() / 2;
    reading.values.assign(raw.begin(), raw.begin() + count);
    reading.status.reserve(count);
    for (std::size_t i = count; i < raw.size(); ++i) {
        reading.status.push_back(static_cast<int>(raw[i]));
    }
    return reading;
}

// -- Errors & status --------------------------------------------------------------

std::vector<ScpiErrorInfo> NormaInstrument::read_errors() { return scpi_->read_all_errors(); }
void NormaInstrument::check_errors() { scpi_->throw_if_error(); }

int NormaInstrument::status_operation_condition() {
    return scpi_->query_int("STAT:OPER:COND?");
}

// -- Plumbing ---------------------------------------------------------------------

void NormaInstrument::set_timeout(std::chrono::milliseconds timeout) {
    scpi_->set_default_timeout(timeout);
}

std::chrono::milliseconds NormaInstrument::timeout() const {
    return scpi_->default_timeout();
}

void NormaInstrument::check_phase(int phase) {
    if (phase < 1 || phase > 6) {
        throw std::invalid_argument("phase must be 1..6, got " + std::to_string(phase));
    }
}

} // namespace fluke::norma

// NormaInstrument: lifecycle, IEEE 488.2 common commands, STATus and errors.
// The configuration subsystems live in instrument_config.cpp and the
// measurement/acquisition ones in instrument_data.cpp.

#include "fluke/norma/instrument.hpp"

#include <stdexcept>
#include <utility>

#include "fluke/norma/error.hpp"
#include "fluke/norma/tcp_transport.hpp"
#include "scpi_keywords.hpp"

namespace fluke::norma {

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

void NormaInstrument::prepare() {
    clear_status();
    // This library parses ASCII responses only, and FORMat survives a
    // disconnect: a previous session may have left the instrument in REAL,64,
    // which would turn every measurement query into a binary block.
    set_data_format(DataFormat::Ascii, 6);
    set_concurrent(true);
    // Whatever the previous session left queued is not ours to report.
    read_errors();
}

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
std::string NormaInstrument::learn() { return scpi_->query("*LRN?"); }

void NormaInstrument::wait_operation_complete(std::chrono::milliseconds timeout) {
    const std::string response = scpi_->query("*OPC?", timeout);
    if (response != "1") {
        throw ProtocolError("unexpected *OPC? response: \"" + response + "\"");
    }
}

void NormaInstrument::set_operation_complete_flag() { scpi_->send("*OPC"); }
void NormaInstrument::wait_pending_operations() { scpi_->send("*WAI"); }
void NormaInstrument::trigger() { scpi_->send("*TRG"); }

void NormaInstrument::set_event_status_enable(int mask) {
    scpi_->send("*ESE " + std::to_string(mask));
}

int NormaInstrument::event_status_enable() { return scpi_->query_int("*ESE?"); }
int NormaInstrument::event_status() { return scpi_->query_int("*ESR?"); }

void NormaInstrument::set_service_request_enable(int mask) {
    scpi_->send("*SRE " + std::to_string(mask));
}

int NormaInstrument::service_request_enable() { return scpi_->query_int("*SRE?"); }
int NormaInstrument::status_byte() { return scpi_->query_int("*STB?"); }

void NormaInstrument::save_setup(int slot) {
    // The manual allows *SAV 10..24; 1 and 2 are read-only factory setups.
    if (slot < 10 || slot > 24) {
        throw std::invalid_argument("*SAV slot must be 10..24, got " + std::to_string(slot));
    }
    scpi_->send("*SAV " + std::to_string(slot));
}

void NormaInstrument::recall_setup(int slot) {
    if (slot != 1 && slot != 2 && (slot < 10 || slot > 24)) {
        throw std::invalid_argument("*RCL slot must be 1, 2 or 10..24, got " +
                                    std::to_string(slot));
    }
    scpi_->send("*RCL " + std::to_string(slot));
}

// -- STATus --------------------------------------------------------------------

std::string NormaInstrument::status_node(StatusRegister reg, RegisterPart part) {
    std::string node = "STAT:";
    switch (reg) {
    case StatusRegister::Operation:           node += "OPER"; break;
    case StatusRegister::Questionable:        node += "QUES"; break;
    case StatusRegister::QuestionableVoltage: node += "QUES:VOLT"; break;
    case StatusRegister::QuestionableCurrent: node += "QUES:CURR"; break;
    }
    switch (part) {
    case RegisterPart::Condition:          node += ":COND"; break;
    case RegisterPart::Event:              node += ":EVEN"; break;
    case RegisterPart::Enable:             node += ":ENAB"; break;
    case RegisterPart::PositiveTransition: node += ":PTR"; break;
    case RegisterPart::NegativeTransition: node += ":NTR"; break;
    }
    return node;
}

int NormaInstrument::status(StatusRegister reg, RegisterPart part) {
    return scpi_->query_int(status_node(reg, part) + "?");
}

void NormaInstrument::set_status(StatusRegister reg, RegisterPart part, int mask) {
    if (part == RegisterPart::Condition || part == RegisterPart::Event) {
        throw std::invalid_argument(
            "the CONDition and EVENt parts of a status register are read-only");
    }
    if (mask < 0 || mask > 65535) {
        throw std::invalid_argument("status register mask must be 0..65535, got " +
                                    std::to_string(mask));
    }
    scpi_->send(status_node(reg, part) + " " + std::to_string(mask));
}

int NormaInstrument::status_operation_condition() {
    return status(StatusRegister::Operation, RegisterPart::Condition);
}

// -- Errors ---------------------------------------------------------------------

std::vector<ScpiErrorInfo> NormaInstrument::read_errors() { return scpi_->read_all_errors(); }

std::vector<ScpiErrorInfo> NormaInstrument::read_errors_at_once() {
    return ScpiClient::parse_errors(scpi_->query("SYST:ERR:ALL?"));
}

void NormaInstrument::check_errors() { scpi_->throw_if_error(); }

// -- Plumbing ---------------------------------------------------------------------

void NormaInstrument::set_timeout(std::chrono::milliseconds timeout) {
    scpi_->set_default_timeout(timeout);
}

std::chrono::milliseconds NormaInstrument::timeout() const {
    return scpi_->default_timeout();
}

// -- Shared private helpers ---------------------------------------------------------

void NormaInstrument::check_phase(int phase) {
    if (phase < 1 || phase > kMaxPhase) {
        throw std::invalid_argument("phase must be 1.." + std::to_string(kMaxPhase) + ", got " +
                                    std::to_string(phase));
    }
}

void NormaInstrument::check_channel(int channel) {
    if (channel < 1 || channel > kMaxInputChannel) {
        throw std::invalid_argument("input channel must be 1.." +
                                    std::to_string(kMaxInputChannel) + ", got " +
                                    std::to_string(channel));
    }
}

std::string NormaInstrument::sense_node(const char* base, int phase) {
    check_phase(phase);
    return std::string(base) + std::to_string(phase);
}

std::string NormaInstrument::data_arguments(int count, int offset, int sparsing) {
    // The manual's data queries take positional optional arguments, so an
    // argument can only be given when every argument before it is.
    if (count <= 0) {
        return {};
    }
    std::string args = " " + std::to_string(count);
    if (offset > 0 || sparsing > 0) {
        args += "," + std::to_string(offset);
    }
    if (sparsing > 0) {
        args += "," + std::to_string(sparsing);
    }
    return args;
}

} // namespace fluke::norma

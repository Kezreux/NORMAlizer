// NormaInstrument: the configuration subsystems — ROUTe, INPut, SENSe ranging
// and scaling, SYNC, FORMat, DISPlay/OUTPut, SYSTem and TIMer.

#include "fluke/norma/instrument.hpp"

#include <stdexcept>
#include <string>

#include "fluke/norma/error.hpp"
#include "scpi_keywords.hpp"

namespace fluke::norma {

namespace {

/// "INP<n>" — the INPut subsystem addresses hardware channels 1..12.
std::string input_node(int channel) { return "INP" + std::to_string(channel); }

/// Formats "<a>,<b>,<c>" for the date/time style parameters.
std::string join_ints(std::initializer_list<int> values) {
    std::string out;
    for (int value : values) {
        if (!out.empty()) {
            out += ',';
        }
        out += std::to_string(value);
    }
    return out;
}

/// Sends a FORMat command: "<type>" or "<type>,<length>" when a length is given.
std::string format_parameter(DataFormat format, int length) {
    std::string parameter = detail::to_scpi(format);
    if (length > 0) {
        parameter += "," + std::to_string(length);
    }
    return parameter;
}

/// Parses "ASCii,6" (the length is optional in the response).
DataFormatSetting parse_format_setting(const std::string& response) {
    const auto fields = ScpiClient::split_csv(response);
    if (fields.empty()) {
        throw ProtocolError("empty FORMat? response");
    }
    DataFormatSetting setting;
    setting.format = detail::parse_data_format(fields[0]);
    setting.length = fields.size() > 1 ? ScpiClient::to_int(fields[1]) : 0;
    return setting;
}

} // namespace

// -- ROUTe --------------------------------------------------------------------

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

// -- INPut ---------------------------------------------------------------------

void NormaInstrument::set_input_coupling(int channel, Coupling coupling) {
    check_channel(channel);
    scpi_->send(input_node(channel) + ":COUP " + detail::to_scpi(coupling));
}

Coupling NormaInstrument::input_coupling(int channel) {
    check_channel(channel);
    return detail::parse_coupling(scpi_->query(input_node(channel) + ":COUP?"));
}

void NormaInstrument::set_input_gain(int channel, double gain) {
    check_channel(channel);
    scpi_->send(input_node(channel) + ":GAIN " + ScpiClient::format_double(gain));
}

double NormaInstrument::input_gain(int channel) {
    check_channel(channel);
    return scpi_->query_value(input_node(channel) + ":GAIN?");
}

void NormaInstrument::set_input_filter(int channel, bool on) {
    check_channel(channel);
    scpi_->send(input_node(channel) + ":FILT:STAT " + ScpiClient::format_bool(on));
}

bool NormaInstrument::input_filter(int channel) {
    check_channel(channel);
    return scpi_->query_bool(input_node(channel) + ":FILT:STAT?");
}

double NormaInstrument::input_filter_frequency(int channel) {
    check_channel(channel);
    return scpi_->query_value(input_node(channel) + ":FILT:LPAS:FREQ?");
}

void NormaInstrument::set_input_shunt(int channel, Shunt shunt) {
    check_channel(channel);
    scpi_->send(input_node(channel) + ":SHUN " + detail::to_scpi(shunt));
}

Shunt NormaInstrument::input_shunt(int channel) {
    check_channel(channel);
    return detail::parse_shunt(scpi_->query(input_node(channel) + ":SHUN?"));
}

// -- SENSe: ranging and scaling -------------------------------------------------

void NormaInstrument::set_voltage_range(int phase, double volts) {
    scpi_->send(sense_node("VOLT", phase) + ":RANG " + ScpiClient::format_double(volts));
}

double NormaInstrument::voltage_range(int phase) {
    return scpi_->query_value(sense_node("VOLT", phase) + ":RANG?");
}

void NormaInstrument::set_voltage_autorange(int phase, bool on) {
    scpi_->send(sense_node("VOLT", phase) + ":RANG:AUTO " + ScpiClient::format_bool(on));
}

bool NormaInstrument::voltage_autorange(int phase) {
    return scpi_->query_bool(sense_node("VOLT", phase) + ":RANG:AUTO?");
}

std::vector<double> NormaInstrument::voltage_ranges(int phase) {
    return scpi_->query_values(sense_node("VOLT", phase) + ":RANG:LIST?");
}

void NormaInstrument::set_voltage_scale(int phase, double ratio) {
    scpi_->send(sense_node("VOLT", phase) + ":SCAL " + ScpiClient::format_double(ratio));
}

double NormaInstrument::voltage_scale(int phase) {
    return scpi_->query_value(sense_node("VOLT", phase) + ":SCAL?");
}

void NormaInstrument::set_current_range(int phase, double amps) {
    scpi_->send(sense_node("CURR", phase) + ":RANG " + ScpiClient::format_double(amps));
}

double NormaInstrument::current_range(int phase) {
    return scpi_->query_value(sense_node("CURR", phase) + ":RANG?");
}

void NormaInstrument::set_current_autorange(int phase, bool on) {
    scpi_->send(sense_node("CURR", phase) + ":RANG:AUTO " + ScpiClient::format_bool(on));
}

bool NormaInstrument::current_autorange(int phase) {
    return scpi_->query_bool(sense_node("CURR", phase) + ":RANG:AUTO?");
}

std::vector<double> NormaInstrument::current_ranges(int phase) {
    return scpi_->query_values(sense_node("CURR", phase) + ":RANG:LIST?");
}

void NormaInstrument::set_current_scale(int phase, double ratio) {
    scpi_->send(sense_node("CURR", phase) + ":SCAL " + ScpiClient::format_double(ratio));
}

double NormaInstrument::current_scale(int phase) {
    return scpi_->query_value(sense_node("CURR", phase) + ":SCAL?");
}

void NormaInstrument::set_aperture(double seconds) {
    if (!(seconds >= kMinAperture && seconds <= kMaxAperture)) {
        throw std::invalid_argument("aperture must be " + std::to_string(kMinAperture) + ".." +
                                    std::to_string(kMaxAperture) + " s, got " +
                                    std::to_string(seconds));
    }
    scpi_->send("APER " + ScpiClient::format_double(seconds));
}

double NormaInstrument::aperture() { return scpi_->query_value("APER?"); }

double NormaInstrument::sampling_frequency() { return scpi_->query_value("SWE:FREQ?"); }

// -- SYNC -----------------------------------------------------------------------

void NormaInstrument::set_sync_enabled(bool on) {
    scpi_->send("SYNC:STAT " + ScpiClient::format_bool(on));
}

bool NormaInstrument::sync_enabled() { return scpi_->query_bool("SYNC:STAT?"); }

void NormaInstrument::set_sync_source(std::string_view source) {
    scpi_->send("SYNC:SOUR " + std::string(source));
}

std::string NormaInstrument::sync_source() {
    return ScpiClient::unquote(scpi_->query("SYNC:SOUR?"));
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

void NormaInstrument::set_sync_level(double level) {
    scpi_->send("SYNC:SOUR:LEV " + ScpiClient::format_double(level));
}

double NormaInstrument::sync_level() { return scpi_->query_value("SYNC:SOUR:LEV?"); }

void NormaInstrument::set_sync_level_unit(LevelUnit unit) {
    scpi_->send("SYNC:LEV:UNIT " + detail::to_scpi(unit));
}

LevelUnit NormaInstrument::sync_level_unit() {
    return detail::parse_level_unit(scpi_->query("SYNC:LEV:UNIT?"));
}

void NormaInstrument::set_sync_slope(Slope slope) {
    scpi_->send("SYNC:SOUR:SLOP " + detail::to_scpi(slope));
}

Slope NormaInstrument::sync_slope() {
    return detail::parse_slope(scpi_->query("SYNC:SOUR:SLOP?"));
}

void NormaInstrument::set_sync_filter(bool on) {
    scpi_->send("SYNC:SOUR:FILT:LPAS:STAT " + ScpiClient::format_bool(on));
}

bool NormaInstrument::sync_filter() {
    return scpi_->query_bool("SYNC:SOUR:FILT:LPAS:STAT?");
}

void NormaInstrument::set_sync_filter_frequency(double hertz) {
    scpi_->send("SYNC:SOUR:FILT:LPAS:FREQ " + ScpiClient::format_double(hertz));
}

double NormaInstrument::sync_filter_frequency() {
    return scpi_->query_value("SYNC:SOUR:FILT:LPAS:FREQ?");
}

void NormaInstrument::set_sync_timeout(double seconds) {
    scpi_->send("SYNC:TIM " + ScpiClient::format_double(seconds));
}

double NormaInstrument::sync_timeout() { return scpi_->query_value("SYNC:TIM?"); }

// -- FORMat ---------------------------------------------------------------------

void NormaInstrument::set_data_format(DataFormat format, int length) {
    scpi_->send("FORM " + format_parameter(format, length));
}

DataFormatSetting NormaInstrument::data_format() {
    return parse_format_setting(scpi_->query("FORM?"));
}

void NormaInstrument::set_status_format(DataFormat format, int length) {
    scpi_->send("FORM:STAT " + format_parameter(format, length));
}

DataFormatSetting NormaInstrument::status_format() {
    return parse_format_setting(scpi_->query("FORM:STAT?"));
}

void NormaInstrument::set_byte_order(ByteOrder order) {
    scpi_->send("FORM:BORD " + detail::to_scpi(order));
}

ByteOrder NormaInstrument::byte_order() {
    return detail::parse_byte_order(scpi_->query("FORM:BORD?"));
}

void NormaInstrument::set_transpose(bool on) {
    scpi_->send("FORM:TRAN " + ScpiClient::format_bool(on));
}

bool NormaInstrument::transpose() { return scpi_->query_bool("FORM:TRAN?"); }

// -- DISPlay and OUTPut ----------------------------------------------------------

void NormaInstrument::set_display_enabled(bool on) {
    scpi_->send("DISP:WIND:STAT " + ScpiClient::format_bool(on));
}

bool NormaInstrument::display_enabled() { return scpi_->query_bool("DISP:WIND:STAT?"); }

void NormaInstrument::set_display_functions(const std::vector<std::string>& functions) {
    if (functions.empty()) {
        throw std::invalid_argument("set_display_functions: the function list must not be empty");
    }
    scpi_->send("DISP:USER:FUNC " + ScpiClient::quote_join(functions));
}

std::vector<std::string> NormaInstrument::display_functions() {
    return scpi_->query_strings("DISP:USER:FUNC?");
}

void NormaInstrument::set_output_enabled(bool on) {
    scpi_->send("OUTP9:STAT " + ScpiClient::format_bool(on));
}

bool NormaInstrument::output_enabled() { return scpi_->query_bool("OUTP9:STAT?"); }

// -- SYSTem -----------------------------------------------------------------------

std::string NormaInstrument::scpi_version() { return scpi_->query("SYST:VERS?"); }

void NormaInstrument::set_key_lock(KeyLock lock) {
    scpi_->send("SYST:KLOC " + detail::to_scpi(lock));
}

KeyLock NormaInstrument::key_lock() {
    return detail::parse_key_lock(scpi_->query("SYST:KLOC?"));
}

void NormaInstrument::set_date(const Date& date) {
    scpi_->send("SYST:DATE " + join_ints({date.year, date.month, date.day}));
}

Date NormaInstrument::date() {
    const std::string response = scpi_->query("SYST:DATE?");
    const auto fields = ScpiClient::split_csv(response);
    if (fields.size() < 3) {
        throw ProtocolError("expected <year>,<month>,<day> from SYST:DATE?, got \"" +
                            response + "\"");
    }
    return Date{ScpiClient::to_int(fields[0]), ScpiClient::to_int(fields[1]),
                ScpiClient::to_int(fields[2])};
}

void NormaInstrument::set_time(const Time& time) {
    scpi_->send("SYST:TIME " + join_ints({time.hours, time.minutes, time.seconds}));
}

Time NormaInstrument::time_of_day() {
    const std::string response = scpi_->query("SYST:TIME?");
    const auto fields = ScpiClient::split_csv(response);
    if (fields.size() < 3) {
        throw ProtocolError("expected <hours>,<minutes>,<seconds> from SYST:TIME?, got \"" +
                            response + "\"");
    }
    return Time{ScpiClient::to_int(fields[0]), ScpiClient::to_int(fields[1]),
                ScpiClient::to_int(fields[2])};
}

void NormaInstrument::set_gpib_address(int address) {
    if (address < 1 || address > 30) {
        throw std::invalid_argument("GPIB address must be 1..30, got " + std::to_string(address));
    }
    scpi_->send("SYST:COMM:GPIB:ADDR " + std::to_string(address));
}

int NormaInstrument::gpib_address() { return scpi_->query_int("SYST:COMM:GPIB:ADDR?"); }

void NormaInstrument::set_serial_baud(int baud) {
    scpi_->send("SYST:COMM:SER:BAUD " + std::to_string(baud));
}

int NormaInstrument::serial_baud() { return scpi_->query_int("SYST:COMM:SER:BAUD?"); }

void NormaInstrument::set_language(std::string_view language) {
    scpi_->send("SYST:LANG " + ScpiClient::quote(language));
}

std::string NormaInstrument::language() {
    return ScpiClient::unquote(scpi_->query("SYST:LANG?"));
}

// -- TIMer -------------------------------------------------------------------------

void NormaInstrument::reset_timer() { scpi_->send("TIM:RES"); }

double NormaInstrument::timer_reset_time() { return scpi_->query_value("TIM:RES:TIME?"); }

} // namespace fluke::norma

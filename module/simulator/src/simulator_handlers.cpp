// The simulator's per-subsystem command handlers.
//
// Each handler recognizes the command keys of one subsystem, applies the
// setting to SimulatorState or produces the query response. Parameter
// validation mirrors the manual's documented ranges and rejects violations into
// the error queue with the SCPI code a real instrument would use, so a client
// that ignores SYST:ERR? behaves visibly differently from one that checks it.

#include <cmath>
#include <cstdio>
#include <string>

#include "simulator_internal.hpp"

namespace fluke::norma::sim {

namespace {

using detail::format_bool;
using detail::format_setting;
using detail::join_values;
using detail::parse_number;
using detail::quote_join;
using detail::unquote;
using detail::upper;

} // namespace

// -- IEEE 488.2 common commands -----------------------------------------------------

bool NormaSimulator::handle_common(Command& command) {
    Access access{*this, command};
    const std::string& key = command.key;

    if (key == "*IDN") {
        if (!access.query_only()) return true;
        const Identification& id = options_.identification;
        access.respond(id.manufacturer + "," + id.model + "," + id.serial_number + "," +
                       id.firmware_version);
    } else if (key == "*RST") {
        if (!access.command_only()) return true;
        reset();
    } else if (key == "*CLS") {
        if (!access.command_only()) return true;
        clear_status();
    } else if (key == "*OPT") {
        if (!access.query_only()) return true;
        access.respond(options_.options_response);
    } else if (key == "*LRN") {
        if (!access.query_only()) return true;
        // A real *LRN? returns the whole setup as a command line; the shape is
        // what matters to a client, so report the settings a client can check.
        access.respond("\"FUNC " + quote_join(state_.functions) + ";APER " +
                       format_setting(state_.aperture) + "\"");
    } else if (key == "*OPC") {
        if (command.query) {
            // Everything this simulator does is synchronous, so all pending
            // operations are complete by the time the query is parsed.
            access.respond("1");
        } else {
            state_.event_status |= 1 << 0; // Operation Complete bit
        }
    } else if (key == "*WAI") {
        if (!access.command_only()) return true;
    } else if (key == "*TRG") {
        if (!access.command_only()) return true;
        state_.initiated = true;
    } else if (key == "*ESE") {
        access.integer(state_.event_status_enable, 0, 255);
    } else if (key == "*ESR") {
        if (!access.query_only()) return true;
        access.respond(std::to_string(state_.event_status));
        state_.event_status = 0; // the ESR clears on read
    } else if (key == "*SRE") {
        access.integer(state_.service_request_enable, 0, 255);
    } else if (key == "*STB") {
        if (!access.query_only()) return true;
        int stb = 0;
        if (!state_.errors.empty()) {
            stb |= status_byte::kErrorQueueNotEmpty;
        }
        if ((latch_event(StatusRegister::Questionable) & state_.questionable.enable) != 0) {
            stb |= status_byte::kQuestionableSummary;
        }
        if ((state_.event_status & state_.event_status_enable) != 0) {
            stb |= status_byte::kEventStatusSummary;
        }
        if ((stb & state_.service_request_enable) != 0) {
            stb |= status_byte::kMasterStatusSummary;
        }
        access.respond(std::to_string(stb));
    } else if (key == "*SAV") {
        if (!access.command_only()) return true;
        int slot = 0;
        access.integer(slot, 10, 24);
        if (!command.failed) {
            state_.saved_setups[static_cast<std::size_t>(slot)] = true;
        }
    } else if (key == "*RCL") {
        if (!access.command_only()) return true;
        int slot = 0;
        access.integer(slot, 1, 24);
        if (!command.failed && slot > 2 && slot < 10) {
            access.fail(error_code::kDataOutOfRange, "Data out of range;*RCL");
        }
    } else {
        return false;
    }
    return true;
}

// -- ROUTe ----------------------------------------------------------------------------

bool NormaSimulator::handle_route(Command& command) {
    Access access{*this, command};
    if (command.key != "ROUT:SYST") {
        return false;
    }
    if (command.query) {
        access.respond(state_.wiring_system == WiringSystem::TwoWattmeter ? "\"2W\"" : "\"3W\"");
        return true;
    }
    auto text = access.parameter();
    if (!text) {
        return true;
    }
    const std::string value = upper(*text);
    if (value == "3W") {
        state_.wiring_system = WiringSystem::ThreeWattmeter;
    } else if (value == "2W") {
        state_.wiring_system = WiringSystem::TwoWattmeter;
    } else {
        access.fail(error_code::kIllegalParameterValue, "Illegal parameter value;" + *text);
    }
    return true;
}

// -- INPut ------------------------------------------------------------------------------

bool NormaSimulator::handle_input(Command& command) {
    Access access{*this, command};
    const std::string& key = command.key;
    if (key.compare(0, 4, "INP#") != 0) {
        return false;
    }

    const int channel = command.suffix();
    if (channel < 1 || channel > options_.channels) {
        access.fail(error_code::kDataOutOfRange,
                    "Data out of range;input channel " + std::to_string(channel));
        return true;
    }
    const auto index = static_cast<std::size_t>(channel);

    if (key == "INP#:COUP") {
        access.keyword<Coupling>(state_.coupling[index],
                                 {{Coupling::AC, {"AC"}}, {Coupling::DC, {"DC"}}});
    } else if (key == "INP#:GAIN") {
        if (channel % 2 == 0) {
            // GAIN is the external-shunt factor of a current input, and the
            // current inputs are the odd channels.
            access.fail(error_code::kSettingsConflict,
                        "Settings conflict;GAIN applies to current channels only");
            return true;
        }
        access.number(state_.gain[index], 0.0, 1.0e12);
    } else if (key == "INP#:SHUN") {
        if (channel % 2 == 0) {
            access.fail(error_code::kSettingsConflict,
                        "Settings conflict;SHUNt applies to current channels only");
            return true;
        }
        access.keyword<Shunt>(state_.shunt[index], {{Shunt::Internal, {"INT", "INTernal"}},
                                                    {Shunt::External, {"EXT", "EXTernal"}}});
    } else if (key == "INP#:FILT:STAT") {
        access.boolean(state_.filter[index]);
    } else if (key == "INP#:FILT:FREQ") {
        if (!access.query_only()) return true;
        access.respond(format_setting(state_.filter_frequency));
    } else {
        return false;
    }
    return true;
}

// -- SENSe: ranging, scaling, functions, data ---------------------------------------------

bool NormaSimulator::handle_sense(Command& command) {
    Access access{*this, command};
    const std::string& key = command.key;

    const bool is_voltage = key.compare(0, 5, "VOLT#") == 0;
    const bool is_current = key.compare(0, 5, "CURR#") == 0;
    if (is_voltage || is_current) {
        const int phase = command.suffix();
        if (phase < 1 || phase > kMaxPhase) {
            access.fail(error_code::kDataOutOfRange,
                        "Data out of range;phase " + std::to_string(phase));
            return true;
        }
        auto& config = is_voltage ? state_.voltage[static_cast<std::size_t>(phase)]
                                  : state_.current[static_cast<std::size_t>(phase)];
        const std::vector<double>& ranges =
            is_voltage ? options_.voltage_ranges : options_.current_ranges;
        const std::string tail = key.substr(5);

        if (tail == ":RANG") {
            if (command.query) {
                access.respond(format_setting(config.range));
                return true;
            }
            auto text = access.parameter();
            if (!text) {
                return true;
            }
            double value = 0.0;
            if (!parse_number(*text, value)) {
                access.fail(error_code::kIllegalParameterValue, "Illegal parameter value;" + *text);
                return true;
            }
            if (value < ranges.front() * 0.999 || value > ranges.back() * 1.001) {
                access.fail(error_code::kDataOutOfRange, "Data out of range;" + *text);
                return true;
            }
            // The instrument snaps to the next range that can hold the request
            // and leaves autorange off, as documented for RANGe.
            config.range = ranges.back();
            for (double candidate : ranges) {
                if (candidate >= value * 0.999) {
                    config.range = candidate;
                    break;
                }
            }
            config.autorange = false;
        } else if (tail == ":RANG:AUTO") {
            access.boolean(config.autorange);
        } else if (tail == ":RANG:LIST") {
            if (!access.query_only()) return true;
            access.respond(join_values(ranges));
        } else if (tail == ":SCAL") {
            access.number(config.scale, 0.9, 1.0e7);
        } else {
            return false;
        }
        return true;
    }

    if (key == "APER") {
        access.number(state_.aperture, kMinAperture, kMaxAperture);
    } else if (key == "SWE:FREQ") {
        if (!access.query_only()) return true;
        access.respond(format_setting(state_.sampling_frequency));
    } else if (key == "FUNC") {
        if (command.query) {
            access.respond(quote_join(state_.functions));
            return true;
        }
        std::vector<std::string> functions;
        if (!access.read_function_list(functions)) {
            return true;
        }
        if (!state_.concurrent && functions.size() > 1) {
            // With CONCurrent OFF, FUNCtion is a one-of-n switch.
            access.fail(error_code::kSettingsConflict,
                        "Settings conflict;FUNCtion:CONCurrent is OFF");
            return true;
        }
        state_.functions = std::move(functions);
    } else if (key == "FUNC:ALL") {
        if (!access.command_only()) return true;
        state_.functions = {"VOLT1", "VOLT2", "VOLT3", "CURR1", "CURR2", "CURR3",
                            "POW1:ACT", "POW2:ACT", "POW3:ACT", "FREQ", "TIME"};
    } else if (key == "FUNC:OFF:ALL") {
        if (!access.command_only()) return true;
        state_.functions.clear();
    } else if (key == "FUNC:CONC") {
        const bool was = state_.concurrent;
        access.boolean(state_.concurrent);
        if (!command.query && was && !state_.concurrent && state_.functions.size() > 1) {
            // CONCurrent OFF invalidates a multi-function list.
            state_.functions.resize(1);
        }
    } else if (key == "FUNC:COUN") {
        if (!access.query_only()) return true;
        access.respond(std::to_string(state_.functions.size()));
    } else if (key == "DATA" || key == "DATA:STAT") {
        if (!access.query_only()) return true;
        if (!command.args.empty()) {
            std::vector<std::string> requested;
            if (!access.read_function_list(requested)) {
                return true;
            }
            // Per the manual, DATA? with an explicit list also replaces the
            // configured FUNCtion list.
            state_.functions = std::move(requested);
        }
        access.respond(measurement_response(state_.functions, key == "DATA:STAT"));
    } else if (key.compare(0, 4, "SWE#") == 0) {
        const int block = command.suffix();
        if (block < 1 || block > 2) {
            access.fail(error_code::kDataOutOfRange,
                        "Data out of range;sweep block " + std::to_string(block));
            return true;
        }
        auto& sweep = state_.sweep[static_cast<std::size_t>(block)];
        const std::string tail = key.substr(4);
        if (tail == ":STAT") {
            access.boolean(sweep.enabled);
        } else if (tail == ":TIME") {
            if (!command.query && command.argument(0).has_value() &&
                upper(*command.argument(0)) == "MAX") {
                sweep.time = 3600.0;
                return true;
            }
            access.number(sweep.time, 1.0e-3, 3600.0);
        } else if (tail == ":OFFS:TIME") {
            access.number(sweep.offset_time, -3600.0, 3600.0);
        } else if (tail == ":COUN") {
            access.integer(sweep.count, 1, 1000000);
        } else if (tail == ":SFAC") {
            access.integer(sweep.sparsing, 1, 65535);
        } else if (tail == ":FUNC") {
            access.function_list(sweep.functions);
        } else if (tail == ":POINTS") {
            if (!access.query_only()) return true;
            access.respond(std::to_string(
                static_cast<long long>(sweep.time / std::max(state_.aperture, kMinAperture))));
        } else if (tail == ":OFFS:POINTS") {
            if (!access.query_only()) return true;
            access.respond(std::to_string(static_cast<long long>(
                sweep.offset_time / std::max(state_.aperture, kMinAperture))));
        } else {
            return false;
        }
        return true;
    } else {
        return false;
    }
    return true;
}

// -- SYNC -------------------------------------------------------------------------------

bool NormaSimulator::handle_sync(Command& command) {
    Access access{*this, command};
    const std::string& key = command.key;

    if (key == "SYNC:STAT") {
        access.boolean(state_.sync_enabled);
    } else if (key == "SYNC:SOUR") {
        if (command.query) {
            access.respond(state_.sync_source);
            return true;
        }
        auto text = access.parameter();
        if (!text) {
            return true;
        }
        const Token token = make_token(*text);
        const bool voltage = detail::mnemonic_matches(token.text, "VOLTage");
        const bool current = detail::mnemonic_matches(token.text, "CURRent");
        const bool external = detail::mnemonic_matches(token.text, "EXTernal");
        if (external) {
            state_.sync_source = "EXT";
        } else if ((voltage || current) && token.suffix >= 1 && token.suffix <= kMaxPhase) {
            state_.sync_source = (voltage ? "VOLT" : "CURR") + std::to_string(token.suffix);
        } else if ((voltage || current) && !token.has_suffix) {
            state_.sync_source = std::string(voltage ? "VOLT" : "CURR") + "1";
        } else {
            access.fail(error_code::kIllegalParameterValue, "Illegal parameter value;" + *text);
        }
    } else if (key == "SYNC:LEV") {
        const double limit = state_.sync_level_unit == LevelUnit::Percent ? 150.0 : 1000.0;
        access.number(state_.sync_level, -limit, limit);
    } else if (key == "SYNC:LEV:UNIT") {
        access.keyword<LevelUnit>(state_.sync_level_unit,
                                  {{LevelUnit::Absolute, {"ABS", "ABSolute"}},
                                   {LevelUnit::Percent, {"PCT", "PERCent"}}});
    } else if (key == "SYNC:SLOP") {
        access.keyword<Slope>(state_.sync_slope, {{Slope::Positive, {"POS", "POSitive"}},
                                                 {Slope::Negative, {"NEG", "NEGative"}}});
    } else if (key == "SYNC:FILT:STAT") {
        access.boolean(state_.sync_filter);
    } else if (key == "SYNC:FILT:FREQ") {
        if (command.query) {
            access.respond(format_setting(state_.sync_filter_frequency));
            return true;
        }
        auto text = access.parameter();
        if (!text) {
            return true;
        }
        double value = 0.0;
        if (!parse_number(*text, value)) {
            access.fail(error_code::kIllegalParameterValue, "Illegal parameter value;" + *text);
            return true;
        }
        // Only the three documented cut-off frequencies exist.
        for (double allowed : {100.0, 1.0e3, 1.0e4}) {
            if (std::fabs(value - allowed) <= allowed * 1e-6) {
                state_.sync_filter_frequency = allowed;
                return true;
            }
        }
        access.fail(error_code::kIllegalParameterValue, "Illegal parameter value;" + *text);
    } else if (key == "SYNC:TIM") {
        access.number(state_.sync_timeout, 0.015, 3600.0);
    } else {
        return false;
    }
    return true;
}

// -- INITiate / ABORt ---------------------------------------------------------------------

bool NormaSimulator::handle_acquisition(Command& command) {
    Access access{*this, command};
    const std::string& key = command.key;

    if (key == "INIT:CONT") {
        access.boolean(state_.continuous);
    } else if (key == "INIT") {
        if (!access.command_only()) return true;
        state_.initiated = true;
    } else if (key == "INIT:SEQ#" || key == "INIT:NAME") {
        if (!access.command_only()) return true;
        const int block = key == "INIT:SEQ#" ? command.suffix() : 1;
        if (block < 1 || block > 2) {
            access.fail(error_code::kDataOutOfRange,
                        "Data out of range;sequence " + std::to_string(block));
            return true;
        }
        state_.sweep[static_cast<std::size_t>(block)].enabled = true;
    } else if (key == "ABOR") {
        if (!access.command_only()) return true;
        state_.initiated = false;
        state_.sweep[1].enabled = false;
        state_.sweep[2].enabled = false;
    } else {
        return false;
    }
    return true;
}

// -- TRIGger -------------------------------------------------------------------------------

bool NormaSimulator::handle_trigger(Command& command) {
    Access access{*this, command};
    const std::string& key = command.key;

    if (key == "TRIG:STAR:SOUR") {
        if (command.query) {
            access.respond(state_.trigger_start_source);
            return true;
        }
        auto text = access.parameter();
        if (text) {
            state_.trigger_start_source = upper(*text);
        }
    } else if (key == "TRIG:STOP:SOUR") {
        if (command.query) {
            access.respond(state_.trigger_stop_source);
            return true;
        }
        auto text = access.parameter();
        if (text) {
            state_.trigger_stop_source = upper(*text);
        }
    } else if (key == "TRIG:STAR:LEV") {
        access.number(state_.trigger_start_level, -1.0e7, 1.0e7);
    } else if (key == "TRIG:STOP:LEV") {
        access.number(state_.trigger_stop_level, -1.0e7, 1.0e7);
    } else if (key == "TRIG:STAR:SLOP") {
        access.keyword<Slope>(state_.trigger_start_slope,
                              {{Slope::Positive, {"POS", "POSitive"}},
                               {Slope::Negative, {"NEG", "NEGative"}}});
    } else if (key == "TRIG:STOP:SLOP") {
        access.keyword<Slope>(state_.trigger_stop_slope,
                              {{Slope::Positive, {"POS", "POSitive"}},
                               {Slope::Negative, {"NEG", "NEGative"}}});
    } else if (key == "TRIG:STAR:TIME" || key == "TRIG:STOP:TIME") {
        if (!access.command_only()) return true;
        if (command.args.size() < 6) {
            access.fail(error_code::kMissingParameter, "Missing parameter;expected yyyy,MM,dd,hh,mm,ss");
        }
    } else {
        return false;
    }
    return true;
}

// -- FORMat -----------------------------------------------------------------------------------

bool NormaSimulator::handle_format(Command& command) {
    Access access{*this, command};
    const std::string& key = command.key;
    const bool is_data = key == "FORM";
    const bool is_status = key == "FORM:STAT";

    if (is_data || is_status) {
        DataFormatSetting& setting = is_data ? state_.data_format : state_.status_format;
        if (command.query) {
            std::string type = "ASC";
            if (setting.format == DataFormat::Integer) type = "INT";
            if (setting.format == DataFormat::Real) type = "REAL";
            access.respond(type + "," + std::to_string(setting.length));
            return true;
        }
        auto text = access.parameter();
        if (!text) {
            return true;
        }
        DataFormat format = setting.format;
        const std::string value = upper(*text);
        if (value == "ASC" || value == "ASCII") {
            format = DataFormat::Ascii;
        } else if (value == "INT" || value == "INTEGER") {
            format = DataFormat::Integer;
        } else if (value == "REAL") {
            format = DataFormat::Real;
        } else {
            access.fail(error_code::kIllegalParameterValue, "Illegal parameter value;" + *text);
            return true;
        }
        int length = setting.length;
        if (command.args.size() > 1) {
            double parsed = 0.0;
            if (!parse_number(command.args[1], parsed)) {
                access.fail(error_code::kIllegalParameterValue,
                            "Illegal parameter value;" + command.args[1]);
                return true;
            }
            length = static_cast<int>(std::lround(parsed));
        }
        setting.format = format;
        setting.length = length;
        // The two FORMat settings are coupled: a change between text and binary
        // applies to both.
        const bool binary = format != DataFormat::Ascii;
        DataFormatSetting& other = is_data ? state_.status_format : state_.data_format;
        if ((other.format != DataFormat::Ascii) != binary) {
            other.format = binary ? (is_data ? DataFormat::Integer : DataFormat::Real)
                                  : DataFormat::Ascii;
        }
        return true;
    }

    if (key == "FORM:BORD") {
        access.keyword<ByteOrder>(state_.byte_order, {{ByteOrder::Normal, {"NORM", "NORMal"}},
                                                      {ByteOrder::Swapped, {"SWAP", "SWAPped"}}});
    } else if (key == "FORM:TRAN") {
        access.boolean(state_.transpose);
    } else {
        return false;
    }
    return true;
}

// -- DISPlay / OUTPut ----------------------------------------------------------------------------

bool NormaSimulator::handle_display_output(Command& command) {
    Access access{*this, command};
    const std::string& key = command.key;

    if (key == "DISP:STAT") {
        access.boolean(state_.display_enabled);
    } else if (key == "DISP:USER:FUNC") {
        access.function_list(state_.display_functions);
    } else if (key == "OUTP#:STAT") {
        if (command.suffix() != 9) {
            access.fail(error_code::kUndefinedHeader,
                        "Undefined header;OUTPut" + std::to_string(command.suffix()));
            return true;
        }
        access.boolean(state_.output_enabled);
    } else {
        return false;
    }
    return true;
}

// -- SYSTem ----------------------------------------------------------------------------------------

bool NormaSimulator::handle_system(Command& command) {
    Access access{*this, command};
    const std::string& key = command.key;

    if (key == "SYST:VERS") {
        if (!access.query_only()) return true;
        access.respond(options_.scpi_version);
    } else if (key == "SYST:ERR") {
        if (!access.query_only()) return true;
        if (state_.errors.empty()) {
            access.respond("0,\"No error\"");
        } else {
            const ScpiErrorInfo entry = state_.errors.front();
            state_.errors.pop_front();
            access.respond(std::to_string(entry.code) + ",\"" + entry.message + "\"");
        }
    } else if (key == "SYST:ERR:ALL") {
        if (!access.query_only()) return true;
        if (state_.errors.empty()) {
            access.respond("0,\"No error\"");
        } else {
            std::string out;
            while (!state_.errors.empty()) {
                if (!out.empty()) {
                    out += ',';
                }
                const ScpiErrorInfo entry = state_.errors.front();
                state_.errors.pop_front();
                out += std::to_string(entry.code) + ",\"" + entry.message + "\"";
            }
            access.respond(out);
        }
    } else if (key == "SYST:KLOC") {
        access.keyword<KeyLock>(state_.key_lock, {{KeyLock::Off, {"OFF", "0"}},
                                                  {KeyLock::On, {"ON", "1"}},
                                                  {KeyLock::Remote, {"REM", "REMote"}}});
    } else if (key == "SYST:LANG") {
        if (command.query) {
            access.respond("\"" + state_.language + "\"");
            return true;
        }
        auto text = access.parameter();
        if (text) {
            state_.language = *text;
        }
    } else if (key == "SYST:DATE") {
        if (command.query) {
            access.respond(std::to_string(state_.date.year) + "," +
                           std::to_string(state_.date.month) + "," +
                           std::to_string(state_.date.day));
            return true;
        }
        if (command.args.size() < 3) {
            access.fail(error_code::kMissingParameter, "Missing parameter;expected year,month,day");
            return true;
        }
        double year = 0.0;
        double month = 0.0;
        double day = 0.0;
        if (!parse_number(command.args[0], year) || !parse_number(command.args[1], month) ||
            !parse_number(command.args[2], day)) {
            access.fail(error_code::kIllegalParameterValue, "Illegal parameter value;SYSTem:DATE");
            return true;
        }
        if (month < 1 || month > 12 || day < 1 || day > 31) {
            access.fail(error_code::kDataOutOfRange, "Data out of range;SYSTem:DATE");
            return true;
        }
        state_.date = Date{static_cast<int>(year), static_cast<int>(month), static_cast<int>(day)};
    } else if (key == "SYST:TIME") {
        if (command.query) {
            access.respond(std::to_string(state_.time.hours) + "," +
                           std::to_string(state_.time.minutes) + "," +
                           std::to_string(state_.time.seconds));
            return true;
        }
        if (command.args.size() < 3) {
            access.fail(error_code::kMissingParameter,
                        "Missing parameter;expected hours,minutes,seconds");
            return true;
        }
        double hours = 0.0;
        double minutes = 0.0;
        double seconds = 0.0;
        if (!parse_number(command.args[0], hours) || !parse_number(command.args[1], minutes) ||
            !parse_number(command.args[2], seconds)) {
            access.fail(error_code::kIllegalParameterValue, "Illegal parameter value;SYSTem:TIME");
            return true;
        }
        if (hours < 0 || hours > 23 || minutes < 0 || minutes > 59 || seconds < 0 ||
            seconds > 59) {
            access.fail(error_code::kDataOutOfRange, "Data out of range;SYSTem:TIME");
            return true;
        }
        state_.time = Time{static_cast<int>(hours), static_cast<int>(minutes),
                           static_cast<int>(seconds)};
    } else if (key == "SYST:COMM:GPIB:ADDR") {
        access.integer(state_.gpib_address, 1, 30);
    } else if (key == "SYST:COMM:SER:BAUD") {
        access.integer(state_.serial_baud, 1200, 115200);
    } else {
        return false;
    }
    return true;
}

// -- TIMer -----------------------------------------------------------------------------------------

bool NormaSimulator::handle_timer(Command& command) {
    Access access{*this, command};
    const std::string& key = command.key;

    if (key == "TIM:RES") {
        if (!access.command_only()) return true;
        state_.timer_seconds = 0.0;
    } else if (key == "TIM:RES:TIME") {
        if (!access.query_only()) return true;
        access.respond(format_setting(state_.timer_seconds));
    } else if (key == "TIM:RES:AUTO") {
        // The manual marks this one "n. i." (not implemented) on the
        // instrument, so the simulator rejects it the same way.
        access.fail(error_code::kUndefinedHeader, "Undefined header;TIMer:RESet:AUTO");
    } else {
        return false;
    }
    return true;
}

} // namespace fluke::norma::sim

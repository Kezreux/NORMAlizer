// The simulator's CALCulate, TRACe and STATus handlers.
//
// CALCulate and TRACe are modelled as state plus synthesized data: the spectrum
// and the recorded trace are generated from the signal model so a client's
// parsing of a long, multi-point response is exercised without needing the real
// instrument's memory.

#include <algorithm>
#include <cmath>

#include "simulator_internal.hpp"

namespace fluke::norma::sim {

namespace {

using detail::format_setting;
using detail::join_values;
using detail::upper;

/// Number of spectrum lines the simulated FFT produces.
constexpr int kSpectrumLines = 32;

/// Applies the <count>,<offset>,<sparsing> window of the data queries to a
/// generated series, the way the instrument serves a slice of its memory.
std::vector<double> apply_window(const std::vector<double>& source, int count, int offset,
                                 int sparsing) {
    std::vector<double> out;
    const int step = std::max(sparsing, 1);
    const int first = std::max(offset, 0);
    for (int i = first; i < static_cast<int>(source.size()); i += step) {
        if (count > 0 && static_cast<int>(out.size()) >= count) {
            break;
        }
        out.push_back(source[static_cast<std::size_t>(i)]);
    }
    return out;
}

/// Reads the optional trailing "<count>[,<offset>[,<sparsing>]]" of a data
/// query. Returns false after queuing an error for an unparsable argument.
bool read_window(Access& access, std::size_t first_argument, int& count, int& offset,
                 int& sparsing) {
    const auto& args = access.command.args;
    int* targets[] = {&count, &offset, &sparsing};
    for (std::size_t i = 0; i < 3; ++i) {
        const std::size_t index = first_argument + i;
        if (index >= args.size() || args[index].empty()) {
            break;
        }
        double value = 0.0;
        if (!detail::parse_number(args[index], value) || value < 0) {
            access.fail(error_code::kIllegalParameterValue, "Illegal parameter value;" + args[index]);
            return false;
        }
        *targets[i] = static_cast<int>(std::lround(value));
    }
    return true;
}

} // namespace

// -- CALCulate ----------------------------------------------------------------------

bool NormaSimulator::handle_calculate(Command& command) {
    Access access{*this, command};
    const std::string& key = command.key;
    if (key.compare(0, 5, "CALC:") != 0) {
        return false;
    }

    if (key == "CALC:TRAN:FREQ:STAT") {
        // The only documented parameter is ONCE: compute the spectrum now.
        if (command.query) {
            access.fail(error_code::kUndefinedHeader, "Undefined header;" + key);
            return true;
        }
        auto text = access.parameter();
        if (!text) {
            return true;
        }
        if (!detail::mnemonic_matches(upper(*text), "ONCE")) {
            access.fail(error_code::kIllegalParameterValue, "Illegal parameter value;" + *text);
        }
    } else if (key == "CALC:TRAN:FREQ:MODE") {
        access.keyword<TransformMode>(state_.transform_mode, {{TransformMode::Fft, {"FFT"}},
                                                             {TransformMode::Dft, {"DFT"}},
                                                             {TransformMode::Std, {"STD"}}});
    } else if (key == "CALC:TRAN:FREQ:FUNC") {
        access.function_list(state_.transform_functions);
    } else if (key == "CALC:TRAN:FREQ:STAR") {
        access.number(state_.transform_start, 0.0, 1.0e6);
    } else if (key == "CALC:TRAN:FREQ:STOP") {
        access.number(state_.transform_stop, 0.0, 1.0e6);
    } else if (key == "CALC:TRAN:FREQ:CYCL") {
        if (command.query) {
            access.respond(std::to_string(state_.transform_cycles));
            return true;
        }
        int cycles = state_.transform_cycles;
        access.integer(cycles, 4, 12);
        if (command.failed) {
            return true;
        }
        if (cycles != 4 && cycles != 6 && cycles != 8 && cycles != 10 && cycles != 12) {
            access.fail(error_code::kIllegalParameterValue,
                        "Illegal parameter value;" + std::to_string(cycles));
            return true;
        }
        state_.transform_cycles = cycles;
    } else if (key == "CALC:TRAN:FREQ:GRO") {
        access.keyword<HarmonicGrouping>(
            state_.transform_grouping, {{HarmonicGrouping::Component, {"COMP", "COMPonent"}},
                                        {HarmonicGrouping::Harmonic, {"HARM", "HARMonic"}},
                                        {HarmonicGrouping::HGroup, {"HGR", "HGRoup"}},
                                        {HarmonicGrouping::HSGroup, {"HSGR", "HSGRoup"}},
                                        {HarmonicGrouping::ISGroup, {"ISGR", "ISGRoup"}},
                                        {HarmonicGrouping::SGroup, {"SGR", "SGRoup"}}});
    } else if (key == "CALC:DATA") {
        if (!access.query_only()) return true;
        int count = 0;
        int offset = 0;
        int sparsing = 0;
        if (!read_window(access, 0, count, offset, sparsing)) {
            return true;
        }
        // A decaying harmonic series: line 1 carries the fundamental, and each
        // further line a fraction of it.
        const double fundamental = signal_.present ? signal_.phases[0].voltage_rms : 0.0;
        std::vector<double> spectrum;
        spectrum.reserve(kSpectrumLines);
        for (int line = 1; line <= kSpectrumLines; ++line) {
            spectrum.push_back(fundamental / static_cast<double>(line * line));
        }
        std::vector<double> window = apply_window(spectrum, count, offset, sparsing);
        std::string out;
        for (std::size_t i = 0; i < window.size(); ++i) {
            if (i != 0) {
                out += ',';
            }
            out += format_value(window[i]);
        }
        access.respond(out);
    } else if (key == "CALC:DATA:PRE") {
        if (!access.query_only()) return true;
        // <count>,<function count>,<interval>,<start frequency>
        const double interval = signal_.frequency;
        access.respond(std::to_string(kSpectrumLines) + "," +
                       std::to_string(std::max<std::size_t>(state_.transform_functions.size(), 1)) +
                       "," + format_setting(interval) + "," +
                       format_setting(state_.transform_start));
    } else if (key == "CALC:DATA:THD") {
        if (!access.query_only()) return true;
        if (state_.transform_mode != TransformMode::Std) {
            // The manual marks CALC:DATA:THD? as STD-mode only.
            access.fail(error_code::kSettingsConflict,
                        "Settings conflict;THD requires TRANsform:FREQuency:MODE STD");
            return true;
        }
        std::vector<double> thd;
        for (int phase = 1; phase <= 3; ++phase) {
            thd.push_back(signal_.phases[static_cast<std::size_t>(phase - 1)].voltage_thd);
        }
        access.respond(join_values(thd));
    } else if (key == "CALC:INT:STAT") {
        access.boolean(state_.integral_enabled);
    } else if (key == "CALC:INT:FUNC") {
        access.function_list(state_.integral_functions);
    } else if (key == "CALC:INT:CLE") {
        if (!access.command_only()) return true;
        state_.timer_seconds = 0.0;
    } else if (key == "CALC:INT:CLE:AUTO") {
        access.boolean(state_.integral_auto_clear);
    } else if (key == "CALC:INT:STAR:SOUR") {
        access.keyword<IntegralStartSource>(state_.integral_start_source,
                                            {{IntegralStartSource::Command, {"CMD"}},
                                             {IntegralStartSource::Time, {"TIME"}},
                                             {IntegralStartSource::Manual, {"MAN", "MANual"}}});
    } else if (key == "CALC:INT:STOP:SOUR") {
        access.keyword<IntegralStopSource>(
            state_.integral_stop_source, {{IntegralStopSource::Command, {"CMD"}},
                                          {IntegralStopSource::Time, {"TIME"}},
                                          {IntegralStopSource::Manual, {"MAN", "MANual"}},
                                          {IntegralStopSource::TimeInterval, {"TINT", "TINTerval"}}});
    } else if (key == "CALC:INT:STAR") {
        if (!access.command_only()) return true;
        if (!state_.integral_enabled) {
            access.fail(error_code::kSettingsConflict,
                        "Settings conflict;CALCulate:INTegral:STATe is OFF");
            return true;
        }
        state_.integral_running = true;
    } else if (key == "CALC:INT:STOP") {
        if (!access.command_only()) return true;
        state_.integral_running = false;
    } else if (key == "CALC:INT:STAR:TIME" || key == "CALC:INT:STOP:TIME") {
        if (!access.command_only()) return true;
        if (command.args.size() < 6) {
            access.fail(error_code::kMissingParameter,
                        "Missing parameter;expected yyyy,MM,dd,hh,mm,ss");
        }
    } else if (key == "CALC:INT:STOP:TINT") {
        access.number(state_.integral_stop_interval, 1.0e-3, 9.99e6);
    } else if (key == "CALC:HARM:ORD") {
        access.integer(state_.harmonic_order, 0, 100);
    } else if (key == "CALC:POW:CORR") {
        access.keyword<PowerCorrection>(state_.power_correction,
                                        {{PowerCorrection::Star, {"STAR"}},
                                         {PowerCorrection::Delta, {"DELT", "DELTa"}}});
    } else if (key == "CALC:POW#:EFF:REF") {
        const int suffix = command.aggregate_suffix();
        if (suffix != 0 && suffix != 460) {
            access.fail(error_code::kUndefinedHeader,
                        "Undefined header;CALCulate:POWer" + std::to_string(suffix));
            return true;
        }
        if (command.query) {
            access.respond("\"" + state_.efficiency_input + "\",\"" + state_.efficiency_output +
                           "\"");
            return true;
        }
        if (command.args.size() < 2) {
            access.fail(error_code::kMissingParameter,
                        "Missing parameter;expected <function1>,<function2>");
            return true;
        }
        state_.efficiency_input = upper(detail::unquote(command.args[0]));
        state_.efficiency_output = upper(detail::unquote(command.args[1]));
    } else {
        return false;
    }
    return true;
}

// -- TRACe ---------------------------------------------------------------------------

bool NormaSimulator::handle_trace(Command& command) {
    Access access{*this, command};
    const std::string& key = command.key;
    if (key.compare(0, 4, "TRAC") != 0) {
        return false;
    }

    if (key == "TRAC:FREE") {
        if (!access.query_only()) return true;
        access.respond(std::to_string(kTraceCapacity - trace_points()));
    } else if (key == "TRAC:CAT:LEN") {
        if (!access.query_only()) return true;
        access.respond(std::to_string(trace_points()));
    } else if (key == "TRAC:DEL:ALL") {
        if (!access.command_only()) return true;
        state_.sweep[1].enabled = false;
        state_.sweep[2].enabled = false;
    } else if (key == "TRAC:PRE" || key == "TRAC" || key == "TRAC:STAT") {
        if (!access.query_only()) return true;
        if (command.args.empty()) {
            access.fail(error_code::kMissingParameter, "Missing parameter;expected <block>");
            return true;
        }
        double block_number = 0.0;
        if (!detail::parse_number(command.args[0], block_number) ||
            (block_number != 1 && block_number != 2)) {
            access.fail(error_code::kIllegalParameterValue,
                        "Illegal parameter value;" + command.args[0]);
            return true;
        }
        const auto block = static_cast<std::size_t>(block_number);
        const auto& sweep = state_.sweep[block];

        if (key == "TRAC:PRE") {
            access.respond(std::to_string(trace_points()) + "," +
                           std::to_string(std::max<std::size_t>(sweep.functions.size(), 1)) + "," +
                           detail::format_setting(state_.aperture) + "," +
                           detail::format_setting(sweep.offset_time));
            return true;
        }

        int count = 0;
        int offset = 0;
        int sparsing = 0;
        if (!read_window(access, 1, count, offset, sparsing)) {
            return true;
        }
        // The recording holds one point per averaging cycle for each configured
        // function; the value walks from the measured value downwards so a
        // client can tell the points apart.
        const std::vector<std::string>& functions =
            sweep.functions.empty() ? state_.functions : sweep.functions;
        std::vector<double> values;
        std::vector<double> status;
        for (const std::string& function : functions) {
            const SimulatedMeasurement measurement = measure(function);
            for (int point = 0; point < trace_points(); ++point) {
                values.push_back(measurement.value *
                                 (1.0 - 0.001 * static_cast<double>(point)));
                status.push_back(static_cast<double>(measurement.status));
            }
        }

        const std::vector<double>& source = key == "TRAC" ? values : status;
        const std::vector<double> window = apply_window(source, count, offset, sparsing);
        if (key == "TRAC:STAT") {
            access.respond(join_values(window));
        } else {
            std::string out;
            for (std::size_t i = 0; i < window.size(); ++i) {
                if (i != 0) {
                    out += ',';
                }
                out += format_value(window[i]);
            }
            access.respond(out);
        }
    } else {
        return false;
    }
    return true;
}

// -- STATus ----------------------------------------------------------------------------

namespace {

/// Maps a STATus command key onto the register and the part it addresses.
bool status_target(const std::string& key, StatusRegister& reg, RegisterPart& part) {
    if (key.compare(0, 5, "STAT:") != 0) {
        return false;
    }
    // Keys are "<register>:<part>", where the register may itself be nested
    // ("QUES:VOLT"), so the part is whatever follows the last colon.
    const std::string rest = key.substr(5);
    const std::size_t last = rest.rfind(':');
    if (last == std::string::npos) {
        return false;
    }
    const std::string register_name = rest.substr(0, last);
    const std::string part_name = rest.substr(last + 1);

    if (register_name == "OPER") {
        reg = StatusRegister::Operation;
    } else if (register_name == "QUES") {
        reg = StatusRegister::Questionable;
    } else if (register_name == "QUES:VOLT") {
        reg = StatusRegister::QuestionableVoltage;
    } else if (register_name == "QUES:CURR") {
        reg = StatusRegister::QuestionableCurrent;
    } else {
        return false;
    }

    if (part_name == "COND") {
        part = RegisterPart::Condition;
    } else if (part_name == "EVEN") {
        part = RegisterPart::Event;
    } else if (part_name == "ENAB") {
        part = RegisterPart::Enable;
    } else if (part_name == "PTR") {
        part = RegisterPart::PositiveTransition;
    } else if (part_name == "NTR") {
        part = RegisterPart::NegativeTransition;
    } else {
        return false;
    }
    return true;
}

} // namespace

bool NormaSimulator::handle_status(Command& command) {
    Access access{*this, command};
    StatusRegister reg{};
    RegisterPart part{};
    if (!status_target(command.key, reg, part)) {
        return false;
    }

    SimulatorState::RegisterState& registers = register_state(reg);
    switch (part) {
    case RegisterPart::Condition:
        if (!access.query_only()) return true;
        access.respond(std::to_string(condition(reg)));
        break;
    case RegisterPart::Event:
        if (!access.query_only()) return true;
        access.respond(std::to_string(latch_event(reg)));
        registers.event = 0; // the EVENt part clears on read
        break;
    case RegisterPart::Enable:
        access.integer(registers.enable, 0, 65535);
        break;
    case RegisterPart::PositiveTransition:
        access.integer(registers.ptransition, 0, 65535);
        break;
    case RegisterPart::NegativeTransition:
        access.integer(registers.ntransition, 0, 65535);
        break;
    }
    return true;
}

} // namespace fluke::norma::sim

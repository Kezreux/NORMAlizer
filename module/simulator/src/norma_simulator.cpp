// SCPI command-line parsing and state handling for the offline NORMA.
//
// The parser mirrors the instrument's input unit as described in
// docs/Fluke-NORMA-TCP-API.md, "Structure of command lines":
//
//   * a command line holds one or more commands separated by ';';
//   * a command that does not start with ':' continues at the level below the
//     shared levels of the previous command (INP1:SHUN EXT;GAIN 25.0);
//   * every mnemonic may be given in its short or its long form, in any casing;
//   * optional nodes ([SENSe:], [:UPPer], [:STATe], ...) may be omitted;
//   * a header ending in '?' is a query, and the responses of a line's queries
//     are returned in order, separated by ';'.
//
// A header that matches no known command is rejected into the error queue as
// -113 "Undefined header", exactly as the firmware would, so that a client's
// error handling is genuinely exercised.

#include "simulator_internal.hpp"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <initializer_list>
#include <map>
#include <utility>

namespace fluke::norma::sim {

using detail::mnemonic_matches;
using detail::split_unquoted;
using detail::trim;
using detail::unquote;
using detail::upper;

namespace {

// -- Command patterns ---------------------------------------------------------

/// One node of a pattern: the mnemonics it accepts, whether it may be omitted
/// and whether it may carry a numeric suffix.
struct PatternNode {
    std::vector<std::string> mnemonics;
    bool optional = false;
    bool suffixed = false;
};

/// Compiles a pattern such as "[SENSe]:VOLTage#:[AC|DC]:RANGe:[UPPer]".
std::vector<PatternNode> compile_pattern(std::string_view pattern) {
    std::vector<PatternNode> nodes;
    for (const std::string& raw : split_unquoted(pattern, ':')) {
        std::string_view node = trim(raw);
        PatternNode compiled;
        if (!node.empty() && node.front() == '[' && node.back() == ']') {
            compiled.optional = true;
            node = node.substr(1, node.size() - 2);
        }
        for (std::string alternative : split_unquoted(node, '|')) {
            if (!alternative.empty() && alternative.back() == '#') {
                compiled.suffixed = true;
                alternative.pop_back();
            }
            compiled.mnemonics.push_back(std::move(alternative));
        }
        nodes.push_back(std::move(compiled));
    }
    return nodes;
}

bool node_accepts(const PatternNode& node, const Token& token) {
    if (token.has_suffix && !node.suffixed) {
        return false;
    }
    for (const std::string& mnemonic : node.mnemonics) {
        if (mnemonic_matches(token.text, mnemonic)) {
            return true;
        }
    }
    return false;
}

/// Matches `tokens` against `pattern` from the given positions, collecting the
/// suffixes of the nodes that were actually present. Optional nodes are tried
/// present-first so the longest interpretation wins.
bool match_pattern(const std::vector<PatternNode>& pattern, std::size_t pi,
                   const std::vector<Token>& tokens, std::size_t ti,
                   std::vector<int>& suffixes) {
    if (pi == pattern.size()) {
        return ti == tokens.size();
    }
    const PatternNode& node = pattern[pi];
    if (ti < tokens.size() && node_accepts(node, tokens[ti])) {
        const std::size_t depth = suffixes.size();
        if (node.suffixed) {
            suffixes.push_back(tokens[ti].suffix);
        }
        if (match_pattern(pattern, pi + 1, tokens, ti + 1, suffixes)) {
            return true;
        }
        suffixes.resize(depth);
    }
    if (node.optional) {
        return match_pattern(pattern, pi + 1, tokens, ti, suffixes);
    }
    return false;
}

/// A command the simulator knows: a stable key to dispatch on, and the pattern
/// of headers that select it.
struct CommandSpec {
    const char* key;
    const char* pattern;
};

/// Every command the simulator implements. The key is the canonical short form
/// and is what the handlers switch on; '#' marks a node carrying a suffix.
const CommandSpec kCommandSpecs[] = {
    // ABORt / ROUTe
    {"ABOR", "ABORt"},
    {"ROUT:SYST", "ROUTe:SYSTem"},

    // INPut
    {"INP#:COUP", "INPut#:COUPling"},
    {"INP#:GAIN", "INPut#:GAIN"},
    {"INP#:FILT:FREQ", "INPut#:FILTer:[LPASs]:FREQuency"},
    {"INP#:FILT:STAT", "INPut#:FILTer:[STATe]"},
    {"INP#:SHUN", "INPut#:SHUNt"},

    // SENSe: ranging and scaling
    {"VOLT#:RANG:LIST", "[SENSe]:VOLTage#:[AC|DC]:RANGe:[UPPer]:LIST"},
    {"VOLT#:RANG:AUTO", "[SENSe]:VOLTage#:[AC|DC]:RANGe:[UPPer]:AUTO"},
    {"VOLT#:RANG", "[SENSe]:VOLTage#:[AC|DC]:RANGe:[UPPer]"},
    {"VOLT#:SCAL", "[SENSe]:VOLTage#:[AC|DC]:SCALe"},
    {"CURR#:RANG:LIST", "[SENSe]:CURRent#:[AC|DC]:RANGe:[UPPer]:LIST"},
    {"CURR#:RANG:AUTO", "[SENSe]:CURRent#:[AC|DC]:RANGe:[UPPer]:AUTO"},
    {"CURR#:RANG", "[SENSe]:CURRent#:[AC|DC]:RANGe:[UPPer]"},
    {"CURR#:SCAL", "[SENSe]:CURRent#:[AC|DC]:SCALe"},
    {"APER", "[SENSe]:[VOLTage#|CURRent#|POWer#]:[AC|DC]:APERture"},
    {"SWE:FREQ", "[SENSe]:SWEep:FREQuency"},

    // SENSe: measurement functions and data
    {"FUNC:OFF:ALL", "[SENSe]:FUNCtion:OFF:ALL"},
    {"FUNC:CONC", "[SENSe]:FUNCtion:CONCurrent"},
    {"FUNC:COUN", "[SENSe]:FUNCtion:[ON]:COUNt"},
    {"FUNC:ALL", "[SENSe]:FUNCtion:[ON]:ALL"},
    {"FUNC", "[SENSe]:FUNCtion:[ON]"},
    {"DATA:STAT", "[SENSe]:DATA:STATus"},
    {"DATA", "[SENSe]:DATA"},

    // SENSe: memory recording
    {"SWE#:OFFS:POINTS", "[SENSe]:SWEep#:OFFSet:POINTS"},
    {"SWE#:OFFS:TIME", "[SENSe]:SWEep#:OFFSet:TIME"},
    {"SWE#:POINTS", "[SENSe]:SWEep#:POINTS"},
    {"SWE#:TIME", "[SENSe]:SWEep#:TIME"},
    {"SWE#:COUN", "[SENSe]:SWEep#:COUNt"},
    {"SWE#:SFAC", "[SENSe]:SWEep#:SFACtor"},
    {"SWE#:FUNC", "[SENSe]:SWEep#:FUNCtion"},
    {"SWE#:STAT", "[SENSe]:SWEep#:[STATe]"},

    // SYNC
    {"SYNC:STAT", "SYNC:STATe"},
    {"SYNC:LEV:UNIT", "SYNC:LEVel:UNIT"},
    {"SYNC:TIM", "SYNC:TIMeout"},
    {"SYNC:LEV", "SYNC:[SOURce|VOLTage#|CURRent#]:LEVel"},
    {"SYNC:SLOP", "SYNC:[SOURce|VOLTage#|CURRent#]:SLOPe"},
    {"SYNC:FILT:FREQ", "SYNC:[SOURce|VOLTage#|CURRent#]:FILTer:[LPASs]:FREQuency"},
    {"SYNC:FILT:STAT", "SYNC:[SOURce|VOLTage#|CURRent#]:FILTer:[LPASs]:[STATe]"},
    {"SYNC:SOUR", "SYNC:[SOURce]"},

    // INITiate
    {"INIT:CONT", "INITiate:CONTinuous"},
    {"INIT:SEQ#", "INITiate:[IMMediate]:SEQuence#"},
    {"INIT:NAME", "INITiate:[IMMediate]:NAME"},
    {"INIT", "INITiate:[IMMediate]"},

    // TRIGger
    {"TRIG:STAR:SOUR", "TRIGger:STARt:SOURce"},
    {"TRIG:STAR:TIME", "TRIGger:STARt:TIME"},
    {"TRIG:STAR:LEV", "TRIGger:STARt:LEVel"},
    {"TRIG:STAR:SLOP", "TRIGger:STARt:SLOPe"},
    {"TRIG:STOP:SOUR", "TRIGger:STOP:SOURce"},
    {"TRIG:STOP:TIME", "TRIGger:STOP:TIME"},
    {"TRIG:STOP:LEV", "TRIGger:STOP:LEVel"},
    {"TRIG:STOP:SLOP", "TRIGger:STOP:SLOPe"},

    // CALCulate: spectrum
    {"CALC:TRAN:FREQ:MODE", "CALCulate:TRANsform:FREQuency:MODE"},
    {"CALC:TRAN:FREQ:FUNC", "CALCulate:TRANsform:FREQuency:FUNCtion"},
    {"CALC:TRAN:FREQ:STAR", "CALCulate:TRANsform:FREQuency:STARt"},
    {"CALC:TRAN:FREQ:STOP", "CALCulate:TRANsform:FREQuency:STOP"},
    {"CALC:TRAN:FREQ:CYCL", "CALCulate:TRANsform:FREQuency:CYCLes"},
    {"CALC:TRAN:FREQ:GRO", "CALCulate:TRANsform:FREQuency:GROuping"},
    {"CALC:TRAN:FREQ:STAT", "CALCulate:TRANsform:FREQuency:[STATe]"},
    {"CALC:DATA:PRE", "CALCulate:DATA:PREamble"},
    {"CALC:DATA:THD", "CALCulate:DATA:THD"},
    {"CALC:DATA", "CALCulate:DATA"},

    // CALCulate: integration
    {"CALC:INT:CLE:AUTO", "CALCulate:INTegral:CLEar:AUTO"},
    {"CALC:INT:CLE", "CALCulate:INTegral:CLEar:[IMMediate]"},
    {"CALC:INT:FUNC", "CALCulate:INTegral:FUNCtion"},
    {"CALC:INT:STAR:SOUR", "CALCulate:INTegral:STARt:SOURce"},
    {"CALC:INT:STAR:TIME", "CALCulate:INTegral:STARt:TIME"},
    {"CALC:INT:STAR", "CALCulate:INTegral:STARt:[IMMediate]"},
    {"CALC:INT:STOP:SOUR", "CALCulate:INTegral:STOP:SOURce"},
    {"CALC:INT:STOP:TIME", "CALCulate:INTegral:STOP:TIME"},
    {"CALC:INT:STOP:TINT", "CALCulate:INTegral:STOP:TINTerval"},
    {"CALC:INT:STOP", "CALCulate:INTegral:STOP:[IMMediate]"},
    {"CALC:INT:STAT", "CALCulate:INTegral:[STATe]"},

    // CALCulate: power
    {"CALC:HARM:ORD", "CALCulate:HARMonic:ORDer"},
    {"CALC:POW:CORR", "CALCulate:POWer:CORRected"},
    {"CALC:POW#:EFF:REF", "CALCulate:POWer#:EFFiciency:REFerence"},

    // TRACe
    {"TRAC:PRE", "TRACe:[DATA]:PREamble"},
    {"TRAC:STAT", "TRACe:[DATA]:STATus"},
    {"TRAC:FREE", "TRACe:FREE"},
    {"TRAC:CAT:LEN", "TRACe:CATalog:LENgth"},
    {"TRAC:DEL:ALL", "TRACe:DELete:ALL"},
    {"TRAC", "TRACe:[DATA]"},

    // FORMat
    {"FORM:STAT", "FORMat:[DATA]:STATus"},
    {"FORM:BORD", "FORMat:BORDer"},
    {"FORM:TRAN", "FORMat:TRANspose"},
    {"FORM", "FORMat:[DATA]"},

    // DISPlay / OUTPut
    {"DISP:USER:FUNC", "DISPlay:USER:FUNCtion"},
    {"DISP:STAT", "DISPlay:[WINDow]:[STATe]"},
    {"OUTP#:STAT", "OUTPut#:[STATe]"},

    // SYSTem
    {"SYST:COMM:GPIB:ADDR", "SYSTem:COMMunicate:GPIB:[SELF]:ADDRess"},
    {"SYST:COMM:SER:BAUD", "SYSTem:COMMunicate:SERial:BAUD"},
    {"SYST:DATE", "SYSTem:DATE"},
    {"SYST:TIME", "SYSTem:TIME"},
    {"SYST:ERR:ALL", "SYSTem:ERRor:ALL"},
    {"SYST:ERR", "SYSTem:ERRor:[NEXT]"},
    {"SYST:KLOC", "SYSTem:KLOCk"},
    {"SYST:LANG", "SYSTem:LANGuage"},
    {"SYST:VERS", "SYSTem:VERSion"},

    // STATus
    {"STAT:OPER:COND", "STATus:OPERation:CONDition"},
    {"STAT:OPER:ENAB", "STATus:OPERation:ENABle"},
    {"STAT:OPER:PTR", "STATus:OPERation:PTRansition"},
    {"STAT:OPER:NTR", "STATus:OPERation:NTRansition"},
    {"STAT:OPER:EVEN", "STATus:OPERation:[EVENt]"},
    {"STAT:QUES:VOLT:COND", "STATus:QUEStionable:VOLTage:CONDition"},
    {"STAT:QUES:VOLT:ENAB", "STATus:QUEStionable:VOLTage:ENABle"},
    {"STAT:QUES:VOLT:PTR", "STATus:QUEStionable:VOLTage:PTRansition"},
    {"STAT:QUES:VOLT:NTR", "STATus:QUEStionable:VOLTage:NTRansition"},
    {"STAT:QUES:VOLT:EVEN", "STATus:QUEStionable:VOLTage:[EVENt]"},
    {"STAT:QUES:CURR:COND", "STATus:QUEStionable:CURRent:CONDition"},
    {"STAT:QUES:CURR:ENAB", "STATus:QUEStionable:CURRent:ENABle"},
    {"STAT:QUES:CURR:PTR", "STATus:QUEStionable:CURRent:PTRansition"},
    {"STAT:QUES:CURR:NTR", "STATus:QUEStionable:CURRent:NTRansition"},
    {"STAT:QUES:CURR:EVEN", "STATus:QUEStionable:CURRent:[EVENt]"},
    {"STAT:QUES:COND", "STATus:QUEStionable:CONDition"},
    {"STAT:QUES:ENAB", "STATus:QUEStionable:ENABle"},
    {"STAT:QUES:PTR", "STATus:QUEStionable:PTRansition"},
    {"STAT:QUES:NTR", "STATus:QUEStionable:NTRansition"},
    {"STAT:QUES:EVEN", "STATus:QUEStionable:[EVENt]"},

    // TIMer
    {"TIM:RES:AUTO", "TIMer:RESet:AUTO"},
    {"TIM:RES:TIME", "TIMer:RESet:TIME"},
    {"TIM:RES", "TIMer:RESet"},
};

/// The compiled patterns, built once.
const std::vector<std::pair<std::string, std::vector<PatternNode>>>& compiled_specs() {
    static const std::vector<std::pair<std::string, std::vector<PatternNode>>> specs = [] {
        std::vector<std::pair<std::string, std::vector<PatternNode>>> out;
        out.reserve(std::size(kCommandSpecs));
        for (const CommandSpec& spec : kCommandSpecs) {
            out.emplace_back(spec.key, compile_pattern(spec.pattern));
        }
        return out;
    }();
    return specs;
}

} // namespace

// -- Construction and reset -----------------------------------------------------

NormaSimulator::NormaSimulator(SimulatorOptions options) : options_(std::move(options)) {
    reset();
}

void NormaSimulator::reset() {
    SimulatorState fresh;
    // *RST defaults documented per command in the manual.
    for (int channel = 0; channel <= kMaxInputChannel; ++channel) {
        fresh.coupling[channel] = Coupling::DC;
        fresh.shunt[channel] = Shunt::Internal;
        fresh.gain[channel] = 1.0;
        fresh.filter[channel] = true;
    }
    for (int phase = 0; phase <= kMaxPhase; ++phase) {
        fresh.voltage[phase] = ChannelConfig{options_.voltage_ranges.back(), true, 1.0};
        fresh.current[phase] = ChannelConfig{options_.current_ranges.back(), true, 1.0};
    }
    // *RST leaves the error queue alone; *CLS is what clears it.
    fresh.errors = std::move(state_.errors);
    state_ = std::move(fresh);
}

void NormaSimulator::clear_status() {
    state_.errors.clear();
    state_.event_status = 0;
    state_.operation.event = 0;
    state_.questionable.event = 0;
    state_.questionable_voltage.event = 0;
    state_.questionable_current.event = 0;
}

void NormaSimulator::push_error(int code, std::string message) {
    // The manual's queue is finite; a real instrument reports -350 "Queue
    // overflow" once it is full.
    if (state_.errors.size() >= 32) {
        state_.errors.back() = ScpiErrorInfo{-350, "Queue overflow"};
        return;
    }
    state_.errors.push_back(ScpiErrorInfo{code, std::move(message)});
    state_.event_status |= 1 << 5; // Command Error bit of the ESR
}

// -- Command line handling --------------------------------------------------------

std::optional<std::string> NormaSimulator::handle_line(std::string_view line) {
    ++handled_lines_;
    std::vector<std::string> responses;
    std::vector<Token> path; // shared path carried across ';'-separated commands

    for (const std::string& piece : split_unquoted(line, ';')) {
        std::string_view text = trim(piece);
        if (text.empty()) {
            continue;
        }

        Command command;

        // Separate the header from its parameters at the first white space
        // outside a quoted string.
        std::size_t split = std::string_view::npos;
        char quote = 0;
        for (std::size_t i = 0; i < text.size(); ++i) {
            const char c = text[i];
            if (quote != 0) {
                if (c == quote) quote = 0;
            } else if (c == '"' || c == '\'') {
                quote = c;
            } else if (std::isspace(static_cast<unsigned char>(c))) {
                split = i;
                break;
            }
        }
        std::string_view header = split == std::string_view::npos ? text : text.substr(0, split);
        const std::string_view parameters =
            split == std::string_view::npos ? std::string_view{} : trim(text.substr(split + 1));

        if (!header.empty() && header.back() == '?') {
            command.query = true;
            header.remove_suffix(1);
        }
        if (!parameters.empty()) {
            for (const std::string& argument : split_unquoted(parameters, ',')) {
                command.args.emplace_back(trim(argument));
            }
        }

        if (!header.empty() && header.front() == '*') {
            // Common commands never change the current path.
            command.key = upper(header);
            command.tokens.push_back(Token{command.key, 0, false});
            if (!handle_common(command)) {
                push_error(error_code::kUndefinedHeader, "Undefined header;" + std::string(text));
                break;
            }
        } else {
            const bool absolute = !header.empty() && header.front() == ':';
            if (absolute) {
                header.remove_prefix(1);
                path.clear();
            }

            std::vector<Token> tokens;
            if (!absolute && !path.empty()) {
                tokens = path;
            }
            for (const std::string& node : split_unquoted(header, ':')) {
                tokens.push_back(make_token(node));
            }
            command.tokens = tokens;

            // The next command in the line continues one level above this one.
            path = tokens;
            if (!path.empty()) {
                path.pop_back();
            }

            bool matched = false;
            for (const auto& [key, pattern] : compiled_specs()) {
                std::vector<int> suffixes;
                if (match_pattern(pattern, 0, command.tokens, 0, suffixes)) {
                    command.key = key;
                    command.suffixes = std::move(suffixes);
                    matched = true;
                    break;
                }
            }
            if (!matched) {
                push_error(error_code::kUndefinedHeader, "Undefined header;" + std::string(text));
                break;
            }
            execute(command);
        }

        if (command.response.has_value()) {
            responses.push_back(*command.response);
        }
        if (command.failed) {
            break;
        }
    }

    if (responses.empty()) {
        return std::nullopt;
    }
    return join_responses(responses);
}

std::string NormaSimulator::join_responses(const std::vector<std::string>& responses) {
    // "With several queries in the same command line, the responses are
    // returned in the same order as the queries, separated by semicolons."
    std::string out;
    for (std::size_t i = 0; i < responses.size(); ++i) {
        if (i != 0) {
            out += ';';
        }
        out += responses[i];
    }
    return out;
}

std::optional<std::string> NormaSimulator::execute(Command& command) {
    const bool handled = handle_route(command) || handle_input(command) || handle_sense(command) ||
                         handle_sync(command) || handle_acquisition(command) ||
                         handle_trigger(command) || handle_calculate(command) ||
                         handle_trace(command) || handle_format(command) ||
                         handle_display_output(command) || handle_system(command) ||
                         handle_status(command) || handle_timer(command);
    if (!handled) {
        // A key in the spec table with no handler is a simulator bug, not a
        // client error; report it as an execution error so a test notices.
        push_error(error_code::kExecutionError,
                   "Simulator has no handler for command;" + command.key);
        command.failed = true;
    }
    return command.response;
}

} // namespace fluke::norma::sim

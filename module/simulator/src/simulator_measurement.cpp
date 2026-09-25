// The simulator's signal model: what DATA? and DATA:STATus? report, and the
// live CONDition parts of the status registers.
//
// Values are computed from SimulatedSignal so a test can set up a signal and
// then assert on real numbers rather than on hard-coded response strings. The
// status bits follow from the configuration exactly as the manual describes
// them: a value too large for the selected range is Overrange, one far below it
// Underrange, a missing synchronization signal makes everything Undefined, and
// a function the instrument cannot provide is Not available.

#include <algorithm>
#include <cmath>
#include <cstdio>

#include "simulator_internal.hpp"

namespace fluke::norma::sim {

namespace {

using detail::upper;

/// A <function> name split into the parts the manual defines.
struct FunctionName {
    std::string quantity;              ///< first node without its suffix, e.g. "VOLT"
    int suffix = 0;                    ///< phase suffix, 0 = aggregate
    std::vector<std::string> tail;     ///< remaining nodes, e.g. {"ACT"} or {"REAC","HAR"}
};

FunctionName split_function(std::string_view function) {
    FunctionName parsed;
    const std::vector<std::string> nodes = detail::split_unquoted(function, ':');
    if (nodes.empty()) {
        return parsed;
    }
    const Token head = make_token(nodes.front());
    parsed.quantity = head.text;
    parsed.suffix = head.suffix;
    for (std::size_t i = 1; i < nodes.size(); ++i) {
        parsed.tail.push_back(upper(std::string(detail::trim(nodes[i]))));
    }
    return parsed;
}

bool tail_is(const std::vector<std::string>& tail, std::initializer_list<const char*> expected) {
    if (tail.size() != expected.size()) {
        return false;
    }
    std::size_t i = 0;
    for (const char* mnemonic : expected) {
        if (!detail::mnemonic_matches(tail[i++], mnemonic)) {
            return false;
        }
    }
    return true;
}

/// True when the tail ends in one of the accumulator nodes, which require the
/// matching CALCulate feature to be switched on.
bool ends_with(const std::vector<std::string>& tail, std::initializer_list<const char*> any) {
    if (tail.empty()) {
        return false;
    }
    for (const char* mnemonic : any) {
        if (detail::mnemonic_matches(tail.back(), mnemonic)) {
            return true;
        }
    }
    return false;
}

/// The phases a suffix aggregates over: a single phase, or 1..3 / 4..6 for the
/// aggregate forms.
std::vector<int> aggregated_phases(int suffix) {
    if (suffix >= 1 && suffix <= kMaxPhase) {
        return {suffix};
    }
    if (suffix == 0) {
        return {1, 2, 3};
    }
    if (suffix == 460) {
        return {4, 5, 6};
    }
    return {};
}

} // namespace

std::string NormaSimulator::format_value(double value) const {
    char buffer[64];
    if (std::isnan(value)) {
        // The manual: an Undefined or unavailable measurement is reported as
        // the SCPI NaN encoding +9.91E+37.
        std::snprintf(buffer, sizeof(buffer), "%+.5E", kScpiNan);
        return buffer;
    }
    const int length = state_.data_format.length;
    if (length > 0) {
        // "For non-zero lengths the values are formatted with the C format
        // string %+.(length-1)e."
        std::snprintf(buffer, sizeof(buffer), "%+.*E", length - 1, value);
    } else {
        std::snprintf(buffer, sizeof(buffer), "%+.5E", value);
    }
    return buffer;
}

int NormaSimulator::trace_points() const {
    const auto& sweep = state_.sweep[1].enabled ? state_.sweep[1] : state_.sweep[2];
    if (!state_.sweep[1].enabled && !state_.sweep[2].enabled) {
        return 0;
    }
    const double points = sweep.time / std::max(state_.aperture, kMinAperture);
    return std::min(static_cast<int>(points), kTraceCapacity);
}

SimulatedMeasurement NormaSimulator::measure(std::string_view function) const {
    SimulatedMeasurement result;
    const FunctionName parsed = split_function(function);
    const std::vector<int> phases = aggregated_phases(parsed.suffix);

    const auto unavailable = [&] {
        result.value = std::nan("");
        result.status = measurement_status::kNotAvailable;
        return result;
    };
    const auto undefined = [&] {
        result.value = std::nan("");
        result.status = measurement_status::kUndefined;
        return result;
    };

    // MINimum/MAXimum are marked unimplemented in the manual, and the integral
    // accumulators only work once CALCulate:INTegral is switched on.
    if (ends_with(parsed.tail, {"MINimum", "MAXimum"})) {
        return unavailable();
    }
    if (ends_with(parsed.tail, {"IPOSitive", "INEGative", "INTegral"}) &&
        !state_.integral_enabled) {
        return unavailable();
    }

    // Without a synchronization signal the instrument cannot compute anything.
    const bool synchronized = signal_.present && state_.sync_enabled;
    const bool measuring = state_.continuous || state_.initiated;
    if (!measuring) {
        return undefined();
    }

    // Instrument-wide quantities first: they carry no phase suffix.
    if (detail::mnemonic_matches(parsed.quantity, "FREQuency") && parsed.tail.empty()) {
        if (!synchronized) {
            return undefined();
        }
        result.value = signal_.frequency;
        return result;
    }
    if (detail::mnemonic_matches(parsed.quantity, "TIME")) {
        if (parsed.tail.empty()) {
            // The averaging interval is stretched to whole signal periods.
            result.value = state_.aperture;
            if (synchronized && signal_.frequency > 0.0) {
                const double period = 1.0 / signal_.frequency;
                result.value = std::ceil(state_.aperture / period) * period;
            }
            return result;
        }
        if (tail_is(parsed.tail, {"RELative"})) {
            result.value = state_.timer_seconds;
            return result;
        }
        return unavailable();
    }

    if (phases.empty()) {
        // A suffix the signal model does not cover (e.g. a phase-to-phase or
        // averaged phase-to-phase form).
        return unavailable();
    }
    if (!synchronized) {
        return undefined();
    }

    // Sum/average the requested phases.
    double voltage_rms = 0.0;
    double current_rms = 0.0;
    double voltage_dc = 0.0;
    double current_dc = 0.0;
    double active = 0.0;
    double apparent = 0.0;
    double reactive = 0.0;
    double voltage_thd = 0.0;
    double current_thd = 0.0;
    double worst_phase_shift = 0.0;
    for (int phase : phases) {
        const PhaseSignal& s = signal_.phases[static_cast<std::size_t>(phase - 1)];
        const double radians = s.phase_shift_deg * 3.14159265358979323846 / 180.0;
        voltage_rms += s.voltage_rms;
        current_rms += s.current_rms;
        voltage_dc += s.voltage_dc;
        current_dc += s.current_dc;
        apparent += s.voltage_rms * s.current_rms;
        active += s.voltage_rms * s.current_rms * std::cos(radians);
        reactive += s.voltage_rms * s.current_rms * std::sin(radians);
        voltage_thd += s.voltage_thd;
        current_thd += s.current_thd;
        if (std::fabs(s.phase_shift_deg) > std::fabs(worst_phase_shift)) {
            worst_phase_shift = s.phase_shift_deg;
        }
    }
    const auto count = static_cast<double>(phases.size());
    // Voltages and currents average over the phases; powers add up.
    const double mean_voltage = voltage_rms / count;
    const double mean_current = current_rms / count;

    // Scaling: a transducer ratio multiplies the measured quantity.
    const auto index = static_cast<std::size_t>(phases.front());
    const double voltage_scale = state_.voltage[index].scale;
    const double current_scale = state_.current[index].scale;

    bool is_voltage = false;
    bool is_current = false;
    double value = 0.0;

    // Peel the optional trailing nodes off the name so what remains is the
    // quantity itself, and remember how to apply them to the result.
    std::vector<std::string> tail = parsed.tail;

    // IPOSitive/INEGative/INTegral sum the quantity over time. The simulator has
    // no clock, so it reports the instantaneous value (with the sign filter the
    // node asks for); what a test needs from it is the status path, which is
    // Not available until CALCulate:INTegral is switched on.
    enum class Accumulator { None, Positive, Negative, Both };
    Accumulator accumulator = Accumulator::None;
    if (!tail.empty()) {
        if (detail::mnemonic_matches(tail.back(), "IPOSitive")) {
            accumulator = Accumulator::Positive;
        } else if (detail::mnemonic_matches(tail.back(), "INEGative")) {
            accumulator = Accumulator::Negative;
        } else if (detail::mnemonic_matches(tail.back(), "INTegral")) {
            accumulator = Accumulator::Both;
        }
        if (accumulator != Accumulator::None) {
            tail.pop_back();
        }
    }

    // HAR selects the harmonic chosen with CALCulate:HARMonic:ORDer, which is a
    // fraction of the fundamental.
    double harmonic_factor = 1.0;
    if (!tail.empty() && detail::mnemonic_matches(tail.back(), "HAR")) {
        tail.pop_back();
        const double order = std::max(state_.harmonic_order, 1);
        harmonic_factor = 1.0 / (order * order);
    }

    if (detail::mnemonic_matches(parsed.quantity, "VOLTage")) {
        is_voltage = true;
        if (tail.empty() || tail_is(tail, {"DC"})) {
            value = mean_voltage * voltage_scale;
        } else if (tail_is(tail, {"AC"})) {
            const double dc = voltage_dc / count;
            const double ac_squared = mean_voltage * mean_voltage - dc * dc;
            value = std::sqrt(std::max(ac_squared, 0.0)) * voltage_scale;
        } else if (tail_is(tail, {"MEAN"})) {
            value = (voltage_dc / count) * voltage_scale;
        } else if (tail_is(tail, {"RMEAN"}) || tail_is(tail, {"RMCORR"})) {
            // Rectified mean of a sine is 2*sqrt(2)/pi of its RMS value.
            value = mean_voltage * 0.900316316 * voltage_scale;
        } else if (tail_is(tail, {"THD"})) {
            value = voltage_thd / count;
        } else if (tail_is(tail, {"CFACtor"})) {
            value = 1.41421356237; // crest factor of a sine
        } else if (tail_is(tail, {"FFACtor"})) {
            value = 1.11072073454; // form factor of a sine
        } else if (tail_is(tail, {"PTP"})) {
            value = 2.0 * 1.41421356237 * mean_voltage * voltage_scale;
        } else if (tail_is(tail, {"PHIGh"}) || tail_is(tail, {"PHIGH"})) {
            value = 1.41421356237 * mean_voltage * voltage_scale;
        } else if (tail_is(tail, {"PLOW"})) {
            value = -1.41421356237 * mean_voltage * voltage_scale;
        } else if (tail_is(tail, {"PHASe"})) {
            value = 0.0; // the voltage is the synchronization reference
        } else {
            return unavailable();
        }
    } else if (detail::mnemonic_matches(parsed.quantity, "CURRent")) {
        is_current = true;
        if (tail.empty() || tail_is(tail, {"DC"})) {
            value = mean_current * current_scale;
        } else if (tail_is(tail, {"AC"})) {
            const double dc = current_dc / count;
            const double ac_squared = mean_current * mean_current - dc * dc;
            value = std::sqrt(std::max(ac_squared, 0.0)) * current_scale;
        } else if (tail_is(tail, {"MEAN"})) {
            value = (current_dc / count) * current_scale;
        } else if (tail_is(tail, {"RMEAN"}) || tail_is(tail, {"RMCORR"})) {
            value = mean_current * 0.900316316 * current_scale;
        } else if (tail_is(tail, {"THD"})) {
            value = current_thd / count;
        } else if (tail_is(tail, {"CFACtor"})) {
            value = 1.41421356237;
        } else if (tail_is(tail, {"FFACtor"})) {
            value = 1.11072073454;
        } else if (tail_is(tail, {"PTP"})) {
            value = 2.0 * 1.41421356237 * mean_current * current_scale;
        } else if (tail_is(tail, {"PHIGH"})) {
            value = 1.41421356237 * mean_current * current_scale;
        } else if (tail_is(tail, {"PLOW"})) {
            value = -1.41421356237 * mean_current * current_scale;
        } else if (tail_is(tail, {"PHASe"})) {
            value = worst_phase_shift;
        } else {
            return unavailable();
        }
    } else if (detail::mnemonic_matches(parsed.quantity, "POWer")) {
        const double scale = voltage_scale * current_scale;
        if (tail.empty() || tail_is(tail, {"ACTive"})) {
            value = active * scale;
        } else if (tail_is(tail, {"APParent"})) {
            value = apparent * scale;
        } else if (tail_is(tail, {"REACtive"})) {
            value = reactive * scale;
        } else if (tail_is(tail, {"FACTor"})) {
            value = apparent != 0.0 ? active / apparent : std::nan("");
            if (worst_phase_shift < 0.0) {
                result.status |= measurement_status::kPowerFactorCapacitive;
            }
        } else if (tail_is(tail, {"CORRected"})) {
            value = active * scale;
        } else if (tail_is(tail, {"EFFiciency"})) {
            if (state_.efficiency_input.empty() || state_.efficiency_output.empty()) {
                return unavailable();
            }
            const double in = measure(state_.efficiency_input).value;
            const double out = measure(state_.efficiency_output).value;
            value = in != 0.0 ? out / in : std::nan("");
        } else {
            return unavailable();
        }
    } else if (detail::mnemonic_matches(parsed.quantity, "PHASe") && tail.empty()) {
        // The manual defines PHASe as arccos of the power factor.
        const double power_factor = apparent != 0.0 ? active / apparent : 1.0;
        value = std::acos(std::clamp(power_factor, -1.0, 1.0)) * 180.0 / 3.14159265358979323846;
        if (worst_phase_shift < 0.0) {
            value = -value;
        }
    } else if (detail::mnemonic_matches(parsed.quantity, "IMPedance") &&
               (tail.empty() || tail_is(tail, {"APParent"}))) {
        value = mean_current != 0.0 ? mean_voltage / mean_current : std::nan("");
    } else if (detail::mnemonic_matches(parsed.quantity, "RESistance")) {
        const double impedance = mean_current != 0.0 ? mean_voltage / mean_current : std::nan("");
        const double power_factor = apparent != 0.0 ? active / apparent : 1.0;
        if (tail_is(tail, {"SERial"})) {
            value = impedance * power_factor;
        } else if (tail_is(tail, {"PARallel"})) {
            value = power_factor != 0.0 ? impedance / power_factor : std::nan("");
        } else {
            return unavailable();
        }
    } else if (detail::mnemonic_matches(parsed.quantity, "REACTance")) {
        const double impedance = mean_current != 0.0 ? mean_voltage / mean_current : std::nan("");
        const double power_factor = apparent != 0.0 ? active / apparent : 1.0;
        const double sine = std::sqrt(std::max(1.0 - power_factor * power_factor, 0.0));
        if (tail_is(tail, {"SERial"})) {
            value = impedance * sine;
        } else if (tail_is(tail, {"PARallel"})) {
            value = sine != 0.0 ? impedance / sine : std::nan("");
        } else {
            return unavailable();
        }
    } else {
        return unavailable();
    }

    value *= harmonic_factor;
    switch (accumulator) {
    case Accumulator::Positive: value = std::max(value, 0.0); break;
    case Accumulator::Negative: value = std::min(value, 0.0); break;
    case Accumulator::None:
    case Accumulator::Both:
        break;
    }
    result.value = value;
    if (std::isnan(value)) {
        result.status |= measurement_status::kUndefined;
        return result;
    }

    // Range status: compare the raw (unscaled) input against the range in use.
    if (is_voltage || is_current) {
        const ChannelConfig& config =
            is_voltage ? state_.voltage[index] : state_.current[index];
        const double raw = is_voltage ? mean_voltage : mean_current;
        if (!config.autorange) {
            if (raw > config.range) {
                result.status |= measurement_status::kOverrange;
            } else if (raw < config.range * 0.05) {
                // Below 5 % of range the resolution is degraded; the manual
                // describes this as Underrange.
                result.status |= measurement_status::kUnderrange;
            }
        }
    }
    return result;
}

std::string NormaSimulator::measurement_response(const std::vector<std::string>& functions,
                                                 bool with_status) {
    if (functions.empty()) {
        // "*RST state: Empty list = no values defined."
        return {};
    }
    if (state_.data_format.format != DataFormat::Ascii) {
        // A real instrument would answer with a definite-length binary block.
        // Reproducing that shape is what lets a client notice that its ASCII
        // parser is talking to an instrument in REAL/INTeger mode.
        const std::string payload(functions.size() * sizeof(double), '\0');
        const std::string length = std::to_string(payload.size());
        return "#" + std::to_string(length.size()) + length + payload;
    }

    std::vector<SimulatedMeasurement> measurements;
    measurements.reserve(functions.size());
    for (const std::string& function : functions) {
        measurements.push_back(measure(function));
    }

    std::string out;
    for (std::size_t i = 0; i < measurements.size(); ++i) {
        if (i != 0) {
            out += ',';
        }
        out += format_value(measurements[i].value);
    }
    if (with_status) {
        // The manual appends all status values after all measurement values.
        for (const SimulatedMeasurement& measurement : measurements) {
            out += ',' + std::to_string(measurement.status);
        }
    }
    return out;
}

// -- Status registers ----------------------------------------------------------------

SimulatorState::RegisterState& NormaSimulator::register_state(StatusRegister reg) {
    switch (reg) {
    case StatusRegister::Questionable:        return state_.questionable;
    case StatusRegister::QuestionableVoltage: return state_.questionable_voltage;
    case StatusRegister::QuestionableCurrent: return state_.questionable_current;
    case StatusRegister::Operation:           break;
    }
    return state_.operation;
}

int NormaSimulator::condition(StatusRegister reg) const {
    const bool synchronized = signal_.present && state_.sync_enabled;

    // The per-channel registers hold six overrange bits followed by six
    // underrange bits; index 0..5 is phase 1..6 within that register.
    const auto channel_bits = [&](bool voltage) {
        int bits = 0;
        for (int phase = 1; phase <= kMaxPhase; ++phase) {
            const auto index = static_cast<std::size_t>(phase);
            const ChannelConfig& config = voltage ? state_.voltage[index] : state_.current[index];
            if (config.autorange) {
                continue;
            }
            const PhaseSignal& s = signal_.phases[index - 1];
            const double raw = voltage ? s.voltage_rms : s.current_rms;
            if (raw > config.range) {
                bits |= channel_status::overrange(phase - 1);
            } else if (raw < config.range * 0.05) {
                bits |= channel_status::underrange(phase - 1);
            }
        }
        return bits;
    };

    switch (reg) {
    case StatusRegister::Operation: {
        int bits = 0;
        if (synchronized) {
            bits |= operation_status::kSynchronized | operation_status::kSyncAvailable;
        }
        if (state_.continuous || state_.initiated) {
            bits |= operation_status::kAveraging;
        }
        if (state_.sweep[1].enabled || state_.sweep[2].enabled) {
            bits |= operation_status::kSweeping;
        }
        for (int phase = 1; phase <= kMaxPhase; ++phase) {
            const auto index = static_cast<std::size_t>(phase);
            if (state_.voltage[index].autorange || state_.current[index].autorange) {
                // Autorange is armed but settled; RANGing is only set while a
                // range change is actually in progress, which never happens in
                // the simulator's instantaneous model.
                break;
            }
        }
        return bits;
    }
    case StatusRegister::QuestionableVoltage:
        return channel_bits(true);
    case StatusRegister::QuestionableCurrent:
        return channel_bits(false);
    case StatusRegister::Questionable: {
        int bits = 0;
        if (!synchronized) {
            bits |= questionable_status::kFrequency;
        }
        if (channel_bits(true) != 0) {
            bits |= questionable_status::kVoltageSummary;
        }
        if (channel_bits(false) != 0) {
            bits |= questionable_status::kCurrentSummary;
        }
        return bits;
    }
    }
    return 0;
}

int NormaSimulator::latch_event(StatusRegister reg) {
    SimulatorState::RegisterState& registers = register_state(reg);
    const int current = condition(reg);
    const int rose = current & ~registers.previous_condition;
    const int fell = ~current & registers.previous_condition;
    registers.event |= (rose & registers.ptransition) | (fell & registers.ntransition);
    registers.previous_condition = current;
    return registers.event;
}

} // namespace fluke::norma::sim

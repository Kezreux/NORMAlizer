#pragma once

#include <cstdint>
#include <initializer_list>
#include <stdexcept>
#include <string>
#include <vector>

namespace fluke::norma {

/// The instrument's TCP port is fixed to 23 (the telnet port).
inline constexpr std::uint16_t kDefaultPort = 23;

/// SCPI's ASCII representation of "Not a Number" (returned e.g. when a
/// measurement is Undefined or Not available), see FORMat[:DATA] in the manual.
inline constexpr double kScpiNan = 9.91e37;

/// Parsing threshold for kScpiNan: the instrument may round the mantissa, so
/// anything within ~1 % below the nominal value is treated as the NaN encoding.
/// No real measurement of this instrument comes anywhere near 1e37.
inline constexpr double kScpiNanThreshold = kScpiNan * 0.999;

/// Lowest/highest averaging time accepted by APERture, in seconds.
inline constexpr double kMinAperture = 0.015;
inline constexpr double kMaxAperture = 3600.0;

/// Number of electrical input channels on the largest model (NORMA 5000).
inline constexpr int kMaxInputChannel = 12;

/// Highest phase index (L1..L6 on the NORMA 5000).
inline constexpr int kMaxPhase = 6;

// ---------------------------------------------------------------------------
// Enumerations mirroring the instrument's character-data parameters.
// ---------------------------------------------------------------------------

/// INPut<n>:COUPling
enum class Coupling { AC, DC };

/// INPut<n>:SHUNt
enum class Shunt { Internal, External };

/// ROUTe:SYSTem "3W" | "2W" ("2W" requires firmware >= 1.4)
enum class WiringSystem {
    ThreeWattmeter, ///< "3W"
    TwoWattmeter,   ///< "2W" (Aron)
};

/// SYNC:...:SLOPe
enum class Slope { Positive, Negative };

/// SYNC:LEVel:UNIT
enum class LevelUnit { Absolute, Percent };

/// FORMat[:DATA] — only Ascii can be parsed by this library (see
/// NormaInstrument::set_data_format).
enum class DataFormat { Ascii, Integer, Real };

/// FORMat:BORDer
enum class ByteOrder { Normal, Swapped };

/// SYSTem:KLOCk — front-panel key lock.
enum class KeyLock {
    Off,    ///< the [LOCAL] key returns the instrument to manual operation
    On,     ///< the [LOCAL] key is disabled
    Remote, ///< a command terminated with <LF> switches to remote control
};

/// CALCulate:TRANsform:FREQuency:MODE
enum class TransformMode { Fft, Dft, Std };

/// CALCulate:TRANsform:FREQuency:GROuping (STD mode, firmware >= 1.5)
enum class HarmonicGrouping { Component, Harmonic, HGroup, HSGroup, ISGroup, SGroup };

/// CALCulate:POWer:CORRected
enum class PowerCorrection { Star, Delta };

/// CALCulate:INTegral:STARt:SOURce
enum class IntegralStartSource { Command, Time, Manual };

/// CALCulate:INTegral:STOP:SOURce
enum class IntegralStopSource { Command, Time, Manual, TimeInterval };

/// The two memory-recording blocks addressed by SENSe:SWEep1|2 and TRACe.
enum class SweepBlock { Block1 = 1, Block2 = 2 };

/// Which STATus register a query addresses.
enum class StatusRegister {
    Operation,           ///< STATus:OPERation
    Questionable,        ///< STATus:QUEStionable
    QuestionableVoltage, ///< STATus:QUEStionable:VOLTage
    QuestionableCurrent, ///< STATus:QUEStionable:CURRent
};

/// Which part of a SCPI status register a query addresses.
enum class RegisterPart {
    Condition,          ///< :CONDition?      — the current state
    Event,              ///< [:EVENt]?        — latched since the last read
    Enable,             ///< :ENABle          — mask feeding the sum bit
    PositiveTransition, ///< :PTRansition     — 0 -> 1 latches into EVENt
    NegativeTransition, ///< :NTRansition     — 1 -> 0 latches into EVENt
};

// ---------------------------------------------------------------------------
// Value types
// ---------------------------------------------------------------------------

/// Parsed response of `*IDN?`, e.g. "Fluke,NORMA4000,KN34512BA,01.00".
struct Identification {
    std::string manufacturer;
    std::string model;
    std::string serial_number;
    std::string firmware_version;
};

/// One entry from the instrument error queue (`SYSTem:ERRor?`).
struct ScpiErrorInfo {
    int code = 0;
    std::string message;
};

/// FORMat[:DATA] response: the transfer format and its length in bits (ASCii:
/// mantissa digits, 0 = instrument's choice).
struct DataFormatSetting {
    DataFormat format = DataFormat::Ascii;
    int length = 6;
};

/// A calendar date as used by SYSTem:DATE and the TRIGger/CALCulate:INTegral
/// time sources.
struct Date {
    int year = 0;
    int month = 0;
    int day = 0;
};

/// A time of day as used by SYSTem:TIME and the TRIGger/CALCulate:INTegral
/// time sources.
struct Time {
    int hours = 0;
    int minutes = 0;
    int seconds = 0;
};

/// Bit flags of the per-measurement status returned by `DATA:STATus?`.
namespace measurement_status {
inline constexpr int kNormal = 0;
inline constexpr int kUnderrange = 1;
inline constexpr int kOverrange = 2;
inline constexpr int kUndefined = 8;
inline constexpr int kNotAvailable = 16;
inline constexpr int kPowerFactorCapacitive = 128;

/// The bits that say the accompanying value cannot be trusted.
inline constexpr int kInvalidMask = kUnderrange | kOverrange | kUndefined | kNotAvailable;
} // namespace measurement_status

/// Bits of the Status Byte (`*STB?`) and the Service Request Enable register
/// (`*SRE`), see manual table 2-1.
namespace status_byte {
inline constexpr int kErrorQueueNotEmpty = 1 << 2;
inline constexpr int kQuestionableSummary = 1 << 3;
inline constexpr int kMessageAvailable = 1 << 4;
inline constexpr int kEventStatusSummary = 1 << 5;
inline constexpr int kMasterStatusSummary = 1 << 6;
} // namespace status_byte

/// Bits of `STATus:OPERation`, see manual table 2-3.
namespace operation_status {
inline constexpr int kRanging = 1 << 2;      ///< autorange is changing range
inline constexpr int kSweeping = 1 << 3;     ///< memory recording in progress
inline constexpr int kWaitingForTrigger = 1 << 5;
inline constexpr int kSynchronized = 1 << 8; ///< locked to a valid SYNC source
inline constexpr int kSyncAvailable = 1 << 9;
inline constexpr int kAveraging = 1 << 10;   ///< averaging cycle in progress
inline constexpr int kCalculating = 1 << 12;
} // namespace operation_status

/// Bits of `STATus:QUEStionable`, see manual table 2-4.
namespace questionable_status {
inline constexpr int kVoltageSummary = 1 << 0;
inline constexpr int kCurrentSummary = 1 << 1;
inline constexpr int kFrequency = 1 << 5; ///< frequency measurement invalid
} // namespace questionable_status

/// Bits of `STATus:QUEStionable:VOLTage` and `:CURRent`, see manual tables 2-5
/// and 2-6. Both registers hold six overrange bits (0..5) followed by six
/// underrange bits (8..13); index 0..5 selects the channel within the register
/// (voltage: INPut 2,4,..,12; current: INPut 1,3,..,11).
namespace channel_status {
inline constexpr int overrange(int index) { return 1 << index; }
inline constexpr int underrange(int index) { return 1 << (index + 8); }
inline constexpr int kOverrangeMask = 0x003F;
inline constexpr int kUnderrangeMask = 0x3F00;
} // namespace channel_status

/// Result of `DATA:STATus?`: one status flag per measurement value.
struct Reading {
    std::vector<double> values;
    std::vector<int> status;

    /// True when the value at `index` carries none of the invalid status bits.
    bool is_valid(std::size_t index) const {
        return index < status.size() && (status[index] & measurement_status::kInvalidMask) == 0;
    }
};

/// Response of `CALCulate:DATA:PREamble?` / `TRACe:DATA:PREamble?`: the shape
/// of the data block that the matching data query returns.
struct DataPreamble {
    int count = 0;              ///< number of values per function
    int function_count = 0;     ///< number of functions in the block
    double interval = 0.0;      ///< spacing between values, in seconds or hertz
    double start = 0.0;         ///< value of the first point
    std::vector<std::string> functions; ///< function names, when reported
    std::string raw;            ///< the unparsed response, for anything else
};

// ---------------------------------------------------------------------------
// Helpers to build <function> names for SENSe:FUNCtion / DATA? queries.
//
// `phase` follows the manual's phase suffixes (see "Phase suffixes"):
//   0            no suffix — average/total of the 1st three-phase system
//   1..6         L1..L6 (4..6 on the NORMA 5000 only)
//   12,13,23,31  phase-to-phase within the 1st system (13 is 2W, 31 is 3W)
//   45,56,64     phase-to-phase within the 2nd system
//   123,456      average phase-to-phase voltage of the 1st/2nd system
//   460          average/total of the 2nd three-phase system
// ---------------------------------------------------------------------------
namespace fn {

/// Suffix accepted on a single-phase quantity (L1..L6).
inline bool is_phase(int phase) { return phase >= 1 && phase <= kMaxPhase; }

/// Suffix denoting a phase-to-phase voltage.
inline bool is_phase_to_phase(int phase) {
    switch (phase) {
    case 12: case 13: case 23: case 31: case 45: case 56: case 64:
        return true;
    default:
        return false;
    }
}

/// Suffix denoting an aggregate over a three-phase system (0 = 1st, 460 = 2nd).
inline bool is_aggregate(int phase) { return phase == 0 || phase == 460; }

/// Suffix denoting an average phase-to-phase voltage (123 = 1st, 456 = 2nd).
inline bool is_average_phase_to_phase(int phase) { return phase == 123 || phase == 456; }

/// Every suffix the manual defines, in any position.
inline bool is_valid_suffix(int phase) {
    return is_aggregate(phase) || is_phase(phase) || is_phase_to_phase(phase) ||
           is_average_phase_to_phase(phase);
}

/// Appends a validated phase suffix to a function node. `phase` 0 appends
/// nothing (the aggregate form). Throws std::invalid_argument for a suffix the
/// manual does not define, so a typo fails locally instead of coming back as
/// SCPI error -113 from the instrument.
inline std::string suffixed(const char* base, int phase,
                           bool (*accepts)(int) = &is_valid_suffix) {
    if (!accepts(phase)) {
        throw std::invalid_argument("invalid phase suffix for function \"" + std::string(base) +
                                    "\": " + std::to_string(phase));
    }
    std::string s(base);
    if (phase > 0) {
        s += std::to_string(phase);
    }
    return s;
}

namespace detail {

/// Accepts a single phase or an aggregate (the most common combination).
inline bool phase_or_aggregate(int phase) { return is_aggregate(phase) || is_phase(phase); }

/// Accepts only a single phase (per-channel quantities such as PTP/PHIGH).
inline bool phase_only(int phase) { return is_phase(phase); }

/// Accepts any phase-to-phase form, single or averaged.
inline bool any_phase_to_phase(int phase) {
    return is_phase_to_phase(phase) || is_average_phase_to_phase(phase);
}

/// Accepts only the 2nd-system aggregate (POWer460:EFFiciency).
inline bool aggregate_only(int phase) { return is_aggregate(phase); }

inline std::string node(const char* base, int phase, const char* tail,
                        bool (*accepts)(int) = &phase_or_aggregate) {
    return suffixed(base, phase, accepts) + tail;
}

} // namespace detail

// -- Voltage -----------------------------------------------------------------
inline std::string voltage(int phase = 0)          { return detail::node("VOLT", phase, ""); }        ///< true RMS
inline std::string voltage_ac(int phase = 0)       { return detail::node("VOLT", phase, ":AC"); }     ///< RMS without DC
inline std::string voltage_mean(int phase = 0)     { return detail::node("VOLT", phase, ":MEAN"); }
inline std::string voltage_rectified_mean(int phase = 0)           { return detail::node("VOLT", phase, ":RMEAN"); }
inline std::string voltage_rectified_mean_corrected(int phase = 0) { return detail::node("VOLT", phase, ":RMCORR"); }
inline std::string voltage_peak_to_peak(int phase)      { return detail::node("VOLT", phase, ":PTP", &detail::phase_only); }
inline std::string voltage_peak_high(int phase)         { return detail::node("VOLT", phase, ":PHIGH", &detail::phase_only); }
inline std::string voltage_peak_low(int phase)          { return detail::node("VOLT", phase, ":PLOW", &detail::phase_only); }
inline std::string voltage_crest_factor(int phase)      { return detail::node("VOLT", phase, ":CFAC", &detail::phase_only); }
inline std::string voltage_form_factor(int phase)       { return detail::node("VOLT", phase, ":FFAC", &detail::phase_only); }
inline std::string voltage_harmonic_content(int phase)  { return detail::node("VOLT", phase, ":HCONT", &detail::phase_only); }
inline std::string voltage_fundamental_content(int phase){ return detail::node("VOLT", phase, ":FCONT", &detail::phase_only); }
inline std::string voltage_thd(int phase)               { return detail::node("VOLT", phase, ":THD", &detail::phase_only); }
inline std::string voltage_phase(int phase)             { return detail::node("VOLT", phase, ":PHAS", &detail::phase_only); }

/// Phase-to-phase voltage, e.g. voltage_line(12) -> "VOLT12". Accepts the
/// averaged forms 123/456 too.
inline std::string voltage_line(int phase)             { return detail::node("VOLT", phase, "", &detail::any_phase_to_phase); }
inline std::string voltage_line_mean(int phase)        { return detail::node("VOLT", phase, ":MEAN", &detail::any_phase_to_phase); }
inline std::string voltage_line_rectified_mean(int phase)           { return detail::node("VOLT", phase, ":RMEAN", &detail::any_phase_to_phase); }
inline std::string voltage_line_rectified_mean_corrected(int phase) { return detail::node("VOLT", phase, ":RMCORR", &detail::any_phase_to_phase); }
inline std::string voltage_line_form_factor(int phase)  { return detail::node("VOLT", phase, ":FFAC", &is_phase_to_phase); }
inline std::string voltage_line_thd(int phase)          { return detail::node("VOLT", phase, ":THD", &is_phase_to_phase); }
inline std::string voltage_line_harmonic_content(int phase)   { return detail::node("VOLT", phase, ":HCONT", &is_phase_to_phase); }
inline std::string voltage_line_fundamental_content(int phase){ return detail::node("VOLT", phase, ":FCONT", &is_phase_to_phase); }
inline std::string voltage_line_phase(int phase)        { return detail::node("VOLT", phase, ":PHAS", &is_phase_to_phase); }

// -- Current -----------------------------------------------------------------
inline std::string current(int phase = 0)          { return detail::node("CURR", phase, ""); }        ///< true RMS
inline std::string current_ac(int phase = 0)       { return detail::node("CURR", phase, ":AC"); }
inline std::string current_mean(int phase = 0)     { return detail::node("CURR", phase, ":MEAN"); }
inline std::string current_rectified_mean(int phase = 0)           { return detail::node("CURR", phase, ":RMEAN"); }
inline std::string current_rectified_mean_corrected(int phase = 0) { return detail::node("CURR", phase, ":RMCORR"); }
inline std::string current_peak_to_peak(int phase)      { return detail::node("CURR", phase, ":PTP", &detail::phase_only); }
inline std::string current_peak_high(int phase)         { return detail::node("CURR", phase, ":PHIGH", &detail::phase_only); }
inline std::string current_peak_low(int phase)          { return detail::node("CURR", phase, ":PLOW", &detail::phase_only); }
inline std::string current_crest_factor(int phase)      { return detail::node("CURR", phase, ":CFAC", &detail::phase_only); }
inline std::string current_form_factor(int phase)       { return detail::node("CURR", phase, ":FFAC", &detail::phase_only); }
inline std::string current_harmonic_content(int phase)  { return detail::node("CURR", phase, ":HCONT", &detail::phase_only); }
inline std::string current_fundamental_content(int phase){ return detail::node("CURR", phase, ":FCONT", &detail::phase_only); }
inline std::string current_thd(int phase)               { return detail::node("CURR", phase, ":THD", &detail::phase_only); }
inline std::string current_phase(int phase)             { return detail::node("CURR", phase, ":PHAS", &detail::phase_only); }

// -- Power and derived quantities --------------------------------------------
// Note the two different short forms: POWer:REACtive is "REAC", while the
// REACTance quantity is "REACT".
inline std::string active_power(int phase = 0)    { return detail::node("POW", phase, ":ACT"); }
inline std::string apparent_power(int phase = 0)  { return detail::node("POW", phase, ":APP"); }
inline std::string reactive_power(int phase = 0)  { return detail::node("POW", phase, ":REAC"); }
inline std::string power_factor(int phase = 0)    { return detail::node("POW", phase, ":FACT"); }
inline std::string corrected_power(int phase = 0) { return detail::node("POW", phase, ":CORR"); }
/// POWer[460]:EFFiciency — the reference functions are chosen with
/// NormaInstrument::set_efficiency_reference.
inline std::string efficiency(int phase = 0)     { return detail::node("POW", phase, ":EFF", &detail::aggregate_only); }
inline std::string phase_angle(int phase = 0)    { return detail::node("PHAS", phase, ""); }
inline std::string apparent_impedance(int phase = 0)  { return detail::node("IMP", phase, ":APP"); }
inline std::string series_resistance(int phase = 0)   { return detail::node("RES", phase, ":SER"); }
inline std::string parallel_resistance(int phase = 0) { return detail::node("RES", phase, ":PAR"); }
inline std::string series_reactance(int phase = 0)    { return detail::node("REACT", phase, ":SER"); }
inline std::string parallel_reactance(int phase = 0)  { return detail::node("REACT", phase, ":PAR"); }

// -- Instrument-wide quantities ----------------------------------------------
inline std::string frequency()     { return "FREQ"; }          ///< frequency of the SYNC source
inline std::string time_interval() { return "TIME"; }          ///< length of the averaging interval [s]
inline std::string time_relative() { return "TIME:REL"; }      ///< time since TIMer:RESet [s]

// -- Modifiers ---------------------------------------------------------------
// The manual defines these as optional trailing parts of any <function> name,
// so they compose: minimum(active_power(1)) -> "POW1:ACT:MIN".
//
// HAR selects the harmonic chosen with CALCulate:HARMonic:ORDer; the
// MIN/MAX and IPOS/INEG/INT accumulators must first be enabled in the
// CALCulate subsystem (see NormaInstrument::set_integral_enabled).

/// The harmonic of a function, e.g. harmonic(active_power(1)) -> "POW1:ACT:HAR".
inline std::string harmonic(const std::string& function) { return function + ":HAR"; }

/// Smallest value seen since the MIN/MAX registers were last cleared.
inline std::string minimum(const std::string& function) { return function + ":MIN"; }

/// Largest value seen since the MIN/MAX registers were last cleared.
inline std::string maximum(const std::string& function) { return function + ":MAX"; }

/// Sum of the positive values of a function (energy-style accumulation).
inline std::string integral_positive(const std::string& function) { return function + ":IPOS"; }

/// Sum of the negative values of a function.
inline std::string integral_negative(const std::string& function) { return function + ":INEG"; }

/// Sum of all values of a function.
inline std::string integral(const std::string& function) { return function + ":INT"; }

} // namespace fn

} // namespace fluke::norma

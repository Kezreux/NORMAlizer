#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace fluke::norma {

/// The instrument's TCP port is fixed to 23 (the telnet port).
inline constexpr std::uint16_t kDefaultPort = 23;

/// SCPI's ASCII representation of "Not a Number" (returned e.g. when a
/// measurement is Undefined or Not available). Parsed values at or above
/// this magnitude are mapped to a quiet NaN.
inline constexpr double kScpiNan = 9.91e37;

/// INPut:COUPling
enum class Coupling { AC, DC };

/// INPut:SHUNt
enum class Shunt { Internal, External };

/// ROUTe:SYSTem "3W" | "2W" ("2W" requires firmware >= 1.4)
enum class WiringSystem {
    ThreeWattmeter, ///< "3W"
    TwoWattmeter,   ///< "2W" (Aron)
};

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

/// Bit flags of the per-measurement status returned by `DATA:STATus?`.
namespace measurement_status {
inline constexpr int kNormal = 0;
inline constexpr int kUnderrange = 1;
inline constexpr int kOverrange = 2;
inline constexpr int kUndefined = 8;
inline constexpr int kNotAvailable = 16;
inline constexpr int kPowerFactorCapacitive = 128;
} // namespace measurement_status

/// Result of `DATA:STATus?`: one status flag per measurement value.
struct Reading {
    std::vector<double> values;
    std::vector<int> status;
};

// ---------------------------------------------------------------------------
// Helpers to build <function> names for SENSe:FUNCtion / DATA? queries.
//
// `phase` follows the manual's phase suffixes: 1..6 = L1..L6, 12/23/31/... =
// phase-to-phase, 460 = totals of the 2nd three-phase system, and 0 = no
// suffix (average/total of the 1st system).
// ---------------------------------------------------------------------------
namespace fn {

inline std::string suffixed(const char* base, int phase) {
    std::string s(base);
    if (phase > 0) s += std::to_string(phase);
    return s;
}

inline std::string voltage(int phase = 0)        { return suffixed("VOLT", phase); }               // True RMS
inline std::string voltage_ac(int phase = 0)     { return suffixed("VOLT", phase) + ":AC"; }       // RMS without DC
inline std::string voltage_mean(int phase = 0)   { return suffixed("VOLT", phase) + ":MEAN"; }
inline std::string current(int phase = 0)        { return suffixed("CURR", phase); }               // True RMS
inline std::string current_ac(int phase = 0)     { return suffixed("CURR", phase) + ":AC"; }
inline std::string current_mean(int phase = 0)   { return suffixed("CURR", phase) + ":MEAN"; }
inline std::string active_power(int phase = 0)   { return suffixed("POW", phase) + ":ACT"; }
inline std::string apparent_power(int phase = 0) { return suffixed("POW", phase) + ":APP"; }
inline std::string reactive_power(int phase = 0) { return suffixed("POW", phase) + ":REAC"; }
inline std::string power_factor(int phase = 0)   { return suffixed("POW", phase) + ":FACT"; }
inline std::string phase_angle(int phase = 0)    { return suffixed("PHAS", phase); }
inline std::string frequency()                   { return "FREQ"; }                                // SYNC frequency
inline std::string time_interval()               { return "TIME"; }                                // averaging interval [s]

} // namespace fn

} // namespace fluke::norma

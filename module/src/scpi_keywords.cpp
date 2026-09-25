#include "scpi_keywords.hpp"

#include <cctype>

#include "fluke/norma/error.hpp"

namespace fluke::norma::detail {

namespace {

[[noreturn]] void unexpected(std::string_view what, std::string_view text) {
    throw ProtocolError("unexpected " + std::string(what) + " response: \"" +
                        std::string(text) + "\"");
}

/// True if `value` equals any of the accepted spellings of a keyword.
bool matches(const std::string& value, std::initializer_list<const char*> spellings) {
    for (const char* spelling : spellings) {
        if (value == spelling) {
            return true;
        }
    }
    return false;
}

} // namespace

std::string normalize_keyword(std::string_view text) {
    std::size_t begin = 0;
    std::size_t end = text.size();
    while (begin < end && std::isspace(static_cast<unsigned char>(text[begin]))) {
        ++begin;
    }
    while (end > begin && std::isspace(static_cast<unsigned char>(text[end - 1]))) {
        --end;
    }
    std::string out(text.substr(begin, end - begin));
    for (char& c : out) {
        c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    }
    return out;
}

// -- Setting commands: always the short form ---------------------------------

std::string to_scpi(Coupling value) { return value == Coupling::AC ? "AC" : "DC"; }
std::string to_scpi(Shunt value) { return value == Shunt::External ? "EXT" : "INT"; }
std::string to_scpi(Slope value) { return value == Slope::Negative ? "NEG" : "POS"; }
std::string to_scpi(LevelUnit value) { return value == LevelUnit::Percent ? "PCT" : "ABS"; }

std::string to_scpi(DataFormat value) {
    switch (value) {
    case DataFormat::Integer: return "INT";
    case DataFormat::Real:    return "REAL";
    case DataFormat::Ascii:   break;
    }
    return "ASC";
}

std::string to_scpi(ByteOrder value) { return value == ByteOrder::Swapped ? "SWAP" : "NORM"; }

std::string to_scpi(KeyLock value) {
    switch (value) {
    case KeyLock::On:     return "ON";
    case KeyLock::Remote: return "REM";
    case KeyLock::Off:    break;
    }
    return "OFF";
}

std::string to_scpi(TransformMode value) {
    switch (value) {
    case TransformMode::Dft: return "DFT";
    case TransformMode::Std: return "STD";
    case TransformMode::Fft: break;
    }
    return "FFT";
}

std::string to_scpi(HarmonicGrouping value) {
    switch (value) {
    case HarmonicGrouping::Harmonic:  return "HARM";
    case HarmonicGrouping::HGroup:    return "HGR";
    case HarmonicGrouping::HSGroup:   return "HSGR";
    case HarmonicGrouping::ISGroup:   return "ISGR";
    case HarmonicGrouping::SGroup:    return "SGR";
    case HarmonicGrouping::Component: break;
    }
    return "COMP";
}

std::string to_scpi(PowerCorrection value) {
    return value == PowerCorrection::Delta ? "DELT" : "STAR";
}

std::string to_scpi(IntegralStartSource value) {
    switch (value) {
    case IntegralStartSource::Time:   return "TIME";
    case IntegralStartSource::Manual: return "MAN";
    case IntegralStartSource::Command: break;
    }
    return "CMD";
}

std::string to_scpi(IntegralStopSource value) {
    switch (value) {
    case IntegralStopSource::Time:         return "TIME";
    case IntegralStopSource::Manual:       return "MAN";
    case IntegralStopSource::TimeInterval: return "TINT";
    case IntegralStopSource::Command:      break;
    }
    return "CMD";
}

// -- Query parsing: short form, long form, any casing ------------------------

Coupling parse_coupling(std::string_view text) {
    const std::string value = normalize_keyword(text);
    if (value == "AC") return Coupling::AC;
    if (value == "DC") return Coupling::DC;
    unexpected("INPut:COUPling?", text);
}

Shunt parse_shunt(std::string_view text) {
    const std::string value = normalize_keyword(text);
    if (matches(value, {"INT", "INTERNAL"})) return Shunt::Internal;
    if (matches(value, {"EXT", "EXTERNAL"})) return Shunt::External;
    unexpected("INPut:SHUNt?", text);
}

Slope parse_slope(std::string_view text) {
    const std::string value = normalize_keyword(text);
    if (matches(value, {"POS", "POSITIVE"})) return Slope::Positive;
    if (matches(value, {"NEG", "NEGATIVE"})) return Slope::Negative;
    unexpected("SLOPe?", text);
}

LevelUnit parse_level_unit(std::string_view text) {
    const std::string value = normalize_keyword(text);
    if (matches(value, {"ABS", "ABSOLUTE"})) return LevelUnit::Absolute;
    if (matches(value, {"PCT", "PERCENT"})) return LevelUnit::Percent;
    unexpected("SYNC:LEVel:UNIT?", text);
}

DataFormat parse_data_format(std::string_view text) {
    const std::string value = normalize_keyword(text);
    if (matches(value, {"ASC", "ASCII"})) return DataFormat::Ascii;
    if (matches(value, {"INT", "INTEGER"})) return DataFormat::Integer;
    if (value == "REAL") return DataFormat::Real;
    unexpected("FORMat?", text);
}

ByteOrder parse_byte_order(std::string_view text) {
    const std::string value = normalize_keyword(text);
    if (matches(value, {"NORM", "NORMAL"})) return ByteOrder::Normal;
    if (matches(value, {"SWAP", "SWAPPED"})) return ByteOrder::Swapped;
    unexpected("FORMat:BORDer?", text);
}

KeyLock parse_key_lock(std::string_view text) {
    const std::string value = normalize_keyword(text);
    if (matches(value, {"ON", "1"})) return KeyLock::On;
    if (matches(value, {"OFF", "0"})) return KeyLock::Off;
    if (matches(value, {"REM", "REMOTE"})) return KeyLock::Remote;
    unexpected("SYSTem:KLOCk?", text);
}

TransformMode parse_transform_mode(std::string_view text) {
    const std::string value = normalize_keyword(text);
    if (value == "FFT") return TransformMode::Fft;
    if (value == "DFT") return TransformMode::Dft;
    if (value == "STD") return TransformMode::Std;
    unexpected("CALCulate:TRANsform:FREQuency:MODE?", text);
}

HarmonicGrouping parse_harmonic_grouping(std::string_view text) {
    const std::string value = normalize_keyword(text);
    if (matches(value, {"COMP", "COMPONENT"})) return HarmonicGrouping::Component;
    if (matches(value, {"HARM", "HARMONIC"})) return HarmonicGrouping::Harmonic;
    if (matches(value, {"HGR", "HGROUP"})) return HarmonicGrouping::HGroup;
    if (matches(value, {"HSGR", "HSGROUP"})) return HarmonicGrouping::HSGroup;
    if (matches(value, {"ISGR", "ISGROUP"})) return HarmonicGrouping::ISGroup;
    if (matches(value, {"SGR", "SGROUP"})) return HarmonicGrouping::SGroup;
    unexpected("CALCulate:TRANsform:FREQuency:GROuping?", text);
}

PowerCorrection parse_power_correction(std::string_view text) {
    const std::string value = normalize_keyword(text);
    if (value == "STAR") return PowerCorrection::Star;
    if (matches(value, {"DELT", "DELTA"})) return PowerCorrection::Delta;
    unexpected("CALCulate:POWer:CORRected?", text);
}

IntegralStartSource parse_integral_start_source(std::string_view text) {
    const std::string value = normalize_keyword(text);
    if (value == "CMD") return IntegralStartSource::Command;
    if (value == "TIME") return IntegralStartSource::Time;
    if (matches(value, {"MAN", "MANUAL"})) return IntegralStartSource::Manual;
    unexpected("CALCulate:INTegral:STARt:SOURce?", text);
}

IntegralStopSource parse_integral_stop_source(std::string_view text) {
    const std::string value = normalize_keyword(text);
    if (value == "CMD") return IntegralStopSource::Command;
    if (value == "TIME") return IntegralStopSource::Time;
    if (matches(value, {"MAN", "MANUAL"})) return IntegralStopSource::Manual;
    if (matches(value, {"TINT", "TINTERVAL"})) return IntegralStopSource::TimeInterval;
    unexpected("CALCulate:INTegral:STOP:SOURce?", text);
}

} // namespace fluke::norma::detail

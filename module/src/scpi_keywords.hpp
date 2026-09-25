#pragma once

// Conversions between the library's enums and the instrument's character-data
// keywords. Internal to the library: the header lives in src/ and is not
// installed.
//
// Setting commands always send the SCPI *short* form, which every firmware
// accepts. Query parsing accepts the short form, the long form and any casing,
// because the manual only promises "the short form of the text is returned".

#include <string>
#include <string_view>

#include "fluke/norma/types.hpp"

namespace fluke::norma::detail {

/// Trims and uppercases a response keyword so comparisons are case-insensitive.
std::string normalize_keyword(std::string_view text);

std::string to_scpi(Coupling value);
std::string to_scpi(Shunt value);
std::string to_scpi(Slope value);
std::string to_scpi(LevelUnit value);
std::string to_scpi(DataFormat value);
std::string to_scpi(ByteOrder value);
std::string to_scpi(KeyLock value);
std::string to_scpi(TransformMode value);
std::string to_scpi(HarmonicGrouping value);
std::string to_scpi(PowerCorrection value);
std::string to_scpi(IntegralStartSource value);
std::string to_scpi(IntegralStopSource value);

/// Parsers. Each throws ProtocolError when the response is not one of the
/// keywords the manual defines for that command.
Coupling parse_coupling(std::string_view text);
Shunt parse_shunt(std::string_view text);
Slope parse_slope(std::string_view text);
LevelUnit parse_level_unit(std::string_view text);
DataFormat parse_data_format(std::string_view text);
ByteOrder parse_byte_order(std::string_view text);
KeyLock parse_key_lock(std::string_view text);
TransformMode parse_transform_mode(std::string_view text);
HarmonicGrouping parse_harmonic_grouping(std::string_view text);
PowerCorrection parse_power_correction(std::string_view text);
IntegralStartSource parse_integral_start_source(std::string_view text);
IntegralStopSource parse_integral_stop_source(std::string_view text);

} // namespace fluke::norma::detail

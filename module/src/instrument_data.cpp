// NormaInstrument: measurement functions, acquisition, triggering, data
// queries, CALCulate (harmonics and integration) and memory recording.

#include "fluke/norma/instrument.hpp"

#include <stdexcept>
#include <string>

#include "fluke/norma/error.hpp"
#include "scpi_keywords.hpp"

namespace fluke::norma {

namespace {

/// "SWE1"/"SWE2" — the mandatory suffix on the memory-recording nodes.
std::string sweep_node(SweepBlock block) {
    return std::string("SWE") + (block == SweepBlock::Block2 ? "2" : "1");
}

/// "1"/"2" — the <block> argument of the TRACe queries.
std::string trace_block(SweepBlock block) {
    return block == SweepBlock::Block2 ? "2" : "1";
}

/// "<block>[,<count>[,<offset>[,<sparsing>]]]" — the TRACe queries take the
/// block first, so the optional arguments follow it as a comma-separated tail.
/// Positional arguments can only be given when every earlier one is, so a
/// count of 0 (meaning "everything") drops the whole tail.
std::string trace_arguments(SweepBlock block, int count, int offset, int sparsing) {
    std::string args = trace_block(block);
    if (count <= 0) {
        return args;
    }
    args += "," + std::to_string(count);
    if (offset > 0 || sparsing > 0) {
        args += "," + std::to_string(offset);
    }
    if (sparsing > 0) {
        args += "," + std::to_string(sparsing);
    }
    return args;
}

void require_functions(const char* what, const std::vector<std::string>& functions) {
    if (functions.empty()) {
        throw std::invalid_argument(std::string(what) + ": the function list must not be empty");
    }
}

/// Formats the date/time parameter shared by the TRIGger and CALCulate:INTegral
/// time sources: yyyy,MM,dd,hh,mm,ss.
std::string format_timestamp(const Date& date, const Time& time) {
    return std::to_string(date.year) + "," + std::to_string(date.month) + "," +
           std::to_string(date.day) + "," + std::to_string(time.hours) + "," +
           std::to_string(time.minutes) + "," + std::to_string(time.seconds);
}

/// Parses a `...:DATA:PREamble?` response. The manual describes it as a
/// comma-separated header whose exact fields depend on the block, so the
/// numbers we can rely on are extracted and the whole line is kept as well.
DataPreamble parse_preamble(const std::string& response) {
    DataPreamble preamble;
    preamble.raw = response;
    const auto fields = ScpiClient::split_csv(response);
    for (std::size_t i = 0; i < fields.size(); ++i) {
        const std::string& field = fields[i];
        // Quoted fields are function names; everything else is numeric.
        if (!field.empty() && (field.front() == '"' || field.front() == '\'')) {
            preamble.functions.push_back(ScpiClient::unquote(field));
            continue;
        }
        switch (preamble.functions.empty() ? i : i - preamble.functions.size()) {
        case 0: preamble.count = ScpiClient::to_int(field); break;
        case 1: preamble.function_count = ScpiClient::to_int(field); break;
        case 2: preamble.interval = ScpiClient::to_double(field); break;
        case 3: preamble.start = ScpiClient::to_double(field); break;
        default: break;
        }
    }
    return preamble;
}

} // namespace

// -- SENSe: measurement functions ------------------------------------------------

void NormaInstrument::set_functions(const std::vector<std::string>& functions) {
    if (functions.empty()) {
        throw std::invalid_argument("set_functions: the function list must not be empty "
                                    "(use clear_functions() to turn all off)");
    }
    scpi_->send("FUNC " + ScpiClient::quote_join(functions));
}

std::vector<std::string> NormaInstrument::functions() {
    return scpi_->query_strings("FUNC?");
}

void NormaInstrument::enable_all_functions() { scpi_->send("FUNC:ON:ALL"); }

void NormaInstrument::clear_functions() { scpi_->send("FUNC:OFF:ALL"); }

int NormaInstrument::function_count() { return scpi_->query_int("FUNC:COUN?"); }

void NormaInstrument::set_concurrent(bool on) {
    scpi_->send("FUNC:CONC " + ScpiClient::format_bool(on));
}

bool NormaInstrument::concurrent() { return scpi_->query_bool("FUNC:CONC?"); }

// -- Acquisition ------------------------------------------------------------------

void NormaInstrument::set_continuous(bool on) {
    scpi_->send(on ? "INIT:CONT ON" : "INIT:CONT OFF");
}

bool NormaInstrument::continuous() { return scpi_->query_bool("INIT:CONT?"); }

void NormaInstrument::initiate() { scpi_->send("INIT"); }

void NormaInstrument::initiate_sweep(SweepBlock block) {
    scpi_->send("INIT:SEQ" + trace_block(block));
}

void NormaInstrument::abort() { scpi_->send("ABOR"); }

// -- TRIGger -----------------------------------------------------------------------

void NormaInstrument::set_trigger_start_source(std::string_view source) {
    scpi_->send("TRIG:STAR:SOUR " + std::string(source));
}

std::string NormaInstrument::trigger_start_source() {
    return ScpiClient::unquote(scpi_->query("TRIG:STAR:SOUR?"));
}

void NormaInstrument::set_trigger_start_level(double level) {
    scpi_->send("TRIG:STAR:LEV " + ScpiClient::format_double(level));
}

double NormaInstrument::trigger_start_level() { return scpi_->query_value("TRIG:STAR:LEV?"); }

void NormaInstrument::set_trigger_start_slope(Slope slope) {
    scpi_->send("TRIG:STAR:SLOP " + detail::to_scpi(slope));
}

Slope NormaInstrument::trigger_start_slope() {
    return detail::parse_slope(scpi_->query("TRIG:STAR:SLOP?"));
}

void NormaInstrument::set_trigger_start_time(const Date& date, const Time& time) {
    scpi_->send("TRIG:STAR:TIME " + format_timestamp(date, time));
}

void NormaInstrument::set_trigger_stop_source(std::string_view source) {
    scpi_->send("TRIG:STOP:SOUR " + std::string(source));
}

std::string NormaInstrument::trigger_stop_source() {
    return ScpiClient::unquote(scpi_->query("TRIG:STOP:SOUR?"));
}

void NormaInstrument::set_trigger_stop_level(double level) {
    scpi_->send("TRIG:STOP:LEV " + ScpiClient::format_double(level));
}

double NormaInstrument::trigger_stop_level() { return scpi_->query_value("TRIG:STOP:LEV?"); }

void NormaInstrument::set_trigger_stop_slope(Slope slope) {
    scpi_->send("TRIG:STOP:SLOP " + detail::to_scpi(slope));
}

Slope NormaInstrument::trigger_stop_slope() {
    return detail::parse_slope(scpi_->query("TRIG:STOP:SLOP?"));
}

void NormaInstrument::set_trigger_stop_time(const Date& date, const Time& time) {
    scpi_->send("TRIG:STOP:TIME " + format_timestamp(date, time));
}

// -- Data ---------------------------------------------------------------------------

std::vector<double> NormaInstrument::data(const std::vector<std::string>& functions) {
    if (functions.empty()) {
        return scpi_->query_values("DATA?");
    }
    return scpi_->query_values("DATA? " + ScpiClient::quote_join(functions));
}

Reading NormaInstrument::data_with_status(const std::vector<std::string>& functions) {
    const std::string command = functions.empty()
                                    ? std::string("DATA:STAT?")
                                    : "DATA:STAT? " + ScpiClient::quote_join(functions);
    const auto raw = scpi_->query_values(command);
    if (raw.size() % 2 != 0) {
        throw ProtocolError("DATA:STAT? returned an odd number of fields (" +
                            std::to_string(raw.size()) + ")");
    }

    Reading reading;
    // The manual appends all status values after all measurement values.
    const std::size_t count = raw.size() / 2;
    reading.values.assign(raw.begin(), raw.begin() + static_cast<std::ptrdiff_t>(count));
    reading.status.reserve(count);
    for (std::size_t i = count; i < raw.size(); ++i) {
        reading.status.push_back(static_cast<int>(raw[i]));
    }
    return reading;
}

// -- CALCulate: harmonics and spectrum ----------------------------------------------

void NormaInstrument::set_harmonic_order(int order) {
    if (order < 0) {
        throw std::invalid_argument("harmonic order must not be negative, got " +
                                    std::to_string(order));
    }
    scpi_->send("CALC:HARM:ORD " + std::to_string(order));
}

int NormaInstrument::harmonic_order() { return scpi_->query_int("CALC:HARM:ORD?"); }

void NormaInstrument::transform_once() { scpi_->send("CALC:TRAN:FREQ ONCE"); }

void NormaInstrument::set_transform_mode(TransformMode mode) {
    scpi_->send("CALC:TRAN:FREQ:MODE " + detail::to_scpi(mode));
}

TransformMode NormaInstrument::transform_mode() {
    return detail::parse_transform_mode(scpi_->query("CALC:TRAN:FREQ:MODE?"));
}

void NormaInstrument::set_transform_functions(const std::vector<std::string>& functions) {
    require_functions("set_transform_functions", functions);
    scpi_->send("CALC:TRAN:FREQ:FUNC " + ScpiClient::quote_join(functions));
}

std::vector<std::string> NormaInstrument::transform_functions() {
    return scpi_->query_strings("CALC:TRAN:FREQ:FUNC?");
}

void NormaInstrument::set_transform_start(double hertz) {
    scpi_->send("CALC:TRAN:FREQ:STAR " + ScpiClient::format_double(hertz));
}

double NormaInstrument::transform_start() { return scpi_->query_value("CALC:TRAN:FREQ:STAR?"); }

void NormaInstrument::set_transform_stop(double hertz) {
    scpi_->send("CALC:TRAN:FREQ:STOP " + ScpiClient::format_double(hertz));
}

double NormaInstrument::transform_stop() { return scpi_->query_value("CALC:TRAN:FREQ:STOP?"); }

void NormaInstrument::set_transform_cycles(int cycles) {
    switch (cycles) {
    case 4: case 6: case 8: case 10: case 12:
        break;
    default:
        throw std::invalid_argument("transform cycles must be 4, 6, 8, 10 or 12, got " +
                                    std::to_string(cycles));
    }
    scpi_->send("CALC:TRAN:FREQ:CYCL " + std::to_string(cycles));
}

int NormaInstrument::transform_cycles() { return scpi_->query_int("CALC:TRAN:FREQ:CYCL?"); }

void NormaInstrument::set_transform_grouping(HarmonicGrouping grouping) {
    scpi_->send("CALC:TRAN:FREQ:GRO " + detail::to_scpi(grouping));
}

HarmonicGrouping NormaInstrument::transform_grouping() {
    return detail::parse_harmonic_grouping(scpi_->query("CALC:TRAN:FREQ:GRO?"));
}

std::vector<double> NormaInstrument::transform_data(int count, int offset) {
    return scpi_->query_values("CALC:DATA?" + data_arguments(count, offset));
}

DataPreamble NormaInstrument::transform_preamble() {
    return parse_preamble(scpi_->query("CALC:DATA:PRE?"));
}

std::vector<double> NormaInstrument::transform_thd() {
    return scpi_->query_values("CALC:DATA:THD?");
}

// -- CALCulate: integration (energy) -------------------------------------------------

void NormaInstrument::set_integral_enabled(bool on) {
    scpi_->send("CALC:INT:STAT " + ScpiClient::format_bool(on));
}

bool NormaInstrument::integral_enabled() { return scpi_->query_bool("CALC:INT:STAT?"); }

void NormaInstrument::set_integral_functions(const std::vector<std::string>& functions) {
    require_functions("set_integral_functions", functions);
    scpi_->send("CALC:INT:FUNC " + ScpiClient::quote_join(functions));
}

std::vector<std::string> NormaInstrument::integral_functions() {
    return scpi_->query_strings("CALC:INT:FUNC?");
}

void NormaInstrument::clear_integral() { scpi_->send("CALC:INT:CLE"); }

void NormaInstrument::set_integral_auto_clear(bool on) {
    scpi_->send("CALC:INT:CLE:AUTO " + ScpiClient::format_bool(on));
}

bool NormaInstrument::integral_auto_clear() { return scpi_->query_bool("CALC:INT:CLE:AUTO?"); }

void NormaInstrument::set_integral_start_source(IntegralStartSource source) {
    scpi_->send("CALC:INT:STAR:SOUR " + detail::to_scpi(source));
}

IntegralStartSource NormaInstrument::integral_start_source() {
    return detail::parse_integral_start_source(scpi_->query("CALC:INT:STAR:SOUR?"));
}

void NormaInstrument::start_integral() { scpi_->send("CALC:INT:STAR"); }

void NormaInstrument::set_integral_start_time(const Date& date, const Time& time) {
    scpi_->send("CALC:INT:STAR:TIME " + format_timestamp(date, time));
}

void NormaInstrument::set_integral_stop_source(IntegralStopSource source) {
    scpi_->send("CALC:INT:STOP:SOUR " + detail::to_scpi(source));
}

IntegralStopSource NormaInstrument::integral_stop_source() {
    return detail::parse_integral_stop_source(scpi_->query("CALC:INT:STOP:SOUR?"));
}

void NormaInstrument::stop_integral() { scpi_->send("CALC:INT:STOP"); }

void NormaInstrument::set_integral_stop_time(const Date& date, const Time& time) {
    scpi_->send("CALC:INT:STOP:TIME " + format_timestamp(date, time));
}

void NormaInstrument::set_integral_stop_interval(double seconds) {
    scpi_->send("CALC:INT:STOP:TINT " + ScpiClient::format_double(seconds));
}

double NormaInstrument::integral_stop_interval() {
    return scpi_->query_value("CALC:INT:STOP:TINT?");
}

// -- CALCulate: power ----------------------------------------------------------------

void NormaInstrument::set_power_correction(PowerCorrection correction) {
    scpi_->send("CALC:POW:CORR " + detail::to_scpi(correction));
}

PowerCorrection NormaInstrument::power_correction() {
    return detail::parse_power_correction(scpi_->query("CALC:POW:CORR?"));
}

void NormaInstrument::set_efficiency_reference(std::string_view input, std::string_view output,
                                              int phase) {
    if (!fn::is_aggregate(phase)) {
        throw std::invalid_argument(
            "efficiency reference is defined for the aggregate suffixes 0 and 460, got " +
            std::to_string(phase));
    }
    const std::string node = phase == 460 ? "CALC:POW460:EFF:REF" : "CALC:POW:EFF:REF";
    scpi_->send(node + " " + ScpiClient::quote(input) + "," + ScpiClient::quote(output));
}

// -- Memory recording: SENSe:SWEep ------------------------------------------------------

void NormaInstrument::set_sweep_enabled(SweepBlock block, bool on) {
    scpi_->send(sweep_node(block) + ":STAT " + ScpiClient::format_bool(on));
}

bool NormaInstrument::sweep_enabled(SweepBlock block) {
    return scpi_->query_bool(sweep_node(block) + ":STAT?");
}

void NormaInstrument::set_sweep_time(SweepBlock block, double seconds) {
    scpi_->send(sweep_node(block) + ":TIME " + ScpiClient::format_double(seconds));
}

void NormaInstrument::set_sweep_time_max(SweepBlock block) {
    scpi_->send(sweep_node(block) + ":TIME MAX");
}

double NormaInstrument::sweep_time(SweepBlock block) {
    return scpi_->query_value(sweep_node(block) + ":TIME?");
}

void NormaInstrument::set_sweep_offset_time(SweepBlock block, double seconds) {
    scpi_->send(sweep_node(block) + ":OFFS:TIME " + ScpiClient::format_double(seconds));
}

double NormaInstrument::sweep_offset_time(SweepBlock block) {
    return scpi_->query_value(sweep_node(block) + ":OFFS:TIME?");
}

int NormaInstrument::sweep_points(SweepBlock block) {
    return scpi_->query_int(sweep_node(block) + ":POINTS?");
}

int NormaInstrument::sweep_offset_points(SweepBlock block) {
    return scpi_->query_int(sweep_node(block) + ":OFFS:POINTS?");
}

void NormaInstrument::set_sweep_count(SweepBlock block, int count) {
    if (count < 1) {
        throw std::invalid_argument("sweep count must be at least 1, got " +
                                    std::to_string(count));
    }
    scpi_->send(sweep_node(block) + ":COUN " + std::to_string(count));
}

int NormaInstrument::sweep_count(SweepBlock block) {
    return scpi_->query_int(sweep_node(block) + ":COUN?");
}

void NormaInstrument::set_sweep_sparsing(SweepBlock block, int factor) {
    if (factor < 1 || factor > 65535) {
        throw std::invalid_argument("sparsing factor must be 1..65535, got " +
                                    std::to_string(factor));
    }
    scpi_->send(sweep_node(block) + ":SFAC " + std::to_string(factor));
}

int NormaInstrument::sweep_sparsing(SweepBlock block) {
    return scpi_->query_int(sweep_node(block) + ":SFAC?");
}

void NormaInstrument::set_sweep_functions(SweepBlock block,
                                          const std::vector<std::string>& functions) {
    require_functions("set_sweep_functions", functions);
    scpi_->send(sweep_node(block) + ":FUNC " + ScpiClient::quote_join(functions));
}

std::vector<std::string> NormaInstrument::sweep_functions(SweepBlock block) {
    return scpi_->query_strings(sweep_node(block) + ":FUNC?");
}

// -- Memory recording: TRACe --------------------------------------------------------------

DataPreamble NormaInstrument::trace_preamble(SweepBlock block) {
    return parse_preamble(scpi_->query("TRAC:PRE? " + trace_block(block)));
}

std::vector<double> NormaInstrument::trace_data(SweepBlock block, int count, int offset,
                                                int sparsing) {
    return scpi_->query_values("TRAC? " + trace_arguments(block, count, offset, sparsing));
}

std::vector<int> NormaInstrument::trace_status(SweepBlock block, int count, int offset,
                                               int sparsing) {
    const auto values =
        scpi_->query_values("TRAC:STAT? " + trace_arguments(block, count, offset, sparsing));
    std::vector<int> status;
    status.reserve(values.size());
    for (double value : values) {
        status.push_back(static_cast<int>(value));
    }
    return status;
}

int NormaInstrument::trace_free() { return scpi_->query_int("TRAC:FREE?"); }

int NormaInstrument::trace_length() { return scpi_->query_int("TRAC:CAT:LEN?"); }

void NormaInstrument::delete_traces() { scpi_->send("TRAC:DEL:ALL"); }

} // namespace fluke::norma

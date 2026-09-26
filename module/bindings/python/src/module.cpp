#include <pybind11/operators.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include <chrono>
#include <memory>
#include <string>

#include <fluke/norma/norma.hpp>

namespace py = pybind11;
using namespace fluke::norma;

namespace {

std::chrono::milliseconds to_ms(double seconds) {
    return std::chrono::milliseconds(static_cast<long long>(seconds * 1000.0));
}

} // namespace

PYBIND11_MODULE(_core, m) {
    m.doc() = "Python bindings for the Fluke NORMA 4000/5000 TCP/SCPI wrapper";
    m.attr("__version__") = kVersionString;
    m.attr("DEFAULT_PORT") = kDefaultPort;
    m.attr("SCPI_NAN") = kScpiNan;
    m.attr("MIN_APERTURE") = kMinAperture;
    m.attr("MAX_APERTURE") = kMaxAperture;
    m.attr("MAX_PHASE") = kMaxPhase;
    m.attr("MAX_INPUT_CHANNEL") = kMaxInputChannel;

    // -- Exceptions -----------------------------------------------------------
    auto base = py::register_exception<Error>(m, "NormaError");
    py::register_exception<ConnectionError>(m, "ConnectionError", base);
    py::register_exception<TimeoutError>(m, "TimeoutError", base);
    py::register_exception<ProtocolError>(m, "ProtocolError", base);
    py::register_exception<ScpiError>(m, "ScpiError", base);

    // -- Value types ----------------------------------------------------------
    py::class_<Identification>(m, "Identification")
        .def_readonly("manufacturer", &Identification::manufacturer)
        .def_readonly("model", &Identification::model)
        .def_readonly("serial_number", &Identification::serial_number)
        .def_readonly("firmware_version", &Identification::firmware_version)
        .def("__repr__", [](const Identification& id) {
            return "Identification(manufacturer='" + id.manufacturer + "', model='" +
                   id.model + "', serial_number='" + id.serial_number +
                   "', firmware_version='" + id.firmware_version + "')";
        });

    py::class_<ScpiErrorInfo>(m, "ScpiErrorInfo")
        .def_readonly("code", &ScpiErrorInfo::code)
        .def_readonly("message", &ScpiErrorInfo::message)
        .def("__repr__", [](const ScpiErrorInfo& e) {
            return "ScpiErrorInfo(code=" + std::to_string(e.code) + ", message='" +
                   e.message + "')";
        });

    py::class_<Reading>(m, "Reading")
        .def_readonly("values", &Reading::values)
        .def_readonly("status", &Reading::status)
        .def("is_valid", &Reading::is_valid, py::arg("index"),
             "True when the value at `index` carries none of the invalid status bits.")
        .def("__repr__", [](const Reading& r) {
            return "Reading(" + std::to_string(r.values.size()) + " values)";
        });

    py::class_<DataFormatSetting>(m, "DataFormatSetting")
        .def_readonly("format", &DataFormatSetting::format)
        .def_readonly("length", &DataFormatSetting::length)
        .def("__repr__", [](const DataFormatSetting& s) {
            return "DataFormatSetting(length=" + std::to_string(s.length) + ")";
        });

    py::class_<Date>(m, "Date")
        .def(py::init([](int year, int month, int day) { return Date{year, month, day}; }),
             py::arg("year"), py::arg("month"), py::arg("day"))
        .def_readwrite("year", &Date::year)
        .def_readwrite("month", &Date::month)
        .def_readwrite("day", &Date::day)
        .def("__repr__", [](const Date& d) {
            return "Date(" + std::to_string(d.year) + ", " + std::to_string(d.month) + ", " +
                   std::to_string(d.day) + ")";
        });

    py::class_<Time>(m, "Time")
        .def(py::init([](int hours, int minutes, int seconds) {
                 return Time{hours, minutes, seconds};
             }),
             py::arg("hours"), py::arg("minutes"), py::arg("seconds"))
        .def_readwrite("hours", &Time::hours)
        .def_readwrite("minutes", &Time::minutes)
        .def_readwrite("seconds", &Time::seconds)
        .def("__repr__", [](const Time& t) {
            return "Time(" + std::to_string(t.hours) + ", " + std::to_string(t.minutes) + ", " +
                   std::to_string(t.seconds) + ")";
        });

    py::class_<DataPreamble>(m, "DataPreamble")
        .def_readonly("count", &DataPreamble::count)
        .def_readonly("function_count", &DataPreamble::function_count)
        .def_readonly("interval", &DataPreamble::interval)
        .def_readonly("start", &DataPreamble::start)
        .def_readonly("functions", &DataPreamble::functions)
        .def_readonly("raw", &DataPreamble::raw)
        .def("__repr__", [](const DataPreamble& p) {
            return "DataPreamble(count=" + std::to_string(p.count) + ", function_count=" +
                   std::to_string(p.function_count) + ")";
        });

    // -- Enumerations ----------------------------------------------------------
    py::enum_<WiringSystem>(m, "WiringSystem")
        .value("THREE_WATTMETER", WiringSystem::ThreeWattmeter)
        .value("TWO_WATTMETER", WiringSystem::TwoWattmeter);

    py::enum_<Coupling>(m, "Coupling")
        .value("AC", Coupling::AC)
        .value("DC", Coupling::DC);

    py::enum_<Shunt>(m, "Shunt")
        .value("INTERNAL", Shunt::Internal)
        .value("EXTERNAL", Shunt::External);

    py::enum_<Slope>(m, "Slope")
        .value("POSITIVE", Slope::Positive)
        .value("NEGATIVE", Slope::Negative);

    py::enum_<LevelUnit>(m, "LevelUnit")
        .value("ABSOLUTE", LevelUnit::Absolute)
        .value("PERCENT", LevelUnit::Percent);

    py::enum_<DataFormat>(m, "DataFormat")
        .value("ASCII", DataFormat::Ascii)
        .value("INTEGER", DataFormat::Integer)
        .value("REAL", DataFormat::Real);

    py::enum_<ByteOrder>(m, "ByteOrder")
        .value("NORMAL", ByteOrder::Normal)
        .value("SWAPPED", ByteOrder::Swapped);

    py::enum_<KeyLock>(m, "KeyLock")
        .value("OFF", KeyLock::Off)
        .value("ON", KeyLock::On)
        .value("REMOTE", KeyLock::Remote);

    py::enum_<TransformMode>(m, "TransformMode")
        .value("FFT", TransformMode::Fft)
        .value("DFT", TransformMode::Dft)
        .value("STD", TransformMode::Std);

    py::enum_<HarmonicGrouping>(m, "HarmonicGrouping")
        .value("COMPONENT", HarmonicGrouping::Component)
        .value("HARMONIC", HarmonicGrouping::Harmonic)
        .value("H_GROUP", HarmonicGrouping::HGroup)
        .value("HS_GROUP", HarmonicGrouping::HSGroup)
        .value("IS_GROUP", HarmonicGrouping::ISGroup)
        .value("S_GROUP", HarmonicGrouping::SGroup);

    py::enum_<PowerCorrection>(m, "PowerCorrection")
        .value("STAR", PowerCorrection::Star)
        .value("DELTA", PowerCorrection::Delta);

    py::enum_<IntegralStartSource>(m, "IntegralStartSource")
        .value("COMMAND", IntegralStartSource::Command)
        .value("TIME", IntegralStartSource::Time)
        .value("MANUAL", IntegralStartSource::Manual);

    py::enum_<IntegralStopSource>(m, "IntegralStopSource")
        .value("COMMAND", IntegralStopSource::Command)
        .value("TIME", IntegralStopSource::Time)
        .value("MANUAL", IntegralStopSource::Manual)
        .value("TIME_INTERVAL", IntegralStopSource::TimeInterval);

    py::enum_<SweepBlock>(m, "SweepBlock")
        .value("BLOCK1", SweepBlock::Block1)
        .value("BLOCK2", SweepBlock::Block2);

    py::enum_<StatusRegister>(m, "StatusRegister")
        .value("OPERATION", StatusRegister::Operation)
        .value("QUESTIONABLE", StatusRegister::Questionable)
        .value("QUESTIONABLE_VOLTAGE", StatusRegister::QuestionableVoltage)
        .value("QUESTIONABLE_CURRENT", StatusRegister::QuestionableCurrent);

    py::enum_<RegisterPart>(m, "RegisterPart")
        .value("CONDITION", RegisterPart::Condition)
        .value("EVENT", RegisterPart::Event)
        .value("ENABLE", RegisterPart::Enable)
        .value("POSITIVE_TRANSITION", RegisterPart::PositiveTransition)
        .value("NEGATIVE_TRANSITION", RegisterPart::NegativeTransition);

    // -- Status bit constants ----------------------------------------------------
    auto status = m.def_submodule("measurement_status", "Bit flags for Reading.status");
    status.attr("NORMAL") = measurement_status::kNormal;
    status.attr("UNDERRANGE") = measurement_status::kUnderrange;
    status.attr("OVERRANGE") = measurement_status::kOverrange;
    status.attr("UNDEFINED") = measurement_status::kUndefined;
    status.attr("NOT_AVAILABLE") = measurement_status::kNotAvailable;
    status.attr("POWER_FACTOR_CAPACITIVE") = measurement_status::kPowerFactorCapacitive;
    status.attr("INVALID_MASK") = measurement_status::kInvalidMask;

    auto stb = m.def_submodule("status_byte", "Bits of *STB? and *SRE");
    stb.attr("ERROR_QUEUE_NOT_EMPTY") = status_byte::kErrorQueueNotEmpty;
    stb.attr("QUESTIONABLE_SUMMARY") = status_byte::kQuestionableSummary;
    stb.attr("MESSAGE_AVAILABLE") = status_byte::kMessageAvailable;
    stb.attr("EVENT_STATUS_SUMMARY") = status_byte::kEventStatusSummary;
    stb.attr("MASTER_STATUS_SUMMARY") = status_byte::kMasterStatusSummary;

    auto oper = m.def_submodule("operation_status", "Bits of STATus:OPERation");
    oper.attr("RANGING") = operation_status::kRanging;
    oper.attr("SWEEPING") = operation_status::kSweeping;
    oper.attr("WAITING_FOR_TRIGGER") = operation_status::kWaitingForTrigger;
    oper.attr("SYNCHRONIZED") = operation_status::kSynchronized;
    oper.attr("SYNC_AVAILABLE") = operation_status::kSyncAvailable;
    oper.attr("AVERAGING") = operation_status::kAveraging;
    oper.attr("CALCULATING") = operation_status::kCalculating;

    auto ques = m.def_submodule("questionable_status", "Bits of STATus:QUEStionable");
    ques.attr("VOLTAGE_SUMMARY") = questionable_status::kVoltageSummary;
    ques.attr("CURRENT_SUMMARY") = questionable_status::kCurrentSummary;
    ques.attr("FREQUENCY") = questionable_status::kFrequency;

    auto channel = m.def_submodule(
        "channel_status", "Bits of STATus:QUEStionable:VOLTage and :CURRent (index 0..5)");
    channel.def("overrange", &channel_status::overrange, py::arg("index"));
    channel.def("underrange", &channel_status::underrange, py::arg("index"));
    channel.attr("OVERRANGE_MASK") = channel_status::kOverrangeMask;
    channel.attr("UNDERRANGE_MASK") = channel_status::kUnderrangeMask;

    // -- <function> name helpers ----------------------------------------------
    auto fns = m.def_submodule("fn", "Builders for SENSe <function> names");
    fns.def("is_phase", &fn::is_phase, py::arg("phase"));
    fns.def("is_phase_to_phase", &fn::is_phase_to_phase, py::arg("phase"));
    fns.def("is_aggregate", &fn::is_aggregate, py::arg("phase"));
    fns.def("is_average_phase_to_phase", &fn::is_average_phase_to_phase, py::arg("phase"));
    fns.def("is_valid_suffix", &fn::is_valid_suffix, py::arg("phase"));

    fns.def("voltage", &fn::voltage, py::arg("phase") = 0);
    fns.def("voltage_ac", &fn::voltage_ac, py::arg("phase") = 0);
    fns.def("voltage_mean", &fn::voltage_mean, py::arg("phase") = 0);
    fns.def("voltage_rectified_mean", &fn::voltage_rectified_mean, py::arg("phase") = 0);
    fns.def("voltage_rectified_mean_corrected", &fn::voltage_rectified_mean_corrected,
            py::arg("phase") = 0);
    fns.def("voltage_peak_to_peak", &fn::voltage_peak_to_peak, py::arg("phase"));
    fns.def("voltage_peak_high", &fn::voltage_peak_high, py::arg("phase"));
    fns.def("voltage_peak_low", &fn::voltage_peak_low, py::arg("phase"));
    fns.def("voltage_crest_factor", &fn::voltage_crest_factor, py::arg("phase"));
    fns.def("voltage_form_factor", &fn::voltage_form_factor, py::arg("phase"));
    fns.def("voltage_harmonic_content", &fn::voltage_harmonic_content, py::arg("phase"));
    fns.def("voltage_fundamental_content", &fn::voltage_fundamental_content, py::arg("phase"));
    fns.def("voltage_thd", &fn::voltage_thd, py::arg("phase"));
    fns.def("voltage_phase", &fn::voltage_phase, py::arg("phase"));
    fns.def("voltage_line", &fn::voltage_line, py::arg("phase"));
    fns.def("voltage_line_mean", &fn::voltage_line_mean, py::arg("phase"));
    fns.def("voltage_line_rectified_mean", &fn::voltage_line_rectified_mean, py::arg("phase"));
    fns.def("voltage_line_rectified_mean_corrected",
            &fn::voltage_line_rectified_mean_corrected, py::arg("phase"));
    fns.def("voltage_line_form_factor", &fn::voltage_line_form_factor, py::arg("phase"));
    fns.def("voltage_line_thd", &fn::voltage_line_thd, py::arg("phase"));
    fns.def("voltage_line_harmonic_content", &fn::voltage_line_harmonic_content,
            py::arg("phase"));
    fns.def("voltage_line_fundamental_content", &fn::voltage_line_fundamental_content,
            py::arg("phase"));
    fns.def("voltage_line_phase", &fn::voltage_line_phase, py::arg("phase"));

    fns.def("current", &fn::current, py::arg("phase") = 0);
    fns.def("current_ac", &fn::current_ac, py::arg("phase") = 0);
    fns.def("current_mean", &fn::current_mean, py::arg("phase") = 0);
    fns.def("current_rectified_mean", &fn::current_rectified_mean, py::arg("phase") = 0);
    fns.def("current_rectified_mean_corrected", &fn::current_rectified_mean_corrected,
            py::arg("phase") = 0);
    fns.def("current_peak_to_peak", &fn::current_peak_to_peak, py::arg("phase"));
    fns.def("current_peak_high", &fn::current_peak_high, py::arg("phase"));
    fns.def("current_peak_low", &fn::current_peak_low, py::arg("phase"));
    fns.def("current_crest_factor", &fn::current_crest_factor, py::arg("phase"));
    fns.def("current_form_factor", &fn::current_form_factor, py::arg("phase"));
    fns.def("current_harmonic_content", &fn::current_harmonic_content, py::arg("phase"));
    fns.def("current_fundamental_content", &fn::current_fundamental_content, py::arg("phase"));
    fns.def("current_thd", &fn::current_thd, py::arg("phase"));
    fns.def("current_phase", &fn::current_phase, py::arg("phase"));

    fns.def("active_power", &fn::active_power, py::arg("phase") = 0);
    fns.def("apparent_power", &fn::apparent_power, py::arg("phase") = 0);
    fns.def("reactive_power", &fn::reactive_power, py::arg("phase") = 0);
    fns.def("power_factor", &fn::power_factor, py::arg("phase") = 0);
    fns.def("corrected_power", &fn::corrected_power, py::arg("phase") = 0);
    fns.def("efficiency", &fn::efficiency, py::arg("phase") = 0);
    fns.def("phase_angle", &fn::phase_angle, py::arg("phase") = 0);
    fns.def("apparent_impedance", &fn::apparent_impedance, py::arg("phase") = 0);
    fns.def("series_resistance", &fn::series_resistance, py::arg("phase") = 0);
    fns.def("parallel_resistance", &fn::parallel_resistance, py::arg("phase") = 0);
    fns.def("series_reactance", &fn::series_reactance, py::arg("phase") = 0);
    fns.def("parallel_reactance", &fn::parallel_reactance, py::arg("phase") = 0);

    fns.def("frequency", &fn::frequency);
    fns.def("time_interval", &fn::time_interval);
    fns.def("time_relative", &fn::time_relative);

    fns.def("harmonic", &fn::harmonic, py::arg("function"));
    fns.def("minimum", &fn::minimum, py::arg("function"));
    fns.def("maximum", &fn::maximum, py::arg("function"));
    fns.def("integral", &fn::integral, py::arg("function"));
    fns.def("integral_positive", &fn::integral_positive, py::arg("function"));
    fns.def("integral_negative", &fn::integral_negative, py::arg("function"));

    // -- The instrument ---------------------------------------------------------
    // Blocking I/O methods release the GIL so other Python threads can run.
    using guard = py::call_guard<py::gil_scoped_release>;
    const std::vector<std::string> no_functions{};

    py::class_<NormaInstrument>(m, "Norma")
        .def(py::init([](const std::string& host, std::uint16_t port, double timeout) {
                 return NormaInstrument::connect(host, port, to_ms(timeout));
             }),
             py::arg("host"), py::arg("port") = kDefaultPort, py::arg("timeout") = 5.0,
             guard(),
             "Connect to the instrument over TCP (default port 23, timeout in seconds).")
        .def("close", &NormaInstrument::close, guard())
        .def_property_readonly("is_open", &NormaInstrument::is_open)
        .def("prepare", &NormaInstrument::prepare, guard(),
             "Clear the status registers, switch the transfer format to ASCii, enable "
             "concurrent functions and drain a stale error queue.")
        .def("__enter__", [](NormaInstrument& self) -> NormaInstrument& { return self; })
        .def("__exit__",
             [](NormaInstrument& self, const py::object&, const py::object&, const py::object&) {
                 self.close();
                 return false;
             })

        // Raw SCPI
        .def("write", &NormaInstrument::write, py::arg("scpi"), guard(),
             "Send a raw SCPI setting command, e.g. \"*RST\".")
        .def("query", &NormaInstrument::query, py::arg("scpi"), guard(),
             "Send a raw SCPI query, e.g. \"*IDN?\", and return the response line.")

        // IEEE 488.2
        .def("identify", &NormaInstrument::identify, guard())
        .def("reset", &NormaInstrument::reset, guard())
        .def("clear_status", &NormaInstrument::clear_status, guard())
        .def("options", &NormaInstrument::options, guard())
        .def("learn", &NormaInstrument::learn, guard())
        .def("wait_operation_complete",
             [](NormaInstrument& self, double timeout) {
                 self.wait_operation_complete(to_ms(timeout));
             },
             py::arg("timeout") = 30.0, guard())
        .def("set_operation_complete_flag", &NormaInstrument::set_operation_complete_flag, guard())
        .def("wait_pending_operations", &NormaInstrument::wait_pending_operations, guard())
        .def("trigger", &NormaInstrument::trigger, guard())
        .def("set_event_status_enable", &NormaInstrument::set_event_status_enable,
             py::arg("mask"), guard())
        .def("event_status_enable", &NormaInstrument::event_status_enable, guard())
        .def("event_status", &NormaInstrument::event_status, guard())
        .def("set_service_request_enable", &NormaInstrument::set_service_request_enable,
             py::arg("mask"), guard())
        .def("service_request_enable", &NormaInstrument::service_request_enable, guard())
        .def("status_byte", &NormaInstrument::status_byte, guard())
        .def("save_setup", &NormaInstrument::save_setup, py::arg("slot"), guard())
        .def("recall_setup", &NormaInstrument::recall_setup, py::arg("slot"), guard())

        // ROUTe
        .def("set_wiring_system", &NormaInstrument::set_wiring_system, py::arg("system"), guard())
        .def("wiring_system", &NormaInstrument::wiring_system, guard())

        // INPut
        .def("set_input_coupling", &NormaInstrument::set_input_coupling, py::arg("channel"),
             py::arg("coupling"), guard())
        .def("input_coupling", &NormaInstrument::input_coupling, py::arg("channel"), guard())
        .def("set_input_gain", &NormaInstrument::set_input_gain, py::arg("channel"),
             py::arg("gain"), guard())
        .def("input_gain", &NormaInstrument::input_gain, py::arg("channel"), guard())
        .def("set_input_filter", &NormaInstrument::set_input_filter, py::arg("channel"),
             py::arg("on") = true, guard())
        .def("input_filter", &NormaInstrument::input_filter, py::arg("channel"), guard())
        .def("input_filter_frequency", &NormaInstrument::input_filter_frequency,
             py::arg("channel"), guard())
        .def("set_input_shunt", &NormaInstrument::set_input_shunt, py::arg("channel"),
             py::arg("shunt"), guard())
        .def("input_shunt", &NormaInstrument::input_shunt, py::arg("channel"), guard())

        // SENSe: ranging, scaling, averaging
        .def("set_voltage_range", &NormaInstrument::set_voltage_range,
             py::arg("phase"), py::arg("volts"), guard())
        .def("voltage_range", &NormaInstrument::voltage_range, py::arg("phase"), guard())
        .def("set_voltage_autorange", &NormaInstrument::set_voltage_autorange,
             py::arg("phase"), py::arg("on") = true, guard())
        .def("voltage_autorange", &NormaInstrument::voltage_autorange, py::arg("phase"), guard())
        .def("voltage_ranges", &NormaInstrument::voltage_ranges, py::arg("phase"), guard())
        .def("set_voltage_scale", &NormaInstrument::set_voltage_scale, py::arg("phase"),
             py::arg("ratio"), guard())
        .def("voltage_scale", &NormaInstrument::voltage_scale, py::arg("phase"), guard())
        .def("set_current_range", &NormaInstrument::set_current_range,
             py::arg("phase"), py::arg("amps"), guard())
        .def("current_range", &NormaInstrument::current_range, py::arg("phase"), guard())
        .def("set_current_autorange", &NormaInstrument::set_current_autorange,
             py::arg("phase"), py::arg("on") = true, guard())
        .def("current_autorange", &NormaInstrument::current_autorange, py::arg("phase"), guard())
        .def("current_ranges", &NormaInstrument::current_ranges, py::arg("phase"), guard())
        .def("set_current_scale", &NormaInstrument::set_current_scale, py::arg("phase"),
             py::arg("ratio"), guard())
        .def("current_scale", &NormaInstrument::current_scale, py::arg("phase"), guard())
        .def("set_aperture", &NormaInstrument::set_aperture, py::arg("seconds"), guard())
        .def("aperture", &NormaInstrument::aperture, guard())
        .def("sampling_frequency", &NormaInstrument::sampling_frequency, guard())

        // SYNC
        .def("set_sync_enabled", &NormaInstrument::set_sync_enabled, py::arg("on") = true, guard())
        .def("sync_enabled", &NormaInstrument::sync_enabled, guard())
        .def("set_sync_source", &NormaInstrument::set_sync_source, py::arg("source"), guard())
        .def("sync_source", &NormaInstrument::sync_source, guard())
        .def("sync_to_voltage", &NormaInstrument::sync_to_voltage, py::arg("phase"), guard())
        .def("sync_to_current", &NormaInstrument::sync_to_current, py::arg("phase"), guard())
        .def("sync_external", &NormaInstrument::sync_external, guard())
        .def("set_sync_level", &NormaInstrument::set_sync_level, py::arg("level"), guard())
        .def("sync_level", &NormaInstrument::sync_level, guard())
        .def("set_sync_level_unit", &NormaInstrument::set_sync_level_unit, py::arg("unit"), guard())
        .def("sync_level_unit", &NormaInstrument::sync_level_unit, guard())
        .def("set_sync_slope", &NormaInstrument::set_sync_slope, py::arg("slope"), guard())
        .def("sync_slope", &NormaInstrument::sync_slope, guard())
        .def("set_sync_filter", &NormaInstrument::set_sync_filter, py::arg("on") = true, guard())
        .def("sync_filter", &NormaInstrument::sync_filter, guard())
        .def("set_sync_filter_frequency", &NormaInstrument::set_sync_filter_frequency,
             py::arg("hertz"), guard())
        .def("sync_filter_frequency", &NormaInstrument::sync_filter_frequency, guard())
        .def("set_sync_timeout", &NormaInstrument::set_sync_timeout, py::arg("seconds"), guard())
        .def("sync_timeout", &NormaInstrument::sync_timeout, guard())

        // SENSe: measurement functions
        .def("set_functions", &NormaInstrument::set_functions, py::arg("functions"), guard())
        .def("functions", &NormaInstrument::functions, guard())
        .def("enable_all_functions", &NormaInstrument::enable_all_functions, guard())
        .def("clear_functions", &NormaInstrument::clear_functions, guard())
        .def("function_count", &NormaInstrument::function_count, guard())
        .def("set_concurrent", &NormaInstrument::set_concurrent, py::arg("on") = true, guard())
        .def("concurrent", &NormaInstrument::concurrent, guard())

        // Acquisition
        .def("set_continuous", &NormaInstrument::set_continuous, py::arg("on") = true, guard())
        .def("continuous", &NormaInstrument::continuous, guard())
        .def("initiate", &NormaInstrument::initiate, guard())
        .def("initiate_sweep", &NormaInstrument::initiate_sweep, py::arg("block"), guard())
        .def("abort", &NormaInstrument::abort, guard())

        // TRIGger
        .def("set_trigger_start_source", &NormaInstrument::set_trigger_start_source,
             py::arg("source"), guard())
        .def("trigger_start_source", &NormaInstrument::trigger_start_source, guard())
        .def("set_trigger_start_level", &NormaInstrument::set_trigger_start_level,
             py::arg("level"), guard())
        .def("trigger_start_level", &NormaInstrument::trigger_start_level, guard())
        .def("set_trigger_start_slope", &NormaInstrument::set_trigger_start_slope,
             py::arg("slope"), guard())
        .def("trigger_start_slope", &NormaInstrument::trigger_start_slope, guard())
        .def("set_trigger_start_time", &NormaInstrument::set_trigger_start_time,
             py::arg("date"), py::arg("time"), guard())
        .def("set_trigger_stop_source", &NormaInstrument::set_trigger_stop_source,
             py::arg("source"), guard())
        .def("trigger_stop_source", &NormaInstrument::trigger_stop_source, guard())
        .def("set_trigger_stop_level", &NormaInstrument::set_trigger_stop_level,
             py::arg("level"), guard())
        .def("trigger_stop_level", &NormaInstrument::trigger_stop_level, guard())
        .def("set_trigger_stop_slope", &NormaInstrument::set_trigger_stop_slope,
             py::arg("slope"), guard())
        .def("trigger_stop_slope", &NormaInstrument::trigger_stop_slope, guard())
        .def("set_trigger_stop_time", &NormaInstrument::set_trigger_stop_time,
             py::arg("date"), py::arg("time"), guard())

        // Data
        .def("data", &NormaInstrument::data, py::arg("functions") = no_functions, guard())
        .def("data_with_status", &NormaInstrument::data_with_status,
             py::arg("functions") = no_functions, guard())

        // CALCulate: spectrum and harmonics
        .def("set_harmonic_order", &NormaInstrument::set_harmonic_order, py::arg("order"), guard())
        .def("harmonic_order", &NormaInstrument::harmonic_order, guard())
        .def("transform_once", &NormaInstrument::transform_once, guard())
        .def("set_transform_mode", &NormaInstrument::set_transform_mode, py::arg("mode"), guard())
        .def("transform_mode", &NormaInstrument::transform_mode, guard())
        .def("set_transform_functions", &NormaInstrument::set_transform_functions,
             py::arg("functions"), guard())
        .def("transform_functions", &NormaInstrument::transform_functions, guard())
        .def("set_transform_start", &NormaInstrument::set_transform_start, py::arg("hertz"),
             guard())
        .def("transform_start", &NormaInstrument::transform_start, guard())
        .def("set_transform_stop", &NormaInstrument::set_transform_stop, py::arg("hertz"), guard())
        .def("transform_stop", &NormaInstrument::transform_stop, guard())
        .def("set_transform_cycles", &NormaInstrument::set_transform_cycles, py::arg("cycles"),
             guard())
        .def("transform_cycles", &NormaInstrument::transform_cycles, guard())
        .def("set_transform_grouping", &NormaInstrument::set_transform_grouping,
             py::arg("grouping"), guard())
        .def("transform_grouping", &NormaInstrument::transform_grouping, guard())
        .def("transform_data", &NormaInstrument::transform_data, py::arg("count") = 0,
             py::arg("offset") = 0, guard())
        .def("transform_preamble", &NormaInstrument::transform_preamble, guard())
        .def("transform_thd", &NormaInstrument::transform_thd, guard())

        // CALCulate: integration
        .def("set_integral_enabled", &NormaInstrument::set_integral_enabled,
             py::arg("on") = true, guard())
        .def("integral_enabled", &NormaInstrument::integral_enabled, guard())
        .def("set_integral_functions", &NormaInstrument::set_integral_functions,
             py::arg("functions"), guard())
        .def("integral_functions", &NormaInstrument::integral_functions, guard())
        .def("clear_integral", &NormaInstrument::clear_integral, guard())
        .def("set_integral_auto_clear", &NormaInstrument::set_integral_auto_clear,
             py::arg("on") = true, guard())
        .def("integral_auto_clear", &NormaInstrument::integral_auto_clear, guard())
        .def("set_integral_start_source", &NormaInstrument::set_integral_start_source,
             py::arg("source"), guard())
        .def("integral_start_source", &NormaInstrument::integral_start_source, guard())
        .def("start_integral", &NormaInstrument::start_integral, guard())
        .def("set_integral_start_time", &NormaInstrument::set_integral_start_time,
             py::arg("date"), py::arg("time"), guard())
        .def("set_integral_stop_source", &NormaInstrument::set_integral_stop_source,
             py::arg("source"), guard())
        .def("integral_stop_source", &NormaInstrument::integral_stop_source, guard())
        .def("stop_integral", &NormaInstrument::stop_integral, guard())
        .def("set_integral_stop_time", &NormaInstrument::set_integral_stop_time,
             py::arg("date"), py::arg("time"), guard())
        .def("set_integral_stop_interval", &NormaInstrument::set_integral_stop_interval,
             py::arg("seconds"), guard())
        .def("integral_stop_interval", &NormaInstrument::integral_stop_interval, guard())

        // CALCulate: power
        .def("set_power_correction", &NormaInstrument::set_power_correction,
             py::arg("correction"), guard())
        .def("power_correction", &NormaInstrument::power_correction, guard())
        .def("set_efficiency_reference", &NormaInstrument::set_efficiency_reference,
             py::arg("input"), py::arg("output"), py::arg("phase") = 0, guard())

        // Memory recording
        .def("set_sweep_enabled", &NormaInstrument::set_sweep_enabled, py::arg("block"),
             py::arg("on") = true, guard())
        .def("sweep_enabled", &NormaInstrument::sweep_enabled, py::arg("block"), guard())
        .def("set_sweep_time", &NormaInstrument::set_sweep_time, py::arg("block"),
             py::arg("seconds"), guard())
        .def("set_sweep_time_max", &NormaInstrument::set_sweep_time_max, py::arg("block"), guard())
        .def("sweep_time", &NormaInstrument::sweep_time, py::arg("block"), guard())
        .def("set_sweep_offset_time", &NormaInstrument::set_sweep_offset_time, py::arg("block"),
             py::arg("seconds"), guard())
        .def("sweep_offset_time", &NormaInstrument::sweep_offset_time, py::arg("block"), guard())
        .def("sweep_points", &NormaInstrument::sweep_points, py::arg("block"), guard())
        .def("sweep_offset_points", &NormaInstrument::sweep_offset_points, py::arg("block"),
             guard())
        .def("set_sweep_count", &NormaInstrument::set_sweep_count, py::arg("block"),
             py::arg("count"), guard())
        .def("sweep_count", &NormaInstrument::sweep_count, py::arg("block"), guard())
        .def("set_sweep_sparsing", &NormaInstrument::set_sweep_sparsing, py::arg("block"),
             py::arg("factor"), guard())
        .def("sweep_sparsing", &NormaInstrument::sweep_sparsing, py::arg("block"), guard())
        .def("set_sweep_functions", &NormaInstrument::set_sweep_functions, py::arg("block"),
             py::arg("functions"), guard())
        .def("sweep_functions", &NormaInstrument::sweep_functions, py::arg("block"), guard())
        .def("trace_preamble", &NormaInstrument::trace_preamble, py::arg("block"), guard())
        .def("trace_data", &NormaInstrument::trace_data, py::arg("block"), py::arg("count") = 0,
             py::arg("offset") = 0, py::arg("sparsing") = 0, guard())
        .def("trace_status", &NormaInstrument::trace_status, py::arg("block"),
             py::arg("count") = 0, py::arg("offset") = 0, py::arg("sparsing") = 0, guard())
        .def("trace_free", &NormaInstrument::trace_free, guard())
        .def("trace_length", &NormaInstrument::trace_length, guard())
        .def("delete_traces", &NormaInstrument::delete_traces, guard())

        // FORMat
        .def("set_data_format", &NormaInstrument::set_data_format, py::arg("format"),
             py::arg("length") = 0, guard())
        .def("data_format", &NormaInstrument::data_format, guard())
        .def("set_status_format", &NormaInstrument::set_status_format, py::arg("format"),
             py::arg("length") = 0, guard())
        .def("status_format", &NormaInstrument::status_format, guard())
        .def("set_byte_order", &NormaInstrument::set_byte_order, py::arg("order"), guard())
        .def("byte_order", &NormaInstrument::byte_order, guard())
        .def("set_transpose", &NormaInstrument::set_transpose, py::arg("on") = true, guard())
        .def("transpose", &NormaInstrument::transpose, guard())

        // DISPlay and OUTPut
        .def("set_display_enabled", &NormaInstrument::set_display_enabled, py::arg("on") = true,
             guard())
        .def("display_enabled", &NormaInstrument::display_enabled, guard())
        .def("set_display_functions", &NormaInstrument::set_display_functions,
             py::arg("functions"), guard())
        .def("display_functions", &NormaInstrument::display_functions, guard())
        .def("set_output_enabled", &NormaInstrument::set_output_enabled, py::arg("on") = true,
             guard())
        .def("output_enabled", &NormaInstrument::output_enabled, guard())

        // SYSTem
        .def("scpi_version", &NormaInstrument::scpi_version, guard())
        .def("set_key_lock", &NormaInstrument::set_key_lock, py::arg("lock"), guard())
        .def("key_lock", &NormaInstrument::key_lock, guard())
        .def("set_date", &NormaInstrument::set_date, py::arg("date"), guard())
        .def("date", &NormaInstrument::date, guard())
        .def("set_time", &NormaInstrument::set_time, py::arg("time"), guard())
        .def("time_of_day", &NormaInstrument::time_of_day, guard())
        .def("set_gpib_address", &NormaInstrument::set_gpib_address, py::arg("address"), guard())
        .def("gpib_address", &NormaInstrument::gpib_address, guard())
        .def("set_serial_baud", &NormaInstrument::set_serial_baud, py::arg("baud"), guard())
        .def("serial_baud", &NormaInstrument::serial_baud, guard())
        .def("set_language", &NormaInstrument::set_language, py::arg("language"), guard())
        .def("language", &NormaInstrument::language, guard())

        // TIMer
        .def("reset_timer", &NormaInstrument::reset_timer, guard())
        .def("timer_reset_time", &NormaInstrument::timer_reset_time, guard())

        // STATus
        .def("status", &NormaInstrument::status, py::arg("register"), py::arg("part"), guard())
        .def("set_status", &NormaInstrument::set_status, py::arg("register"), py::arg("part"),
             py::arg("mask"), guard())
        .def("status_operation_condition", &NormaInstrument::status_operation_condition, guard())

        // Errors & status
        .def("read_errors", &NormaInstrument::read_errors, guard())
        .def("read_errors_at_once", &NormaInstrument::read_errors_at_once, guard())
        .def("check_errors", &NormaInstrument::check_errors, guard(),
             "Raise ScpiError if the instrument error queue is non-empty.")

        // Plumbing
        .def("set_timeout",
             [](NormaInstrument& self, double timeout) { self.set_timeout(to_ms(timeout)); },
             py::arg("timeout"), "Set the default I/O timeout in seconds.")
        .def_property_readonly("timeout", [](const NormaInstrument& self) {
            return std::chrono::duration<double>(self.timeout()).count();
        });
}

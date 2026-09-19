#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include <chrono>
#include <memory>

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
        .def("__repr__", [](const Reading& r) {
            return "Reading(" + std::to_string(r.values.size()) + " values)";
        });

    py::enum_<WiringSystem>(m, "WiringSystem")
        .value("THREE_WATTMETER", WiringSystem::ThreeWattmeter)
        .value("TWO_WATTMETER", WiringSystem::TwoWattmeter);

    py::enum_<Coupling>(m, "Coupling")
        .value("AC", Coupling::AC)
        .value("DC", Coupling::DC);

    py::enum_<Shunt>(m, "Shunt")
        .value("INTERNAL", Shunt::Internal)
        .value("EXTERNAL", Shunt::External);

    // Bit flags of Reading.status entries.
    auto status = m.def_submodule("measurement_status", "Bit flags for Reading.status");
    status.attr("NORMAL") = measurement_status::kNormal;
    status.attr("UNDERRANGE") = measurement_status::kUnderrange;
    status.attr("OVERRANGE") = measurement_status::kOverrange;
    status.attr("UNDEFINED") = measurement_status::kUndefined;
    status.attr("NOT_AVAILABLE") = measurement_status::kNotAvailable;
    status.attr("POWER_FACTOR_CAPACITIVE") = measurement_status::kPowerFactorCapacitive;

    // -- <function> name helpers ----------------------------------------------
    auto fns = m.def_submodule("fn", "Builders for SENSe <function> names");
    fns.def("voltage", &fn::voltage, py::arg("phase") = 0);
    fns.def("voltage_ac", &fn::voltage_ac, py::arg("phase") = 0);
    fns.def("voltage_mean", &fn::voltage_mean, py::arg("phase") = 0);
    fns.def("current", &fn::current, py::arg("phase") = 0);
    fns.def("current_ac", &fn::current_ac, py::arg("phase") = 0);
    fns.def("current_mean", &fn::current_mean, py::arg("phase") = 0);
    fns.def("active_power", &fn::active_power, py::arg("phase") = 0);
    fns.def("apparent_power", &fn::apparent_power, py::arg("phase") = 0);
    fns.def("reactive_power", &fn::reactive_power, py::arg("phase") = 0);
    fns.def("power_factor", &fn::power_factor, py::arg("phase") = 0);
    fns.def("phase_angle", &fn::phase_angle, py::arg("phase") = 0);
    fns.def("frequency", &fn::frequency);
    fns.def("time_interval", &fn::time_interval);

    // -- The instrument ---------------------------------------------------------
    // Blocking I/O methods release the GIL so other Python threads can run.
    using guard = py::call_guard<py::gil_scoped_release>;

    py::class_<NormaInstrument>(m, "Norma")
        .def(py::init([](const std::string& host, std::uint16_t port, double timeout) {
                 return NormaInstrument::connect(host, port, to_ms(timeout));
             }),
             py::arg("host"), py::arg("port") = kDefaultPort, py::arg("timeout") = 5.0,
             guard(),
             "Connect to the instrument over TCP (default port 23, timeout in seconds).")
        .def("close", &NormaInstrument::close, guard())
        .def_property_readonly("is_open", &NormaInstrument::is_open)
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
        .def("scpi_version", &NormaInstrument::scpi_version, guard())
        .def("wait_operation_complete",
             [](NormaInstrument& self, double timeout) {
                 self.wait_operation_complete(to_ms(timeout));
             },
             py::arg("timeout") = 30.0, guard())
        .def("trigger", &NormaInstrument::trigger, guard())

        // Configuration
        .def("set_wiring_system", &NormaInstrument::set_wiring_system, py::arg("system"), guard())
        .def("wiring_system", &NormaInstrument::wiring_system, guard())
        .def("set_sync_source", &NormaInstrument::set_sync_source, py::arg("source"), guard())
        .def("sync_to_voltage", &NormaInstrument::sync_to_voltage, py::arg("phase"), guard())
        .def("sync_to_current", &NormaInstrument::sync_to_current, py::arg("phase"), guard())
        .def("sync_external", &NormaInstrument::sync_external, guard())
        .def("set_voltage_range", &NormaInstrument::set_voltage_range,
             py::arg("phase"), py::arg("volts"), guard())
        .def("set_voltage_autorange", &NormaInstrument::set_voltage_autorange,
             py::arg("phase"), py::arg("on") = true, guard())
        .def("set_current_range", &NormaInstrument::set_current_range,
             py::arg("phase"), py::arg("amps"), guard())
        .def("set_current_autorange", &NormaInstrument::set_current_autorange,
             py::arg("phase"), py::arg("on") = true, guard())
        .def("set_aperture", &NormaInstrument::set_aperture, py::arg("seconds"), guard())
        .def("aperture", &NormaInstrument::aperture, guard())
        .def("set_functions", &NormaInstrument::set_functions, py::arg("functions"), guard())
        .def("functions", &NormaInstrument::functions, guard())
        .def("function_count", &NormaInstrument::function_count, guard())
        .def("clear_functions", &NormaInstrument::clear_functions, guard())

        // Acquisition
        .def("set_continuous", &NormaInstrument::set_continuous, py::arg("on") = true, guard())
        .def("initiate", &NormaInstrument::initiate, guard())
        .def("abort", &NormaInstrument::abort, guard())
        .def("data", &NormaInstrument::data,
             py::arg("functions") = std::vector<std::string>{}, guard())
        .def("data_with_status", &NormaInstrument::data_with_status,
             py::arg("functions") = std::vector<std::string>{}, guard())

        // Errors & status
        .def("read_errors", &NormaInstrument::read_errors, guard())
        .def("check_errors", &NormaInstrument::check_errors, guard(),
             "Raise ScpiError if the instrument error queue is non-empty.")
        .def("status_operation_condition", &NormaInstrument::status_operation_condition, guard())

        // Plumbing
        .def("set_timeout",
             [](NormaInstrument& self, double timeout) { self.set_timeout(to_ms(timeout)); },
             py::arg("timeout"), "Set the default I/O timeout in seconds.")
        .def_property_readonly("timeout", [](const NormaInstrument& self) {
            return std::chrono::duration<double>(self.timeout()).count();
        });
}

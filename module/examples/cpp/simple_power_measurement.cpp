// U, I, P measurement on a three-phase system — the TCP equivalent of the
// "Perform Simple Power Measurement" example in the manual:
//
//   *RST -> ROUT:SYST "3W" -> SYNC:SOUR VOLT1 -> ranges -> APER 1.0
//        -> FUNC "VOLT1","CURR1","POW1:ACT" -> INIT:CONT ON -> DATA?
//
// Usage: norma_simple_power [host]

#include <chrono>
#include <cstdio>
#include <exception>
#include <string>
#include <thread>

#include <fluke/norma/norma.hpp>

int main(int argc, char** argv) {
    using namespace fluke::norma;

    const std::string host = argc > 1 ? argv[1] : "192.168.1.100";

    try {
        auto norma = NormaInstrument::connect(host);
        std::printf("Connected to %s\n", norma.identify().model.c_str());

        norma.reset();
        norma.set_wiring_system(WiringSystem::ThreeWattmeter);
        norma.sync_to_voltage(1);
        norma.set_voltage_range(1, 300.0);
        norma.set_current_autorange(1, true);
        norma.set_aperture(1.0); // averaging time: 1 s

        const std::vector<std::string> functions = {
            fn::voltage(1),      // "VOLT1"
            fn::current(1),      // "CURR1"
            fn::active_power(1), // "POW1:ACT"
        };
        norma.set_functions(functions);
        norma.set_continuous(true);
        norma.check_errors(); // raise early if the configuration was rejected

        // DATA? does not wait for the averaging interval; give the instrument
        // time to produce the first complete measurement.
        std::this_thread::sleep_for(std::chrono::seconds(2));

        const Reading reading = norma.data_with_status(functions);
        static const char* names[] = {"U1 [V]", "I1 [A]", "P1 [W]"};
        for (std::size_t i = 0; i < reading.values.size() && i < 3; ++i) {
            std::printf("%-8s %12.6g  (status %d)\n", names[i], reading.values[i],
                        reading.status[i]);
        }
        return 0;
    } catch (const std::exception& e) {
        std::fprintf(stderr, "Error: %s\n", e.what());
        return 1;
    }
}

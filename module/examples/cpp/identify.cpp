// Minimal smoke test: connect and ask the instrument who it is.
//
// Usage: norma_identify [host]   (default host 192.168.1.100, TCP port 23)

#include <cstdio>
#include <exception>
#include <string>

#include <fluke/norma/norma.hpp>

int main(int argc, char** argv) {
    const std::string host = argc > 1 ? argv[1] : "192.168.1.100";

    try {
        auto norma = fluke::norma::NormaInstrument::connect(host);

        const auto id = norma.identify();
        std::printf("Manufacturer: %s\n", id.manufacturer.c_str());
        std::printf("Model:        %s\n", id.model.c_str());
        std::printf("Serial:       %s\n", id.serial_number.c_str());
        std::printf("Firmware:     %s\n", id.firmware_version.c_str());
        std::printf("SCPI version: %s\n", norma.scpi_version().c_str());
        std::printf("Options:      %s\n", norma.options().c_str());
        return 0;
    } catch (const std::exception& e) {
        std::fprintf(stderr, "Error: %s\n", e.what());
        return 1;
    }
}

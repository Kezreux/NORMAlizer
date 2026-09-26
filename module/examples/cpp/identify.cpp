// Minimal smoke test: connect and ask the instrument who it is.
//
// Usage: norma_identify [host] [port]   (default 192.168.1.100, TCP port 23)
//
// With no instrument on the bench, start the simulator and point this at it:
//
//   norma_sim --port 2300
//   norma_identify 127.0.0.1 2300

#include <cstdio>
#include <cstdlib>
#include <exception>
#include <string>

#include <fluke/norma/norma.hpp>

int main(int argc, char** argv) {
    const std::string host = argc > 1 ? argv[1] : "192.168.1.100";
    const auto port = static_cast<std::uint16_t>(
        argc > 2 ? std::atoi(argv[2]) : fluke::norma::kDefaultPort);

    try {
        auto norma = fluke::norma::NormaInstrument::connect(host, port);

        const auto id = norma.identify();
        std::printf("Manufacturer: %s\n", id.manufacturer.c_str());
        std::printf("Model:        %s\n", id.model.c_str());
        std::printf("Serial:       %s\n", id.serial_number.c_str());
        std::printf("Firmware:     %s\n", id.firmware_version.c_str());
        std::printf("SCPI version: %s\n", norma.scpi_version().c_str());
        std::printf("Options:      %s\n", norma.options().c_str());

        const auto format = norma.data_format();
        std::printf("Data format:  %s,%d%s\n",
                    format.format == fluke::norma::DataFormat::Ascii ? "ASCii" : "binary",
                    format.length,
                    format.format == fluke::norma::DataFormat::Ascii
                        ? ""
                        : "   (call prepare() before reading measurements)");
        return 0;
    } catch (const std::exception& e) {
        std::fprintf(stderr, "Error: %s\n", e.what());
        return 1;
    }
}

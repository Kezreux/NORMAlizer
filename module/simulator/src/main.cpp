// norma_sim — a Fluke NORMA 4000/5000 on loopback, for developing against
// without an instrument on the bench.
//
//   norma_sim                       listen on 127.0.0.1, port chosen by the OS
//   norma_sim --port 2300           listen on a fixed port
//   norma_sim --host 0.0.0.0        reachable from other machines
//   norma_sim --verbose             log every command line and response
//
// The bound port is printed as "listening on <host>:<port>" on the first line
// of stdout, so a test harness can start the process and read the port.

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <csignal>
#include <string>
#include <thread>

#include <fluke/norma/simulator/simulator_server.hpp>

namespace {

std::atomic<bool> g_stop{false};

void on_signal(int) { g_stop = true; }

void print_usage() {
    std::puts("usage: norma_sim [--host <address>] [--port <port>] [--verbose]\n"
              "\n"
              "Serves a simulated Fluke NORMA 4000/5000 over TCP. Point a client at the\n"
              "printed host:port; the SCPI command set is the one in\n"
              "docs/Fluke-NORMA-TCP-API.md.");
}

} // namespace

int main(int argc, char** argv) {
    std::string host = "127.0.0.1";
    int port = 0;
    bool verbose = false;

    for (int i = 1; i < argc; ++i) {
        const std::string argument = argv[i];
        const bool has_value = i + 1 < argc;
        if ((argument == "--host" || argument == "-h") && has_value) {
            host = argv[++i];
        } else if ((argument == "--port" || argument == "-p") && has_value) {
            port = std::atoi(argv[++i]);
        } else if (argument == "--verbose" || argument == "-v") {
            verbose = true;
        } else if (argument == "--help") {
            print_usage();
            return 0;
        } else {
            std::fprintf(stderr, "norma_sim: unrecognized argument \"%s\"\n\n", argument.c_str());
            print_usage();
            return 2;
        }
    }
    if (port < 0 || port > 65535) {
        std::fprintf(stderr, "norma_sim: port must be 0..65535\n");
        return 2;
    }

    try {
        auto simulator = std::make_shared<fluke::norma::sim::NormaSimulator>();
        fluke::norma::sim::SimulatorServer server(simulator, static_cast<std::uint16_t>(port),
                                                  host);

        std::printf("listening on %s:%u\n", server.host().c_str(), server.port());
        const auto& id = simulator->options().identification;
        std::printf("identifies as %s,%s,%s,%s\n", id.manufacturer.c_str(), id.model.c_str(),
                    id.serial_number.c_str(), id.firmware_version.c_str());
        std::fflush(stdout);

        std::signal(SIGINT, on_signal);
        std::signal(SIGTERM, on_signal);

        std::uint64_t reported_connections = 0;
        std::uint64_t reported_lines = 0;
        while (!g_stop) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            if (!verbose) {
                continue;
            }
            if (server.connections() != reported_connections) {
                reported_connections = server.connections();
                std::printf("client connected (%llu total)\n",
                            static_cast<unsigned long long>(reported_connections));
                std::fflush(stdout);
            }
            const std::uint64_t lines = simulator->handled_lines();
            if (lines != reported_lines) {
                std::printf("handled %llu command lines\n",
                            static_cast<unsigned long long>(lines));
                reported_lines = lines;
                std::fflush(stdout);
            }
        }

        std::puts("stopping");
        server.stop();
        return 0;
    } catch (const std::exception& e) {
        std::fprintf(stderr, "norma_sim: %s\n", e.what());
        return 1;
    }
}

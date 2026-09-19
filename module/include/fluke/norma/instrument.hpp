#pragma once

#include <chrono>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "fluke/norma/scpi_client.hpp"
#include "fluke/norma/transport.hpp"
#include "fluke/norma/types.hpp"

namespace fluke::norma {

/// High-level facade over the Fluke NORMA 4000/5000 remote-control API.
///
/// Covers the typical measurement workflow from the manual:
///   *RST -> configure (ROUT:SYST, SYNC:SOUR, ranges, APER, FUNC)
///        -> INIT:CONT ON -> DATA? / DATA:STAT?
///
/// Everything else in the command reference is reachable through the raw
/// write()/query() escape hatches (or scpi() for parsing helpers).
class NormaInstrument {
public:
    explicit NormaInstrument(std::unique_ptr<Transport> transport,
                             std::chrono::milliseconds timeout = std::chrono::milliseconds(5000));

    /// Connects over TCP/Ethernet (port fixed to 23 on the instrument).
    static NormaInstrument connect(std::string host,
                                   std::uint16_t port = kDefaultPort,
                                   std::chrono::milliseconds timeout = std::chrono::milliseconds(5000));

    void close();
    bool is_open() const;

    // -- Raw SCPI escape hatches ---------------------------------------------
    void write(std::string_view scpi);
    std::string query(std::string_view scpi);

    // -- IEEE 488.2 common commands ------------------------------------------
    Identification identify();                       ///< *IDN?
    void reset();                                    ///< *RST
    void clear_status();                             ///< *CLS
    std::string options();                           ///< *OPT?
    std::string scpi_version();                      ///< SYSTem:VERSion?

    /// *OPC? — blocks until all previous commands have completed.
    void wait_operation_complete(std::chrono::milliseconds timeout = std::chrono::milliseconds(30000));
    void trigger();                                  ///< *TRG

    // -- Configuration ---------------------------------------------------------
    void set_wiring_system(WiringSystem system);     ///< ROUTe:SYSTem "3W"|"2W"
    WiringSystem wiring_system();                    ///< ROUTe:SYSTem?

    void set_sync_source(std::string_view source);   ///< SYNC:SOURce <source>
    void sync_to_voltage(int phase);                 ///< SYNC:SOURce VOLTage<phase>
    void sync_to_current(int phase);                 ///< SYNC:SOURce CURRent<phase>
    void sync_external();                            ///< SYNC:SOURce EXTernal

    void set_voltage_range(int phase, double volts); ///< VOLT<n>:RANGe (disables autorange)
    void set_voltage_autorange(int phase, bool on);  ///< VOLT<n>:RANGe:AUTO
    void set_current_range(int phase, double amps);  ///< CURR<n>:RANGe
    void set_current_autorange(int phase, bool on);  ///< CURR<n>:RANGe:AUTO

    void set_aperture(double seconds);               ///< APERture (averaging time, 0.015..3600 s)
    double aperture();                               ///< APERture?

    /// SENSe:FUNCtion "f1","f2",... — see fn:: helpers in types.hpp.
    void set_functions(const std::vector<std::string>& functions);
    std::vector<std::string> functions();            ///< SENSe:FUNCtion?
    int function_count();                            ///< SENSe:FUNCtion:COUNt?
    void clear_functions();                          ///< SENSe:FUNCtion:OFF:ALL

    // -- Acquisition -----------------------------------------------------------
    void set_continuous(bool on);                    ///< INITiate:CONTinuous
    void initiate();                                 ///< INITiate (single shot)
    void abort();                                    ///< ABORt

    /// DATA? — averaged values of the configured (or given) functions.
    std::vector<double> data(const std::vector<std::string>& functions = {});

    /// DATA:STATus? — values plus one status flag per value
    /// (see measurement_status:: bit constants).
    Reading data_with_status(const std::vector<std::string>& functions = {});

    // -- Errors & status --------------------------------------------------------
    std::vector<ScpiErrorInfo> read_errors();        ///< drains SYST:ERR?
    void check_errors();                             ///< throws ScpiError if queue non-empty
    int status_operation_condition();                ///< STATus:OPERation:CONDition?

    // -- Plumbing ---------------------------------------------------------------
    void set_timeout(std::chrono::milliseconds timeout);
    std::chrono::milliseconds timeout() const;
    ScpiClient& scpi() { return *scpi_; }

private:
    static void check_phase(int phase);

    std::unique_ptr<ScpiClient> scpi_;
};

} // namespace fluke::norma

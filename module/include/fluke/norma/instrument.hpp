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
/// and, beyond it, the INPut, SENSe, SYNC, FORMat, SYSTem, STATus, CALCulate
/// and TRACe subsystems. Anything not wrapped here is still reachable through
/// the raw write()/query() escape hatches (or scpi() for the parsing helpers).
///
/// Method names follow the subsystem they belong to, and the doc comment on
/// each one gives the exact SCPI command so it can be looked up in
/// docs/Fluke-NORMA-TCP-API.md.
///
/// Not thread-safe as a whole: individual commands are serialized by
/// ScpiClient's mutex, but a configure-then-read sequence from two threads can
/// still interleave.
class NormaInstrument {
public:
    explicit NormaInstrument(std::unique_ptr<Transport> transport,
                             std::chrono::milliseconds timeout = std::chrono::milliseconds(5000));

    /// Connects over TCP/Ethernet (port fixed to 23 on the instrument).
    ///
    /// Deliberately leaves the instrument's settings alone — it may already be
    /// configured for a running measurement. Call prepare() (or reset()) first
    /// if you need known-good defaults.
    static NormaInstrument connect(std::string host,
                                   std::uint16_t port = kDefaultPort,
                                   std::chrono::milliseconds timeout = std::chrono::milliseconds(5000));

    void close();
    bool is_open() const;

    /// Brings the instrument into the state this library can talk to, without
    /// disturbing the measurement configuration: clears the status registers,
    /// switches the transfer format to ASCii (this library cannot parse the
    /// REAL/INTeger block formats), enables concurrent functions, and drains
    /// any stale error queue. Safe to call on a connection whose previous
    /// owner left FORMat on REAL.
    void prepare();

    // -- Raw SCPI escape hatches ---------------------------------------------
    /// Sends one command line. Must not contain a line terminator; several
    /// commands can be combined with ';' as described in the manual.
    void write(std::string_view scpi);
    std::string query(std::string_view scpi);

    // -- IEEE 488.2 common commands ------------------------------------------
    Identification identify();                       ///< *IDN?
    void reset();                                    ///< *RST
    void clear_status();                             ///< *CLS
    std::string options();                           ///< *OPT?
    std::string learn();                             ///< *LRN?

    /// *OPC? — blocks until all previous commands have completed.
    void wait_operation_complete(std::chrono::milliseconds timeout = std::chrono::milliseconds(30000));
    void set_operation_complete_flag();              ///< *OPC (sets the ESR bit, does not block)
    void wait_pending_operations();                  ///< *WAI
    void trigger();                                  ///< *TRG

    void set_event_status_enable(int mask);          ///< *ESE
    int event_status_enable();                       ///< *ESE?
    int event_status();                              ///< *ESR? (clears on read)
    void set_service_request_enable(int mask);       ///< *SRE
    int service_request_enable();                    ///< *SRE?
    int status_byte();                               ///< *STB? (see status_byte:: bits)

    void save_setup(int slot);                       ///< *SAV (slots 10..24)
    void recall_setup(int slot);                     ///< *RCL (slots 1, 2, 10..24)

    // -- ROUTe ----------------------------------------------------------------
    void set_wiring_system(WiringSystem system);     ///< ROUTe:SYSTem "3W"|"2W"
    WiringSystem wiring_system();                    ///< ROUTe:SYSTem?

    // -- INPut (hardware channels 1..12, not phases) --------------------------
    // The electrical INPut subsystem addresses hardware channels: the current
    // inputs are the odd numbers and the voltage inputs the even ones (see the
    // STATus:QUEStionable:CURRent/VOLTage tables). GAIN and SHUNt apply to
    // current channels only.
    void set_input_coupling(int channel, Coupling coupling); ///< INPut<n>:COUPling
    Coupling input_coupling(int channel);                    ///< INPut<n>:COUPling?
    void set_input_gain(int channel, double gain);           ///< INPut<n>:GAIN (external shunt factor)
    double input_gain(int channel);                          ///< INPut<n>:GAIN?
    void set_input_filter(int channel, bool on);             ///< INPut<n>:FILTer:STATe
    bool input_filter(int channel);                          ///< INPut<n>:FILTer:STATe?
    double input_filter_frequency(int channel);              ///< INPut<n>:FILTer:LPASs:FREQuency?
    void set_input_shunt(int channel, Shunt shunt);          ///< INPut<n>:SHUNt
    Shunt input_shunt(int channel);                          ///< INPut<n>:SHUNt?

    // -- SENSe: ranging, scaling, averaging -----------------------------------
    void set_voltage_range(int phase, double volts); ///< VOLT<n>:RANGe (disables autorange)
    double voltage_range(int phase);                 ///< VOLT<n>:RANGe?
    void set_voltage_autorange(int phase, bool on);  ///< VOLT<n>:RANGe:AUTO
    bool voltage_autorange(int phase);               ///< VOLT<n>:RANGe:AUTO?
    std::vector<double> voltage_ranges(int phase);   ///< VOLT<n>:RANGe:LIST? (selectable ranges)
    void set_voltage_scale(int phase, double ratio); ///< VOLT<n>:SCALe (transducer ratio)
    double voltage_scale(int phase);                 ///< VOLT<n>:SCALe?

    void set_current_range(int phase, double amps);  ///< CURR<n>:RANGe
    double current_range(int phase);                 ///< CURR<n>:RANGe?
    void set_current_autorange(int phase, bool on);  ///< CURR<n>:RANGe:AUTO
    bool current_autorange(int phase);               ///< CURR<n>:RANGe:AUTO?
    std::vector<double> current_ranges(int phase);   ///< CURR<n>:RANGe:LIST?
    void set_current_scale(int phase, double ratio); ///< CURR<n>:SCALe
    double current_scale(int phase);                 ///< CURR<n>:SCALe?

    /// APERture — averaging time in seconds (kMinAperture..kMaxAperture).
    void set_aperture(double seconds);
    double aperture();                               ///< APERture?

    double sampling_frequency();                     ///< SWEep:FREQuency?

    // -- SYNC -----------------------------------------------------------------
    void set_sync_enabled(bool on);                  ///< SYNC:STATe
    bool sync_enabled();                             ///< SYNC:STATe?
    void set_sync_source(std::string_view source);   ///< SYNC:SOURce <source>
    std::string sync_source();                       ///< SYNC:SOURce?
    void sync_to_voltage(int phase);                 ///< SYNC:SOURce VOLTage<phase>
    void sync_to_current(int phase);                 ///< SYNC:SOURce CURRent<phase>
    void sync_external();                            ///< SYNC:SOURce EXTernal
    void set_sync_level(double level);               ///< SYNC:SOURce:LEVel
    double sync_level();                             ///< SYNC:SOURce:LEVel?
    void set_sync_level_unit(LevelUnit unit);        ///< SYNC:LEVel:UNIT
    LevelUnit sync_level_unit();                     ///< SYNC:LEVel:UNIT?
    void set_sync_slope(Slope slope);                ///< SYNC:SOURce:SLOPe
    Slope sync_slope();                              ///< SYNC:SOURce:SLOPe?
    void set_sync_filter(bool on);                   ///< SYNC:SOURce:FILTer:LPASs:STATe
    bool sync_filter();                              ///< SYNC:SOURce:FILTer:LPASs:STATe?
    void set_sync_filter_frequency(double hertz);    ///< SYNC:SOURce:FILTer:LPASs:FREQuency (100|1e3|10e3)
    double sync_filter_frequency();                  ///< SYNC:SOURce:FILTer:LPASs:FREQuency?
    void set_sync_timeout(double seconds);           ///< SYNC:TIMeout
    double sync_timeout();                           ///< SYNC:TIMeout?

    // -- SENSe: measurement functions ------------------------------------------
    /// SENSe:FUNCtion "f1","f2",... — see the fn:: helpers in types.hpp.
    void set_functions(const std::vector<std::string>& functions);
    std::vector<std::string> functions();            ///< SENSe:FUNCtion?
    void enable_all_functions();                     ///< SENSe:FUNCtion:ON:ALL
    void clear_functions();                          ///< SENSe:FUNCtion:OFF:ALL
    int function_count();                            ///< SENSe:FUNCtion:COUNt?
    void set_concurrent(bool on);                    ///< SENSe:FUNCtion:CONCurrent
    bool concurrent();                               ///< SENSe:FUNCtion:CONCurrent?

    // -- Acquisition -----------------------------------------------------------
    void set_continuous(bool on);                    ///< INITiate:CONTinuous
    bool continuous();                               ///< INITiate:CONTinuous?
    void initiate();                                 ///< INITiate (single shot)
    void initiate_sweep(SweepBlock block);           ///< INITiate:SEQuence1|2 (memory recording)
    void abort();                                    ///< ABORt

    void set_trigger_start_source(std::string_view source); ///< TRIGger:STARt:SOURce
    std::string trigger_start_source();              ///< TRIGger:STARt:SOURce?
    void set_trigger_start_level(double level);      ///< TRIGger:STARt:LEVel
    double trigger_start_level();                    ///< TRIGger:STARt:LEVel?
    void set_trigger_start_slope(Slope slope);       ///< TRIGger:STARt:SLOPe
    Slope trigger_start_slope();                     ///< TRIGger:STARt:SLOPe?
    void set_trigger_start_time(const Date& date, const Time& time); ///< TRIGger:STARt:TIME
    void set_trigger_stop_source(std::string_view source);  ///< TRIGger:STOP:SOURce
    std::string trigger_stop_source();               ///< TRIGger:STOP:SOURce?
    void set_trigger_stop_level(double level);       ///< TRIGger:STOP:LEVel
    double trigger_stop_level();                     ///< TRIGger:STOP:LEVel?
    void set_trigger_stop_slope(Slope slope);        ///< TRIGger:STOP:SLOPe
    Slope trigger_stop_slope();                      ///< TRIGger:STOP:SLOPe?
    void set_trigger_stop_time(const Date& date, const Time& time);  ///< TRIGger:STOP:TIME

    // -- Data -------------------------------------------------------------------
    /// DATA? — averaged values of the configured functions, or of `functions`.
    ///
    /// Passing an explicit list also *replaces* the configured SENSe:FUNCtion
    /// list on the instrument (the manual lists DATA? as invalidating
    /// FUNCtion[:ON] and FUNCtion:COUNt?), so a later data() with no argument
    /// returns these functions, not the ones set by set_functions().
    std::vector<double> data(const std::vector<std::string>& functions = {});

    /// DATA:STATus? — values plus one status flag per value
    /// (see the measurement_status:: bit constants). Replaces the configured
    /// function list in the same way as data() when given an explicit list.
    Reading data_with_status(const std::vector<std::string>& functions = {});

    // -- CALCulate: harmonics and spectrum --------------------------------------
    void set_harmonic_order(int order);               ///< CALCulate:HARMonic:ORDer
    int harmonic_order();                             ///< CALCulate:HARMonic:ORDer?
    void transform_once();                            ///< CALCulate:TRANsform:FREQuency ONCE
    void set_transform_mode(TransformMode mode);      ///< CALCulate:TRANsform:FREQuency:MODE
    TransformMode transform_mode();                   ///< CALCulate:TRANsform:FREQuency:MODE?
    void set_transform_functions(const std::vector<std::string>& functions); ///< ...:FUNCtion
    std::vector<std::string> transform_functions();   ///< ...:FUNCtion?
    void set_transform_start(double hertz);           ///< ...:STARt
    double transform_start();                         ///< ...:STARt?
    void set_transform_stop(double hertz);            ///< ...:STOP
    double transform_stop();                          ///< ...:STOP?
    void set_transform_cycles(int cycles);            ///< ...:CYCLes (4|6|8|10|12, STD mode)
    int transform_cycles();                           ///< ...:CYCLes?
    void set_transform_grouping(HarmonicGrouping grouping); ///< ...:GROuping
    HarmonicGrouping transform_grouping();            ///< ...:GROuping?
    /// CALCulate:DATA? [<count>[,<offset>]] — spectrum values. `count` 0 asks
    /// for everything the instrument has.
    std::vector<double> transform_data(int count = 0, int offset = 0);
    DataPreamble transform_preamble();                ///< CALCulate:DATA:PREamble?
    std::vector<double> transform_thd();              ///< CALCulate:DATA:THD?

    // -- CALCulate: integration (energy) ----------------------------------------
    void set_integral_enabled(bool on);               ///< CALCulate:INTegral:STATe
    bool integral_enabled();                          ///< CALCulate:INTegral:STATe?
    void set_integral_functions(const std::vector<std::string>& functions); ///< CALCulate:INTegral:FUNCtion
    std::vector<std::string> integral_functions();    ///< CALCulate:INTegral:FUNCtion?
    void clear_integral();                            ///< CALCulate:INTegral:CLEar
    void set_integral_auto_clear(bool on);            ///< CALCulate:INTegral:CLEar:AUTO
    bool integral_auto_clear();                       ///< CALCulate:INTegral:CLEar:AUTO?
    void set_integral_start_source(IntegralStartSource source); ///< ...:STARt:SOURce
    IntegralStartSource integral_start_source();      ///< ...:STARt:SOURce?
    void start_integral();                            ///< CALCulate:INTegral:STARt
    void set_integral_start_time(const Date& date, const Time& time); ///< ...:STARt:TIME
    void set_integral_stop_source(IntegralStopSource source);   ///< ...:STOP:SOURce
    IntegralStopSource integral_stop_source();        ///< ...:STOP:SOURce?
    void stop_integral();                             ///< CALCulate:INTegral:STOP
    void set_integral_stop_time(const Date& date, const Time& time);  ///< ...:STOP:TIME
    void set_integral_stop_interval(double seconds);  ///< ...:STOP:TINTerval
    double integral_stop_interval();                  ///< ...:STOP:TINTerval?

    // -- CALCulate: power ------------------------------------------------------
    void set_power_correction(PowerCorrection correction); ///< CALCulate:POWer:CORRected
    PowerCorrection power_correction();               ///< CALCulate:POWer:CORRected?
    /// CALCulate:POWer[460]:EFFiciency:REFerence — the two power functions
    /// whose ratio fn::efficiency() reports.
    void set_efficiency_reference(std::string_view input, std::string_view output,
                                 int phase = 0);

    // -- Memory recording: SENSe:SWEep and TRACe --------------------------------
    void set_sweep_enabled(SweepBlock block, bool on);    ///< SENSe:SWEep<n>:STATe
    bool sweep_enabled(SweepBlock block);                 ///< SENSe:SWEep<n>:STATe?
    void set_sweep_time(SweepBlock block, double seconds); ///< SENSe:SWEep<n>:TIME
    void set_sweep_time_max(SweepBlock block);            ///< SENSe:SWEep<n>:TIME MAX
    double sweep_time(SweepBlock block);                  ///< SENSe:SWEep<n>:TIME?
    void set_sweep_offset_time(SweepBlock block, double seconds); ///< ...:OFFSet:TIME
    double sweep_offset_time(SweepBlock block);           ///< ...:OFFSet:TIME?
    int sweep_points(SweepBlock block);                   ///< ...:POINTS?
    int sweep_offset_points(SweepBlock block);            ///< ...:OFFSet:POINTS?
    void set_sweep_count(SweepBlock block, int count);    ///< ...:COUNt
    int sweep_count(SweepBlock block);                    ///< ...:COUNt?
    void set_sweep_sparsing(SweepBlock block, int factor); ///< ...:SFACtor (1..65535)
    int sweep_sparsing(SweepBlock block);                 ///< ...:SFACtor?
    void set_sweep_functions(SweepBlock block, const std::vector<std::string>& functions); ///< ...:FUNCtion
    std::vector<std::string> sweep_functions(SweepBlock block); ///< ...:FUNCtion?

    DataPreamble trace_preamble(SweepBlock block);        ///< TRACe:DATA:PREamble?
    /// TRACe:DATA? — recorded values. `count` 0 asks for everything.
    std::vector<double> trace_data(SweepBlock block, int count = 0, int offset = 0,
                                   int sparsing = 0);
    /// TRACe:DATA:STATus? — the status flags belonging to trace_data().
    std::vector<int> trace_status(SweepBlock block, int count = 0, int offset = 0,
                                  int sparsing = 0);
    int trace_free();                                     ///< TRACe:FREE? (free points)
    int trace_length();                                   ///< TRACe:CATalog:LENgth?
    void delete_traces();                                 ///< TRACe:DELete:ALL

    // -- FORMat -----------------------------------------------------------------
    /// FORMat[:DATA] — only DataFormat::Ascii can be parsed by this library;
    /// selecting REAL or INTeger makes every measurement query return a binary
    /// block, so the typed helpers then throw ProtocolError.
    void set_data_format(DataFormat format, int length = 0);
    DataFormatSetting data_format();                  ///< FORMat[:DATA]?
    void set_status_format(DataFormat format, int length = 0); ///< FORMat:DATA:STATus
    DataFormatSetting status_format();                ///< FORMat:DATA:STATus?
    void set_byte_order(ByteOrder order);             ///< FORMat:BORDer
    ByteOrder byte_order();                           ///< FORMat:BORDer?
    void set_transpose(bool on);                      ///< FORMat:TRANspose
    bool transpose();                                 ///< FORMat:TRANspose?

    // -- DISPlay and OUTPut ------------------------------------------------------
    void set_display_enabled(bool on);                ///< DISPlay:WINDow:STATe
    bool display_enabled();                           ///< DISPlay:WINDow:STATe?
    void set_display_functions(const std::vector<std::string>& functions); ///< DISPlay:USER:FUNCtion
    std::vector<std::string> display_functions();     ///< DISPlay:USER:FUNCtion?
    void set_output_enabled(bool on);                 ///< OUTPut9:STATe (process interface)
    bool output_enabled();                            ///< OUTPut9:STATe?

    // -- SYSTem -------------------------------------------------------------------
    std::string scpi_version();                       ///< SYSTem:VERSion?
    void set_key_lock(KeyLock lock);                  ///< SYSTem:KLOCk
    KeyLock key_lock();                               ///< SYSTem:KLOCk?
    void set_date(const Date& date);                  ///< SYSTem:DATE
    Date date();                                      ///< SYSTem:DATE?
    void set_time(const Time& time);                  ///< SYSTem:TIME
    Time time_of_day();                               ///< SYSTem:TIME?
    void set_gpib_address(int address);               ///< SYSTem:COMMunicate:GPIB:ADDRess (1..30)
    int gpib_address();                               ///< SYSTem:COMMunicate:GPIB:ADDRess?
    void set_serial_baud(int baud);                   ///< SYSTem:COMMunicate:SERial:BAUD
    int serial_baud();                                ///< SYSTem:COMMunicate:SERial:BAUD?
    void set_language(std::string_view language);     ///< SYSTem:LANGuage
    std::string language();                           ///< SYSTem:LANGuage?

    // -- TIMer ---------------------------------------------------------------------
    void reset_timer();                               ///< TIMer:RESet
    double timer_reset_time();                        ///< TIMer:RESet:TIME?

    // -- STATus ---------------------------------------------------------------------
    /// Reads one part of one status register, e.g.
    /// status(StatusRegister::Operation, RegisterPart::Condition).
    /// The bit meanings are in the operation_status::, questionable_status::
    /// and channel_status:: namespaces.
    int status(StatusRegister reg, RegisterPart part);

    /// Writes one of the settable parts (Enable, PositiveTransition,
    /// NegativeTransition). Throws std::invalid_argument for Condition/Event,
    /// which are read-only.
    void set_status(StatusRegister reg, RegisterPart part, int mask);

    /// STATus:OPERation:CONDition? — kept as a shorthand for the register a
    /// measurement loop polls (see operation_status::kAveraging).
    int status_operation_condition();

    // -- Errors -----------------------------------------------------------------------
    std::vector<ScpiErrorInfo> read_errors();        ///< drains SYST:ERR?
    std::vector<ScpiErrorInfo> read_errors_at_once();///< SYST:ERR:ALL? (one round-trip)
    void check_errors();                             ///< throws ScpiError if queue non-empty

    // -- Plumbing ---------------------------------------------------------------------
    void set_timeout(std::chrono::milliseconds timeout);
    std::chrono::milliseconds timeout() const;
    ScpiClient& scpi() { return *scpi_; }
    const ScpiClient& scpi() const { return *scpi_; }

private:
    /// Validates a phase suffix used on the SENSe VOLTage/CURRent nodes (1..6).
    static void check_phase(int phase);
    /// Validates a hardware input channel for the INPut subsystem (1..12).
    static void check_channel(int channel);
    /// Builds "VOLT<n>" / "CURR<n>" prefixes for the SENSe subsystem.
    static std::string sense_node(const char* base, int phase);
    /// Builds the SCPI path of a status register part.
    static std::string status_node(StatusRegister reg, RegisterPart part);
    /// Appends " <count>[,<offset>[,<sparsing>]]" when the arguments are set.
    static std::string data_arguments(int count, int offset, int sparsing = 0);

    std::unique_ptr<ScpiClient> scpi_;
};

} // namespace fluke::norma

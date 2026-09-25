#pragma once

#include <array>
#include <cstdint>
#include <deque>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <fluke/norma/types.hpp>

namespace fluke::norma::sim {

/// The electrical signal presented to one phase of the simulated instrument.
/// Tests set these to decide what the measurement queries should report.
struct PhaseSignal {
    double voltage_rms = 230.0;     ///< true-RMS voltage [V]
    double current_rms = 1.0;       ///< true-RMS current [A]
    double phase_shift_deg = 0.0;   ///< angle between U and I; negative = capacitive
    double voltage_dc = 0.0;        ///< DC component of the voltage [V]
    double current_dc = 0.0;        ///< DC component of the current [A]
    double voltage_thd = 0.0;       ///< total harmonic distortion of U, as a fraction
    double current_thd = 0.0;       ///< total harmonic distortion of I, as a fraction
};

/// The whole signal the simulated instrument "sees".
struct SimulatedSignal {
    /// When false the instrument cannot synchronize, so every measurement is
    /// reported as NaN with the Undefined status bit — the behaviour a client
    /// gets from a real instrument with nothing connected to its inputs.
    bool present = true;
    double frequency = 50.0; ///< frequency of the SYNC source [Hz]
    std::array<PhaseSignal, kMaxPhase> phases{};
};

/// Fixed properties of the simulated instrument.
struct SimulatorOptions {
    Identification identification{"Fluke", "NORMA5000", "KN34512BA", "01.05"};
    std::string scpi_version = "1999.0";
    std::string options_response = "PP54,PI1";
    /// Number of electrical input channels: 6 or 12.
    int channels = kMaxInputChannel;
    /// Selectable voltage/current ranges, reported by RANGe:LIST? and used to
    /// derive the under/overrange status bits.
    std::vector<double> voltage_ranges{0.3, 1.0, 3.0, 10.0, 30.0, 100.0, 300.0, 1000.0};
    std::vector<double> current_ranges{0.03, 0.1, 0.3, 1.0, 3.0, 10.0};
};

/// Per-phase input configuration.
struct ChannelConfig {
    double range = 1000.0;
    bool autorange = true;
    double scale = 1.0;
};

/// Everything the simulated instrument remembers. Exposed so tests can assert
/// on the effect of a command instead of only on its response.
struct SimulatorState {
    // ROUTe
    WiringSystem wiring_system = WiringSystem::ThreeWattmeter;

    // INPut, indexed by hardware channel 1..12 (index 0 unused).
    std::array<Coupling, kMaxInputChannel + 1> coupling{};
    std::array<Shunt, kMaxInputChannel + 1> shunt{};
    std::array<double, kMaxInputChannel + 1> gain{};
    std::array<bool, kMaxInputChannel + 1> filter{};
    double filter_frequency = 3.0e5;

    // SENSe ranging, indexed by phase 1..6 (index 0 unused).
    std::array<ChannelConfig, kMaxPhase + 1> voltage{};
    std::array<ChannelConfig, kMaxPhase + 1> current{};
    double aperture = 0.1;
    double sampling_frequency = 1.0e5;

    // SYNC
    bool sync_enabled = true;
    std::string sync_source = "VOLT1";
    double sync_level = 0.0;
    LevelUnit sync_level_unit = LevelUnit::Percent;
    Slope sync_slope = Slope::Positive;
    bool sync_filter = false;
    double sync_filter_frequency = 1.0e3;
    double sync_timeout = 1.0;

    // SENSe functions
    std::vector<std::string> functions;
    bool concurrent = true;

    // Acquisition
    bool continuous = true;
    bool initiated = false;

    // TRIGger
    std::string trigger_start_source = "IMM";
    std::string trigger_stop_source = "IMM";
    double trigger_start_level = 0.0;
    double trigger_stop_level = 0.0;
    Slope trigger_start_slope = Slope::Positive;
    Slope trigger_stop_slope = Slope::Positive;

    // FORMat
    DataFormatSetting data_format{DataFormat::Ascii, 6};
    DataFormatSetting status_format{DataFormat::Ascii, 16};
    ByteOrder byte_order = ByteOrder::Normal;
    bool transpose = false;

    // CALCulate
    int harmonic_order = 1;
    TransformMode transform_mode = TransformMode::Fft;
    std::vector<std::string> transform_functions;
    double transform_start = 0.0;
    double transform_stop = 5000.0;
    int transform_cycles = 10;
    HarmonicGrouping transform_grouping = HarmonicGrouping::Component;
    bool integral_enabled = false;
    bool integral_running = false;
    bool integral_auto_clear = true;
    std::vector<std::string> integral_functions;
    IntegralStartSource integral_start_source = IntegralStartSource::Command;
    IntegralStopSource integral_stop_source = IntegralStopSource::Command;
    double integral_stop_interval = 60.0;
    PowerCorrection power_correction = PowerCorrection::Star;
    std::string efficiency_input;
    std::string efficiency_output;

    // Memory recording, indexed by block (index 0 unused).
    struct SweepConfig {
        bool enabled = false;
        double time = 1.0;
        double offset_time = 0.0;
        int count = 1;
        int sparsing = 1;
        std::vector<std::string> functions;
    };
    std::array<SweepConfig, 3> sweep{};

    // DISPlay / OUTPut
    bool display_enabled = true;
    std::vector<std::string> display_functions;
    bool output_enabled = false;

    // SYSTem
    KeyLock key_lock = KeyLock::Off;
    Date date{2026, 1, 1};
    Time time{12, 0, 0};
    int gpib_address = 5;
    int serial_baud = 115200;
    std::string language = "DEFault";
    double timer_seconds = 0.0;

    // IEEE 488.2 registers
    int event_status = 0;
    int event_status_enable = 0;
    int service_request_enable = 0;

    /// The settable and latched parts of one SCPI status register. The
    /// CONDition part is not stored: it is derived from the instrument state
    /// every time it is read (see NormaSimulator::condition).
    struct RegisterState {
        int event = 0;
        int enable = 0;
        int ptransition = 0xFFFF; ///< SCPI default: every 0->1 edge latches
        int ntransition = 0;
        int previous_condition = 0;
    };
    RegisterState operation;
    RegisterState questionable;
    RegisterState questionable_voltage;
    RegisterState questionable_current;

    /// The instrument error queue, oldest first.
    std::deque<ScpiErrorInfo> errors;

    /// Saved setups, keyed by *SAV slot.
    std::array<bool, 25> saved_setups{};
};

/// SCPI errors the simulator raises, with the wording from the manual's error
/// table so a client sees the same text a real instrument would send.
namespace error_code {
inline constexpr int kNoError = 0;
inline constexpr int kCommandError = -100;
inline constexpr int kInvalidCharacter = -101;
inline constexpr int kSyntaxError = -102;
inline constexpr int kParameterNotAllowed = -108;
inline constexpr int kMissingParameter = -109;
inline constexpr int kUndefinedHeader = -113;
inline constexpr int kNumericDataError = -120;
inline constexpr int kExecutionError = -200;
inline constexpr int kSettingsConflict = -221;
inline constexpr int kDataOutOfRange = -222;
inline constexpr int kIllegalParameterValue = -224;
inline constexpr int kQueryUnterminated = -420;
} // namespace error_code

/// One measurement the simulator produced, before it is formatted.
struct SimulatedMeasurement {
    double value = 0.0;
    int status = measurement_status::kNormal;
};

/// A stateful, offline Fluke NORMA 4000/5000.
///
/// `handle_line()` consumes one command line exactly as the instrument's input
/// unit does — `;`-separated commands, shared-path shortening, long and short
/// mnemonics, quoted string parameters — and returns the response line when the
/// line contained at least one query. Commands it cannot parse are rejected
/// into the error queue with the SCPI code a real instrument would use, so
/// client-side error handling is exercised rather than assumed.
///
/// It models instrument *state*, not instrument *timing*: a measurement is
/// always immediately available, and the averaging interval only affects the
/// value reported for the TIME function.
class NormaSimulator {
public:
    explicit NormaSimulator(SimulatorOptions options = {});

    /// Handles one command line (without its terminator). Returns the response
    /// line for a line containing queries, or std::nullopt for a pure setting
    /// command.
    std::optional<std::string> handle_line(std::string_view line);

    /// Restores the documented *RST state. Does not clear the error queue.
    void reset();

    /// Clears the status registers and the error queue (*CLS).
    void clear_status();

    /// The live CONDition part of a status register, derived from the current
    /// configuration and signal (see the manual's register tables).
    int condition(StatusRegister reg) const;

    SimulatorState& state() { return state_; }
    const SimulatorState& state() const { return state_; }

    SimulatedSignal& signal() { return signal_; }
    const SimulatedSignal& signal() const { return signal_; }

    const SimulatorOptions& options() const { return options_; }

    /// Number of command lines handled — lets a test assert how many
    /// round-trips a library call actually costs.
    std::uint64_t handled_lines() const { return handled_lines_; }

    /// Points the simulated recording memory holds, as TRACe:FREE? reports.
    static constexpr int kTraceCapacity = 4096;

    /// Computes what a single <function> currently measures. Public so tests
    /// can check the library's parsing against the simulator's own numbers.
    SimulatedMeasurement measure(std::string_view function) const;

    /// Queues an instrument error, as the firmware does when it rejects a
    /// command. Public so a test can make the next SYST:ERR? report a fault.
    void push_error(int code, std::string message);

    /// One parsed command out of a command line. Defined in the simulator's
    /// internal header: public only so the subsystem handlers' shared plumbing
    /// can name it.
    struct Command;

private:
    /// Parses and executes one command out of a command line. Returns the
    /// response for a query.
    std::optional<std::string> execute(Command& command);

    // Subsystem handlers. Each returns true when it recognized the command;
    // the response (if any) is written to `command.response`.
    bool handle_common(Command& command);
    bool handle_route(Command& command);
    bool handle_input(Command& command);
    bool handle_sense(Command& command);
    bool handle_sync(Command& command);
    bool handle_acquisition(Command& command);
    bool handle_trigger(Command& command);
    bool handle_calculate(Command& command);
    bool handle_trace(Command& command);
    bool handle_format(Command& command);
    bool handle_display_output(Command& command);
    bool handle_system(Command& command);
    bool handle_status(Command& command);
    bool handle_timer(Command& command);

    /// DATA? / DATA:STATus? — builds the response from the signal model.
    std::string measurement_response(const std::vector<std::string>& functions,
                                    bool with_status);

    /// Latches newly set/cleared condition bits into a register's EVENt part
    /// according to its PTRansition/NTRansition masks, and returns EVENt.
    int latch_event(StatusRegister reg);

    SimulatorState::RegisterState& register_state(StatusRegister reg);

    std::string format_value(double value) const;

    /// Points currently recorded, derived from the configured sweep time and
    /// the averaging interval.
    int trace_points() const;

    /// Joins the responses of one command line with ';', as the manual requires.
    static std::string join_responses(const std::vector<std::string>& responses);

    SimulatorOptions options_;
    SimulatorState state_;
    SimulatedSignal signal_;
    std::uint64_t handled_lines_ = 0;
};

} // namespace fluke::norma::sim

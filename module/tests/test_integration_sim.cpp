// The library driven against the simulated instrument, in process.
//
// These are the tests that cross-check the wrapper against an independent
// reading of the manual: the simulator's SCPI parser was written from the
// command reference, so a command the library formats wrongly is *rejected*
// here rather than merely differing from a string another test also wrote.
//
// SimulatorTransport keeps it in-process, so a test is a few microseconds and
// can inspect instrument state directly. test_integration_tcp.cpp runs the same
// kind of workflow over a real socket.

#include <cmath>
#include <memory>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <fluke/norma/instrument.hpp>
#include <fluke/norma/simulator/norma_simulator.hpp>
#include <fluke/norma/simulator/simulator_transport.hpp>

using namespace fluke::norma;
using fluke::norma::sim::NormaSimulator;
using fluke::norma::sim::SimulatorTransport;

namespace {

/// A connected instrument talking to an in-process simulator.
struct Rig {
    Rig() {
        auto transport = std::make_unique<SimulatorTransport>();
        transport->open();
        link = transport.get();
        simulator = transport->shared_simulator();
        norma = std::make_unique<NormaInstrument>(std::move(transport));
    }

    SimulatorTransport* link = nullptr;
    std::shared_ptr<NormaSimulator> simulator;
    std::unique_ptr<NormaInstrument> norma;

    NormaInstrument* operator->() { return norma.get(); }

    /// Fails the test if the instrument queued an error, naming it.
    void require_accepted() {
        const auto errors = norma->read_errors();
        for (const ScpiErrorInfo& error : errors) {
            FAIL_CHECK("instrument rejected a command: " << error.code << " \"" << error.message
                                                         << "\"");
        }
        CHECK(errors.empty());
    }
};

} // namespace

TEST_CASE("the canonical measurement workflow runs end to end", "[simulator]") {
    Rig rig;
    rig.simulator->signal().frequency = 50.0;
    for (int phase = 0; phase < 3; ++phase) {
        rig.simulator->signal().phases[static_cast<std::size_t>(phase)] = {
            230.0, 5.0, 30.0, 0.0, 0.0, 0.0, 0.0};
    }

    // The sequence the manual prescribes.
    rig->reset();
    rig->wait_operation_complete();
    rig->set_wiring_system(WiringSystem::ThreeWattmeter);
    rig->sync_to_voltage(1);
    rig->set_voltage_autorange(1, true);
    rig->set_current_autorange(1, true);
    rig->set_aperture(0.3);

    const std::vector<std::string> functions = {fn::voltage(1), fn::current(1),
                                                fn::active_power(1), fn::frequency(),
                                                fn::time_interval()};
    rig->set_functions(functions);
    rig->set_continuous(true);
    rig.require_accepted();

    CHECK(rig->function_count() == static_cast<int>(functions.size()));
    CHECK(rig->functions() == functions);

    const auto values = rig->data();
    REQUIRE(values.size() == functions.size());
    CHECK_THAT(values[0], Catch::Matchers::WithinRel(230.0, 1e-4));
    CHECK_THAT(values[1], Catch::Matchers::WithinRel(5.0, 1e-4));
    CHECK_THAT(values[2], Catch::Matchers::WithinRel(230.0 * 5.0 * std::cos(30.0 * M_PI / 180.0),
                                                     1e-4));
    CHECK_THAT(values[3], Catch::Matchers::WithinRel(50.0, 1e-6));
    CHECK(values[4] > 0.0);

    const Reading reading = rig->data_with_status();
    REQUIRE(reading.values.size() == functions.size());
    REQUIRE(reading.status.size() == functions.size());
    for (std::size_t i = 0; i < reading.values.size(); ++i) {
        CHECK(reading.is_valid(i));
    }
    rig.require_accepted();
}

// Every command the facade can send, in one pass, checked against the
// simulator's parser. A header the library spells wrongly lands in the error
// queue, so this is the test that would catch a typo the conformance table
// cannot — both sides of that table are written by hand from the same file.
TEST_CASE("the instrument accepts every command the facade sends", "[simulator]") {
    Rig rig;
    rig->prepare();
    rig.require_accepted();

    SECTION("IEEE 488.2") {
        rig->reset();
        rig->clear_status();
        CHECK_FALSE(rig->identify().manufacturer.empty());
        CHECK_FALSE(rig->options().empty());
        CHECK_FALSE(rig->learn().empty());
        rig->wait_operation_complete();
        rig->set_operation_complete_flag();
        rig->wait_pending_operations();
        rig->trigger();
        rig->set_event_status_enable(255);
        CHECK(rig->event_status_enable() == 255);
        rig->event_status();
        rig->set_service_request_enable(32);
        CHECK(rig->service_request_enable() == 32);
        rig->status_byte();
        rig->save_setup(10);
        rig->recall_setup(10);
    }

    SECTION("ROUTe and INPut") {
        rig->set_wiring_system(WiringSystem::TwoWattmeter);
        CHECK(rig->wiring_system() == WiringSystem::TwoWattmeter);
        rig->set_input_coupling(2, Coupling::AC);
        CHECK(rig->input_coupling(2) == Coupling::AC);
        rig->set_input_gain(1, 25.0);
        CHECK_THAT(rig->input_gain(1), Catch::Matchers::WithinRel(25.0, 1e-9));
        rig->set_input_filter(1, false);
        CHECK_FALSE(rig->input_filter(1));
        CHECK(rig->input_filter_frequency(1) > 0.0);
        rig->set_input_shunt(1, Shunt::External);
        CHECK(rig->input_shunt(1) == Shunt::External);
    }

    SECTION("SENSe ranging and scaling") {
        rig->set_voltage_range(1, 300.0);
        CHECK_THAT(rig->voltage_range(1), Catch::Matchers::WithinRel(300.0, 1e-9));
        rig->set_voltage_autorange(1, true);
        CHECK(rig->voltage_autorange(1));
        CHECK(rig->voltage_ranges(1).size() > 1);
        rig->set_voltage_scale(1, 10.0);
        CHECK_THAT(rig->voltage_scale(1), Catch::Matchers::WithinRel(10.0, 1e-9));
        rig->set_current_range(1, 1.0);
        CHECK_THAT(rig->current_range(1), Catch::Matchers::WithinRel(1.0, 1e-9));
        rig->set_current_autorange(1, false);
        CHECK_FALSE(rig->current_autorange(1));
        CHECK(rig->current_ranges(1).size() > 1);
        rig->set_current_scale(1, 5.0);
        CHECK_THAT(rig->current_scale(1), Catch::Matchers::WithinRel(5.0, 1e-9));
        rig->set_aperture(1.0);
        CHECK_THAT(rig->aperture(), Catch::Matchers::WithinRel(1.0, 1e-9));
        CHECK(rig->sampling_frequency() > 0.0);
    }

    SECTION("SYNC") {
        rig->set_sync_enabled(true);
        CHECK(rig->sync_enabled());
        rig->sync_to_voltage(2);
        CHECK(rig->sync_source() == "VOLT2");
        rig->sync_to_current(3);
        CHECK(rig->sync_source() == "CURR3");
        rig->sync_external();
        CHECK(rig->sync_source() == "EXT");
        rig->set_sync_level_unit(LevelUnit::Absolute);
        CHECK(rig->sync_level_unit() == LevelUnit::Absolute);
        rig->set_sync_level(12.5);
        CHECK_THAT(rig->sync_level(), Catch::Matchers::WithinRel(12.5, 1e-9));
        rig->set_sync_slope(Slope::Negative);
        CHECK(rig->sync_slope() == Slope::Negative);
        rig->set_sync_filter(true);
        CHECK(rig->sync_filter());
        rig->set_sync_filter_frequency(1000.0);
        CHECK_THAT(rig->sync_filter_frequency(), Catch::Matchers::WithinRel(1000.0, 1e-9));
        rig->set_sync_timeout(2.0);
        CHECK_THAT(rig->sync_timeout(), Catch::Matchers::WithinRel(2.0, 1e-9));
    }

    SECTION("SENSe functions and acquisition") {
        rig->set_concurrent(true);
        CHECK(rig->concurrent());
        rig->set_functions({fn::voltage(1), fn::current(1)});
        CHECK(rig->function_count() == 2);
        rig->enable_all_functions();
        CHECK(rig->function_count() > 2);
        rig->clear_functions();
        CHECK(rig->function_count() == 0);
        rig->set_continuous(false);
        CHECK_FALSE(rig->continuous());
        rig->initiate();
        rig->initiate_sweep(SweepBlock::Block1);
        rig->abort();
        rig->set_continuous(true);
    }

    SECTION("TRIGger") {
        rig->set_trigger_start_source("BUS");
        CHECK(rig->trigger_start_source() == "BUS");
        rig->set_trigger_start_level(1.0);
        CHECK_THAT(rig->trigger_start_level(), Catch::Matchers::WithinRel(1.0, 1e-9));
        rig->set_trigger_start_slope(Slope::Negative);
        CHECK(rig->trigger_start_slope() == Slope::Negative);
        rig->set_trigger_start_time(Date{2026, 9, 26}, Time{12, 0, 0});
        rig->set_trigger_stop_source("IMM");
        CHECK(rig->trigger_stop_source() == "IMM");
        rig->set_trigger_stop_level(-1.0);
        CHECK_THAT(rig->trigger_stop_level(), Catch::Matchers::WithinRel(-1.0, 1e-9));
        rig->set_trigger_stop_slope(Slope::Positive);
        CHECK(rig->trigger_stop_slope() == Slope::Positive);
        rig->set_trigger_stop_time(Date{2026, 9, 26}, Time{13, 0, 0});
    }

    SECTION("CALCulate") {
        rig->set_harmonic_order(3);
        CHECK(rig->harmonic_order() == 3);
        rig->set_transform_mode(TransformMode::Std);
        CHECK(rig->transform_mode() == TransformMode::Std);
        rig->set_transform_functions({fn::voltage(1)});
        CHECK(rig->transform_functions() == std::vector<std::string>{"VOLT1"});
        rig->set_transform_start(0.0);
        rig->set_transform_stop(2500.0);
        CHECK_THAT(rig->transform_stop(), Catch::Matchers::WithinRel(2500.0, 1e-9));
        rig->set_transform_cycles(12);
        CHECK(rig->transform_cycles() == 12);
        rig->set_transform_grouping(HarmonicGrouping::HGroup);
        CHECK(rig->transform_grouping() == HarmonicGrouping::HGroup);
        rig->transform_once();
        CHECK_FALSE(rig->transform_data().empty());
        CHECK(rig->transform_preamble().count > 0);
        CHECK_FALSE(rig->transform_thd().empty());

        rig->set_integral_enabled(true);
        CHECK(rig->integral_enabled());
        rig->set_integral_functions({fn::integral(fn::active_power(1))});
        CHECK_FALSE(rig->integral_functions().empty());
        rig->set_integral_auto_clear(false);
        CHECK_FALSE(rig->integral_auto_clear());
        rig->set_integral_start_source(IntegralStartSource::Command);
        CHECK(rig->integral_start_source() == IntegralStartSource::Command);
        rig->start_integral();
        rig->set_integral_start_time(Date{2026, 9, 26}, Time{12, 0, 0});
        rig->set_integral_stop_source(IntegralStopSource::TimeInterval);
        CHECK(rig->integral_stop_source() == IntegralStopSource::TimeInterval);
        rig->set_integral_stop_interval(30.0);
        CHECK_THAT(rig->integral_stop_interval(), Catch::Matchers::WithinRel(30.0, 1e-9));
        rig->set_integral_stop_time(Date{2026, 9, 26}, Time{13, 0, 0});
        rig->stop_integral();
        rig->clear_integral();

        rig->set_power_correction(PowerCorrection::Delta);
        CHECK(rig->power_correction() == PowerCorrection::Delta);
        rig->set_efficiency_reference(fn::active_power(1), fn::active_power(2));
        rig->set_efficiency_reference(fn::active_power(), fn::active_power(460), 460);
    }

    SECTION("memory recording") {
        rig->set_sweep_functions(SweepBlock::Block1, {fn::voltage(1)});
        CHECK_FALSE(rig->sweep_functions(SweepBlock::Block1).empty());
        rig->set_sweep_time(SweepBlock::Block1, 1.0);
        CHECK_THAT(rig->sweep_time(SweepBlock::Block1), Catch::Matchers::WithinRel(1.0, 1e-9));
        rig->set_sweep_time_max(SweepBlock::Block2);
        rig->set_sweep_offset_time(SweepBlock::Block1, 0.0);
        CHECK(rig->sweep_offset_time(SweepBlock::Block1) == 0.0);
        rig->set_sweep_count(SweepBlock::Block1, 5);
        CHECK(rig->sweep_count(SweepBlock::Block1) == 5);
        rig->set_sweep_sparsing(SweepBlock::Block1, 2);
        CHECK(rig->sweep_sparsing(SweepBlock::Block1) == 2);
        rig->set_sweep_enabled(SweepBlock::Block1, true);
        CHECK(rig->sweep_enabled(SweepBlock::Block1));
        CHECK(rig->sweep_points(SweepBlock::Block1) > 0);
        rig->sweep_offset_points(SweepBlock::Block1);

        CHECK(rig->trace_length() > 0);
        CHECK(rig->trace_free() > 0);
        CHECK(rig->trace_preamble(SweepBlock::Block1).count > 0);
        CHECK_FALSE(rig->trace_data(SweepBlock::Block1).empty());
        CHECK_FALSE(rig->trace_status(SweepBlock::Block1).empty());
        CHECK(rig->trace_data(SweepBlock::Block1, 3).size() == 3);
        rig->delete_traces();
    }

    SECTION("FORMat, DISPlay and OUTPut") {
        rig->set_data_format(DataFormat::Ascii, 6);
        CHECK(rig->data_format().format == DataFormat::Ascii);
        CHECK(rig->data_format().length == 6);
        rig->set_status_format(DataFormat::Ascii, 16);
        CHECK(rig->status_format().format == DataFormat::Ascii);
        rig->set_byte_order(ByteOrder::Swapped);
        CHECK(rig->byte_order() == ByteOrder::Swapped);
        rig->set_transpose(true);
        CHECK(rig->transpose());
        rig->set_display_enabled(false);
        CHECK_FALSE(rig->display_enabled());
        rig->set_display_functions({fn::voltage(1)});
        CHECK_FALSE(rig->display_functions().empty());
        rig->set_output_enabled(true);
        CHECK(rig->output_enabled());
    }

    SECTION("SYSTem, TIMer and STATus") {
        CHECK_FALSE(rig->scpi_version().empty());
        rig->set_key_lock(KeyLock::Remote);
        CHECK(rig->key_lock() == KeyLock::Remote);
        rig->set_key_lock(KeyLock::Off);
        rig->set_date(Date{2026, 9, 26});
        const Date date = rig->date();
        CHECK(date.year == 2026);
        CHECK(date.month == 9);
        CHECK(date.day == 26);
        rig->set_time(Time{12, 30, 5});
        const Time time = rig->time_of_day();
        CHECK(time.hours == 12);
        CHECK(time.seconds == 5);
        rig->set_gpib_address(7);
        CHECK(rig->gpib_address() == 7);
        rig->set_serial_baud(9600);
        CHECK(rig->serial_baud() == 9600);
        rig->set_language("DEFault");
        CHECK(rig->language() == "DEFault");
        CHECK(rig->read_errors_at_once().empty());

        rig->reset_timer();
        CHECK(rig->timer_reset_time() == 0.0);

        for (StatusRegister reg : {StatusRegister::Operation, StatusRegister::Questionable,
                                   StatusRegister::QuestionableVoltage,
                                   StatusRegister::QuestionableCurrent}) {
            rig->status(reg, RegisterPart::Condition);
            rig->status(reg, RegisterPart::Event);
            rig->set_status(reg, RegisterPart::Enable, 1);
            CHECK(rig->status(reg, RegisterPart::Enable) == 1);
            rig->set_status(reg, RegisterPart::PositiveTransition, 2);
            CHECK(rig->status(reg, RegisterPart::PositiveTransition) == 2);
            rig->set_status(reg, RegisterPart::NegativeTransition, 4);
            CHECK(rig->status(reg, RegisterPart::NegativeTransition) == 4);
        }
        CHECK(rig->status_operation_condition() >= 0);
    }

    rig.require_accepted();
}

TEST_CASE("a rejected command surfaces as ScpiError with the instrument's code",
          "[simulator]") {
    Rig rig;
    rig->write("SYSTEM:THIS:DOES:NOT:EXIST");

    try {
        rig->check_errors();
        FAIL("expected the instrument to report the undefined header");
    } catch (const ScpiError& e) {
        CHECK(e.code() == -113);
        CHECK(e.message().find("Undefined header") != std::string::npos);
    }

    // check_errors() drained the queue, so the next call is clean.
    CHECK_NOTHROW(rig->check_errors());
}

TEST_CASE("several rejected commands all reach the caller", "[simulator]") {
    Rig rig;
    rig->write("NOT:A:COMMAND");
    rig->write("ALSO:NOT:A:COMMAND");

    try {
        rig->check_errors();
        FAIL("expected ScpiError");
    } catch (const ScpiError& e) {
        CHECK(e.all().size() == 2);
    }
}

TEST_CASE("prepare() recovers a link whose instrument was left in REAL format",
          "[simulator]") {
    // The scenario prepare() exists for: FORMat survives a disconnect, so a
    // previous session can leave every measurement query answering with a binary
    // block that the ASCII parser cannot read.
    Rig rig;
    rig->set_functions({fn::voltage(1)});
    rig->set_data_format(DataFormat::Real, 64);
    CHECK_THROWS_AS(rig->data(), ProtocolError);

    rig->prepare();

    CHECK(rig->data_format().format == DataFormat::Ascii);
    CHECK_NOTHROW(rig->data());
    rig.require_accepted();
}

TEST_CASE("prepare() enables concurrent functions and clears a stale queue",
          "[simulator]") {
    Rig rig;
    rig->set_concurrent(false);
    rig->write("GARBAGE");

    rig->prepare();

    CHECK(rig->concurrent());
    CHECK_NOTHROW(rig->check_errors());
    CHECK_NOTHROW(rig->set_functions({fn::voltage(1), fn::current(1)}));
    rig.require_accepted();
}

TEST_CASE("data() with an explicit list replaces the configured functions",
          "[simulator]") {
    // Documented on NormaInstrument::data: the manual lists DATA? as
    // invalidating FUNCtion[:ON]. A caller that does not expect it would read
    // the wrong values from a later data() call.
    Rig rig;
    rig->set_functions({fn::voltage(1), fn::current(1)});
    REQUIRE(rig->function_count() == 2);

    rig->data({fn::active_power(1)});

    CHECK(rig->function_count() == 1);
    CHECK(rig->functions() == std::vector<std::string>{"POW1:ACT"});
}

TEST_CASE("an unsynchronized instrument reports NaN with the Undefined bit",
          "[simulator]") {
    Rig rig;
    rig.simulator->signal().present = false;
    rig->set_functions({fn::voltage(1), fn::frequency()});

    const Reading reading = rig->data_with_status();

    REQUIRE(reading.values.size() == 2);
    CHECK(std::isnan(reading.values[0]));
    CHECK(std::isnan(reading.values[1]));
    CHECK(reading.status[0] == measurement_status::kUndefined);
    CHECK_FALSE(reading.is_valid(0));
    // A measurement the instrument could not compute is not an error.
    rig.require_accepted();
}

TEST_CASE("the status registers report a lost synchronization signal",
          "[simulator]") {
    Rig rig;
    CHECK((rig->status_operation_condition() & operation_status::kSynchronized) != 0);

    rig.simulator->signal().present = false;

    CHECK((rig->status_operation_condition() & operation_status::kSynchronized) == 0);
    CHECK((rig->status(StatusRegister::Questionable, RegisterPart::Condition) &
           questionable_status::kFrequency) != 0);
}

TEST_CASE("an overranged channel shows up in the questionable register",
          "[simulator]") {
    Rig rig;
    rig.simulator->signal().phases[0] = {230.0, 1.0, 0.0, 0.0, 0.0, 0.0, 0.0};
    rig->set_voltage_range(1, 100.0);

    const int voltage =
        rig->status(StatusRegister::QuestionableVoltage, RegisterPart::Condition);
    CHECK((voltage & channel_status::overrange(0)) != 0);

    rig->set_functions({fn::voltage(1)});
    const Reading reading = rig->data_with_status();
    CHECK((reading.status[0] & measurement_status::kOverrange) != 0);
    CHECK_FALSE(reading.is_valid(0));
}

TEST_CASE("a transport failure is reported as a ConnectionError", "[simulator]") {
    Rig rig;
    rig.link->fail_reads = true;
    CHECK_THROWS_AS(rig->identify(), ConnectionError);

    rig.link->fail_reads = false;
    rig.link->fail_writes = true;
    CHECK_THROWS_AS(rig->reset(), ConnectionError);
}

TEST_CASE("a stalled instrument is reported as a TimeoutError", "[simulator]") {
    Rig rig;
    rig.link->stall_reads = true;
    CHECK_THROWS_AS(rig->aperture(), TimeoutError);
}

// Reading a response that was never produced is the shape of a desynchronized
// session, and it must fail loudly rather than return the previous answer.
TEST_CASE("querying a setting command times out instead of returning stale data",
          "[simulator]") {
    Rig rig;
    rig->set_aperture(1.0);              // produces no response
    CHECK(rig.link->pending_responses() == 0);
    CHECK_THROWS_AS(rig->query("*CLS"), TimeoutError);
}

TEST_CASE("every query consumes exactly its own response", "[simulator]") {
    // A leftover response would mean a later query reads the wrong answer.
    Rig rig;
    rig->prepare();
    rig->identify();
    rig->set_functions({fn::voltage(1), fn::current(1)});
    rig->function_count();
    rig->data();
    rig->data_with_status();
    rig->aperture();
    rig->wiring_system();
    rig->read_errors();

    CHECK(rig.link->pending_responses() == 0);
}

TEST_CASE("closing and reopening the link keeps the instrument's settings",
          "[simulator]") {
    Rig rig;
    rig->set_aperture(0.25);
    rig->close();
    CHECK_FALSE(rig->is_open());
    CHECK_THROWS_AS(rig->aperture(), ConnectionError);

    rig.link->open();
    CHECK(rig->is_open());
    CHECK_THAT(rig->aperture(), Catch::Matchers::WithinRel(0.25, 1e-9));
}

TEST_CASE("single-shot acquisition works without continuous mode", "[simulator]") {
    Rig rig;
    rig.simulator->signal().phases[0] = {230.0, 1.0, 0.0, 0.0, 0.0, 0.0, 0.0};
    rig->set_functions({fn::voltage(1)});

    rig->set_continuous(false);
    rig->abort();
    // With no measurement running there is nothing to report yet.
    CHECK(std::isnan(rig->data().at(0)));

    rig->initiate();
    rig->wait_operation_complete();
    CHECK_THAT(rig->data().at(0), Catch::Matchers::WithinRel(230.0, 1e-4));
    rig.require_accepted();
}

TEST_CASE("a compound command line reaches the instrument as one round-trip",
          "[simulator]") {
    Rig rig;
    const std::uint64_t before = rig.simulator->handled_lines();

    rig->write("INP1:SHUN EXT;GAIN 25.0");

    CHECK(rig.simulator->handled_lines() == before + 1);
    CHECK(rig->input_shunt(1) == Shunt::External);
    CHECK_THAT(rig->input_gain(1), Catch::Matchers::WithinRel(25.0, 1e-9));
    rig.require_accepted();
}

TEST_CASE("a compound query returns its answers in order", "[simulator]") {
    Rig rig;
    rig->set_aperture(0.5);
    const std::string response = rig->query("APER?;:FUNC:CONC?");

    const auto parts = ScpiClient::split_semicolons(response);
    REQUIRE(parts.size() == 2);
    CHECK_THAT(ScpiClient::to_double(parts[0]), Catch::Matchers::WithinRel(0.5, 1e-9));
    CHECK(ScpiClient::to_bool(parts[1]));
}

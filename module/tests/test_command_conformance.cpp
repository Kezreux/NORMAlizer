// Wire-format conformance: every facade call against the exact SCPI command
// string the manual defines for it (docs/Fluke-NORMA-TCP-API.md, "Quick
// reference: all commands").
//
// This is one table on purpose. Adding a command to NormaInstrument means adding
// a row here, so the wrapper cannot drift from the manual unnoticed, and a
// reviewer can check the whole command surface against the reference in one
// screenful instead of hunting through behavioural tests.

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include <fluke/norma/instrument.hpp>
#include <fluke/norma/types.hpp>

#include "mock_transport.hpp"

using namespace fluke::norma;
using fluke::norma::test::MockTransport;

namespace {

/// One row: the command the instrument must receive, and the call that produces
/// it. `response` is the line the instrument answers with, for query rows.
struct Row {
    const char* expected;
    std::function<void(NormaInstrument&)> call;
    const char* response = nullptr;
};

/// Runs one row against a scripted transport and checks the bytes sent.
void check(const Row& row) {
    auto transport = std::make_unique<MockTransport>();
    transport->open();
    MockTransport* mock = transport.get();
    NormaInstrument norma(std::move(transport));

    if (row.response != nullptr) {
        mock->responses.push_back(row.response);
    }

    INFO("expected command: " << row.expected);
    REQUIRE_NOTHROW(row.call(norma));
    REQUIRE_FALSE(mock->writes.empty());
    CHECK(mock->last_command() == row.expected);
    // A response the call did not consume means the round-trip count is wrong.
    CHECK(mock->responses.empty());
}

void check_all(const std::vector<Row>& rows) {
    for (const Row& row : rows) {
        check(row);
    }
}

const Date kDate{2026, 9, 26};
const Time kTime{12, 30, 5};
const std::vector<std::string> kFunctions{"VOLT1", "CURR1", "POW1:ACT"};

} // namespace

TEST_CASE("IEEE 488.2 common commands", "[conformance]") {
    check_all({
        {"*RST", [](NormaInstrument& n) { n.reset(); }},
        {"*CLS", [](NormaInstrument& n) { n.clear_status(); }},
        {"*OPC", [](NormaInstrument& n) { n.set_operation_complete_flag(); }},
        {"*WAI", [](NormaInstrument& n) { n.wait_pending_operations(); }},
        {"*TRG", [](NormaInstrument& n) { n.trigger(); }},
        {"*ESE 255", [](NormaInstrument& n) { n.set_event_status_enable(255); }},
        {"*SRE 32", [](NormaInstrument& n) { n.set_service_request_enable(32); }},
        {"*SAV 10", [](NormaInstrument& n) { n.save_setup(10); }},
        {"*RCL 1", [](NormaInstrument& n) { n.recall_setup(1); }},
        {"*IDN?", [](NormaInstrument& n) { n.identify(); }, "Fluke,NORMA5000,KN1,01.05"},
        {"*OPT?", [](NormaInstrument& n) { n.options(); }, "PP54,PI1"},
        {"*LRN?", [](NormaInstrument& n) { n.learn(); }, "\"FUNC \"\"VOLT1\"\"\""},
        {"*OPC?", [](NormaInstrument& n) { n.wait_operation_complete(); }, "1"},
        {"*ESE?", [](NormaInstrument& n) { n.event_status_enable(); }, "255"},
        {"*ESR?", [](NormaInstrument& n) { n.event_status(); }, "0"},
        {"*SRE?", [](NormaInstrument& n) { n.service_request_enable(); }, "32"},
        {"*STB?", [](NormaInstrument& n) { n.status_byte(); }, "4"},
    });
}

TEST_CASE("ROUTe", "[conformance]") {
    check_all({
        {"ROUT:SYST \"3W\"",
         [](NormaInstrument& n) { n.set_wiring_system(WiringSystem::ThreeWattmeter); }},
        {"ROUT:SYST \"2W\"",
         [](NormaInstrument& n) { n.set_wiring_system(WiringSystem::TwoWattmeter); }},
        {"ROUT:SYST?", [](NormaInstrument& n) { n.wiring_system(); }, "\"3W\""},
    });
}

TEST_CASE("INPut", "[conformance]") {
    check_all({
        {"INP1:COUP AC", [](NormaInstrument& n) { n.set_input_coupling(1, Coupling::AC); }},
        {"INP2:COUP DC", [](NormaInstrument& n) { n.set_input_coupling(2, Coupling::DC); }},
        {"INP12:COUP DC", [](NormaInstrument& n) { n.set_input_coupling(12, Coupling::DC); }},
        {"INP1:GAIN 25", [](NormaInstrument& n) { n.set_input_gain(1, 25.0); }},
        {"INP3:FILT:STAT ON", [](NormaInstrument& n) { n.set_input_filter(3, true); }},
        {"INP3:FILT:STAT OFF", [](NormaInstrument& n) { n.set_input_filter(3, false); }},
        {"INP1:SHUN EXT", [](NormaInstrument& n) { n.set_input_shunt(1, Shunt::External); }},
        {"INP1:SHUN INT", [](NormaInstrument& n) { n.set_input_shunt(1, Shunt::Internal); }},
        {"INP1:COUP?", [](NormaInstrument& n) { n.input_coupling(1); }, "DC"},
        {"INP1:GAIN?", [](NormaInstrument& n) { n.input_gain(1); }, "1"},
        {"INP1:FILT:STAT?", [](NormaInstrument& n) { n.input_filter(1); }, "1"},
        {"INP1:FILT:LPAS:FREQ?", [](NormaInstrument& n) { n.input_filter_frequency(1); }, "3.0E5"},
        {"INP1:SHUN?", [](NormaInstrument& n) { n.input_shunt(1); }, "INT"},
    });
}

TEST_CASE("SENSe ranging, scaling and averaging", "[conformance]") {
    check_all({
        {"VOLT1:RANG 300", [](NormaInstrument& n) { n.set_voltage_range(1, 300.0); }},
        {"VOLT1:RANG:AUTO ON", [](NormaInstrument& n) { n.set_voltage_autorange(1, true); }},
        {"VOLT1:SCAL 100", [](NormaInstrument& n) { n.set_voltage_scale(1, 100.0); }},
        {"CURR2:RANG 5", [](NormaInstrument& n) { n.set_current_range(2, 5.0); }},
        {"CURR2:RANG:AUTO OFF", [](NormaInstrument& n) { n.set_current_autorange(2, false); }},
        {"CURR2:SCAL 10", [](NormaInstrument& n) { n.set_current_scale(2, 10.0); }},
        {"APER 1", [](NormaInstrument& n) { n.set_aperture(1.0); }},
        {"APER 0.015", [](NormaInstrument& n) { n.set_aperture(0.015); }},
        {"VOLT1:RANG?", [](NormaInstrument& n) { n.voltage_range(1); }, "300"},
        {"VOLT1:RANG:AUTO?", [](NormaInstrument& n) { n.voltage_autorange(1); }, "1"},
        {"VOLT1:RANG:LIST?", [](NormaInstrument& n) { n.voltage_ranges(1); }, "0.3,1,3,10"},
        {"VOLT1:SCAL?", [](NormaInstrument& n) { n.voltage_scale(1); }, "1"},
        {"CURR1:RANG?", [](NormaInstrument& n) { n.current_range(1); }, "10"},
        {"CURR1:RANG:AUTO?", [](NormaInstrument& n) { n.current_autorange(1); }, "0"},
        {"CURR1:RANG:LIST?", [](NormaInstrument& n) { n.current_ranges(1); }, "0.03,0.1"},
        {"CURR1:SCAL?", [](NormaInstrument& n) { n.current_scale(1); }, "1"},
        {"APER?", [](NormaInstrument& n) { n.aperture(); }, "0.1"},
        {"SWE:FREQ?", [](NormaInstrument& n) { n.sampling_frequency(); }, "1.0E5"},
    });
}

TEST_CASE("SYNC", "[conformance]") {
    check_all({
        {"SYNC:STAT ON", [](NormaInstrument& n) { n.set_sync_enabled(true); }},
        {"SYNC:SOUR VOLT1", [](NormaInstrument& n) { n.set_sync_source("VOLT1"); }},
        {"SYNC:SOUR VOLT2", [](NormaInstrument& n) { n.sync_to_voltage(2); }},
        {"SYNC:SOUR CURR3", [](NormaInstrument& n) { n.sync_to_current(3); }},
        {"SYNC:SOUR EXT", [](NormaInstrument& n) { n.sync_external(); }},
        {"SYNC:SOUR:LEV 50", [](NormaInstrument& n) { n.set_sync_level(50.0); }},
        {"SYNC:LEV:UNIT PCT",
         [](NormaInstrument& n) { n.set_sync_level_unit(LevelUnit::Percent); }},
        {"SYNC:LEV:UNIT ABS",
         [](NormaInstrument& n) { n.set_sync_level_unit(LevelUnit::Absolute); }},
        {"SYNC:SOUR:SLOP NEG", [](NormaInstrument& n) { n.set_sync_slope(Slope::Negative); }},
        {"SYNC:SOUR:FILT:LPAS:STAT ON", [](NormaInstrument& n) { n.set_sync_filter(true); }},
        {"SYNC:SOUR:FILT:LPAS:FREQ 1000",
         [](NormaInstrument& n) { n.set_sync_filter_frequency(1000.0); }},
        {"SYNC:TIM 2.5", [](NormaInstrument& n) { n.set_sync_timeout(2.5); }},
        {"SYNC:STAT?", [](NormaInstrument& n) { n.sync_enabled(); }, "1"},
        {"SYNC:SOUR?", [](NormaInstrument& n) { n.sync_source(); }, "VOLT1"},
        {"SYNC:SOUR:LEV?", [](NormaInstrument& n) { n.sync_level(); }, "0"},
        {"SYNC:LEV:UNIT?", [](NormaInstrument& n) { n.sync_level_unit(); }, "PCT"},
        {"SYNC:SOUR:SLOP?", [](NormaInstrument& n) { n.sync_slope(); }, "POS"},
        {"SYNC:SOUR:FILT:LPAS:STAT?", [](NormaInstrument& n) { n.sync_filter(); }, "0"},
        {"SYNC:SOUR:FILT:LPAS:FREQ?",
         [](NormaInstrument& n) { n.sync_filter_frequency(); }, "1000"},
        {"SYNC:TIM?", [](NormaInstrument& n) { n.sync_timeout(); }, "1"},
    });
}

TEST_CASE("SENSe measurement functions", "[conformance]") {
    check_all({
        {"FUNC \"VOLT1\",\"CURR1\",\"POW1:ACT\"",
         [](NormaInstrument& n) { n.set_functions(kFunctions); }},
        {"FUNC:ON:ALL", [](NormaInstrument& n) { n.enable_all_functions(); }},
        {"FUNC:OFF:ALL", [](NormaInstrument& n) { n.clear_functions(); }},
        {"FUNC:CONC ON", [](NormaInstrument& n) { n.set_concurrent(true); }},
        {"FUNC?", [](NormaInstrument& n) { n.functions(); }, "\"VOLT1\""},
        {"FUNC:COUN?", [](NormaInstrument& n) { n.function_count(); }, "1"},
        {"FUNC:CONC?", [](NormaInstrument& n) { n.concurrent(); }, "1"},
    });
}

TEST_CASE("Acquisition and TRIGger", "[conformance]") {
    check_all({
        {"INIT:CONT ON", [](NormaInstrument& n) { n.set_continuous(true); }},
        {"INIT:CONT OFF", [](NormaInstrument& n) { n.set_continuous(false); }},
        {"INIT", [](NormaInstrument& n) { n.initiate(); }},
        {"INIT:SEQ1", [](NormaInstrument& n) { n.initiate_sweep(SweepBlock::Block1); }},
        {"INIT:SEQ2", [](NormaInstrument& n) { n.initiate_sweep(SweepBlock::Block2); }},
        {"ABOR", [](NormaInstrument& n) { n.abort(); }},
        {"INIT:CONT?", [](NormaInstrument& n) { n.continuous(); }, "1"},
        {"TRIG:STAR:SOUR BUS", [](NormaInstrument& n) { n.set_trigger_start_source("BUS"); }},
        {"TRIG:STAR:LEV 1.5", [](NormaInstrument& n) { n.set_trigger_start_level(1.5); }},
        {"TRIG:STAR:SLOP POS",
         [](NormaInstrument& n) { n.set_trigger_start_slope(Slope::Positive); }},
        {"TRIG:STAR:TIME 2026,9,26,12,30,5",
         [](NormaInstrument& n) { n.set_trigger_start_time(kDate, kTime); }},
        {"TRIG:STOP:SOUR IMM", [](NormaInstrument& n) { n.set_trigger_stop_source("IMM"); }},
        {"TRIG:STOP:LEV -1.5", [](NormaInstrument& n) { n.set_trigger_stop_level(-1.5); }},
        {"TRIG:STOP:SLOP NEG",
         [](NormaInstrument& n) { n.set_trigger_stop_slope(Slope::Negative); }},
        {"TRIG:STOP:TIME 2026,9,26,12,30,5",
         [](NormaInstrument& n) { n.set_trigger_stop_time(kDate, kTime); }},
        {"TRIG:STAR:SOUR?", [](NormaInstrument& n) { n.trigger_start_source(); }, "IMM"},
        {"TRIG:STAR:LEV?", [](NormaInstrument& n) { n.trigger_start_level(); }, "0"},
        {"TRIG:STAR:SLOP?", [](NormaInstrument& n) { n.trigger_start_slope(); }, "POS"},
        {"TRIG:STOP:SOUR?", [](NormaInstrument& n) { n.trigger_stop_source(); }, "IMM"},
        {"TRIG:STOP:LEV?", [](NormaInstrument& n) { n.trigger_stop_level(); }, "0"},
        {"TRIG:STOP:SLOP?", [](NormaInstrument& n) { n.trigger_stop_slope(); }, "NEG"},
    });
}

TEST_CASE("Data queries", "[conformance]") {
    check_all({
        {"DATA?", [](NormaInstrument& n) { n.data(); }, "+2.30000E+02"},
        {"DATA? \"VOLT1\",\"CURR1\",\"POW1:ACT\"",
         [](NormaInstrument& n) { n.data(kFunctions); }, "1,2,3"},
        {"DATA:STAT?", [](NormaInstrument& n) { n.data_with_status(); }, "1,0"},
        {"DATA:STAT? \"VOLT1\"",
         [](NormaInstrument& n) { n.data_with_status({"VOLT1"}); }, "1,0"},
    });
}

TEST_CASE("CALCulate: spectrum and harmonics", "[conformance]") {
    check_all({
        {"CALC:HARM:ORD 3", [](NormaInstrument& n) { n.set_harmonic_order(3); }},
        {"CALC:TRAN:FREQ ONCE", [](NormaInstrument& n) { n.transform_once(); }},
        {"CALC:TRAN:FREQ:MODE STD",
         [](NormaInstrument& n) { n.set_transform_mode(TransformMode::Std); }},
        {"CALC:TRAN:FREQ:MODE FFT",
         [](NormaInstrument& n) { n.set_transform_mode(TransformMode::Fft); }},
        {"CALC:TRAN:FREQ:FUNC \"VOLT1\"",
         [](NormaInstrument& n) { n.set_transform_functions({"VOLT1"}); }},
        {"CALC:TRAN:FREQ:STAR 50", [](NormaInstrument& n) { n.set_transform_start(50.0); }},
        {"CALC:TRAN:FREQ:STOP 2500", [](NormaInstrument& n) { n.set_transform_stop(2500.0); }},
        {"CALC:TRAN:FREQ:CYCL 10", [](NormaInstrument& n) { n.set_transform_cycles(10); }},
        {"CALC:TRAN:FREQ:GRO HGR",
         [](NormaInstrument& n) { n.set_transform_grouping(HarmonicGrouping::HGroup); }},
        {"CALC:DATA?", [](NormaInstrument& n) { n.transform_data(); }, "1,2,3"},
        {"CALC:DATA? 16", [](NormaInstrument& n) { n.transform_data(16); }, "1,2"},
        {"CALC:DATA? 16,4", [](NormaInstrument& n) { n.transform_data(16, 4); }, "1,2"},
        {"CALC:DATA:PRE?", [](NormaInstrument& n) { n.transform_preamble(); }, "32,1,50,0"},
        {"CALC:DATA:THD?", [](NormaInstrument& n) { n.transform_thd(); }, "0.02,0.03"},
        {"CALC:HARM:ORD?", [](NormaInstrument& n) { n.harmonic_order(); }, "1"},
        {"CALC:TRAN:FREQ:MODE?", [](NormaInstrument& n) { n.transform_mode(); }, "FFT"},
        {"CALC:TRAN:FREQ:FUNC?",
         [](NormaInstrument& n) { n.transform_functions(); }, "\"VOLT1\""},
        {"CALC:TRAN:FREQ:STAR?", [](NormaInstrument& n) { n.transform_start(); }, "0"},
        {"CALC:TRAN:FREQ:STOP?", [](NormaInstrument& n) { n.transform_stop(); }, "5000"},
        {"CALC:TRAN:FREQ:CYCL?", [](NormaInstrument& n) { n.transform_cycles(); }, "10"},
        {"CALC:TRAN:FREQ:GRO?", [](NormaInstrument& n) { n.transform_grouping(); }, "COMP"},
    });
}

TEST_CASE("CALCulate: integration and power", "[conformance]") {
    check_all({
        {"CALC:INT:STAT ON", [](NormaInstrument& n) { n.set_integral_enabled(true); }},
        {"CALC:INT:FUNC \"POW1:ACT:INT\"",
         [](NormaInstrument& n) { n.set_integral_functions({"POW1:ACT:INT"}); }},
        {"CALC:INT:CLE", [](NormaInstrument& n) { n.clear_integral(); }},
        {"CALC:INT:CLE:AUTO OFF", [](NormaInstrument& n) { n.set_integral_auto_clear(false); }},
        {"CALC:INT:STAR:SOUR TIME",
         [](NormaInstrument& n) { n.set_integral_start_source(IntegralStartSource::Time); }},
        {"CALC:INT:STAR:SOUR CMD",
         [](NormaInstrument& n) { n.set_integral_start_source(IntegralStartSource::Command); }},
        {"CALC:INT:STAR", [](NormaInstrument& n) { n.start_integral(); }},
        {"CALC:INT:STAR:TIME 2026,9,26,12,30,5",
         [](NormaInstrument& n) { n.set_integral_start_time(kDate, kTime); }},
        {"CALC:INT:STOP:SOUR TINT",
         [](NormaInstrument& n) { n.set_integral_stop_source(IntegralStopSource::TimeInterval); }},
        {"CALC:INT:STOP", [](NormaInstrument& n) { n.stop_integral(); }},
        {"CALC:INT:STOP:TIME 2026,9,26,12,30,5",
         [](NormaInstrument& n) { n.set_integral_stop_time(kDate, kTime); }},
        {"CALC:INT:STOP:TINT 60", [](NormaInstrument& n) { n.set_integral_stop_interval(60.0); }},
        {"CALC:POW:CORR DELT",
         [](NormaInstrument& n) { n.set_power_correction(PowerCorrection::Delta); }},
        {"CALC:POW:CORR STAR",
         [](NormaInstrument& n) { n.set_power_correction(PowerCorrection::Star); }},
        {"CALC:POW:EFF:REF \"POW1:ACT\",\"POW2:ACT\"",
         [](NormaInstrument& n) { n.set_efficiency_reference("POW1:ACT", "POW2:ACT"); }},
        {"CALC:POW460:EFF:REF \"POW:ACT\",\"POW460:ACT\"",
         [](NormaInstrument& n) { n.set_efficiency_reference("POW:ACT", "POW460:ACT", 460); }},
        {"CALC:INT:STAT?", [](NormaInstrument& n) { n.integral_enabled(); }, "0"},
        {"CALC:INT:FUNC?", [](NormaInstrument& n) { n.integral_functions(); }, "\"POW1:ACT\""},
        {"CALC:INT:CLE:AUTO?", [](NormaInstrument& n) { n.integral_auto_clear(); }, "1"},
        {"CALC:INT:STAR:SOUR?", [](NormaInstrument& n) { n.integral_start_source(); }, "CMD"},
        {"CALC:INT:STOP:SOUR?", [](NormaInstrument& n) { n.integral_stop_source(); }, "CMD"},
        {"CALC:INT:STOP:TINT?", [](NormaInstrument& n) { n.integral_stop_interval(); }, "60"},
        {"CALC:POW:CORR?", [](NormaInstrument& n) { n.power_correction(); }, "STAR"},
    });
}

TEST_CASE("Memory recording: SENSe:SWEep and TRACe", "[conformance]") {
    check_all({
        {"SWE1:STAT ON",
         [](NormaInstrument& n) { n.set_sweep_enabled(SweepBlock::Block1, true); }},
        {"SWE2:STAT OFF",
         [](NormaInstrument& n) { n.set_sweep_enabled(SweepBlock::Block2, false); }},
        {"SWE1:TIME 2", [](NormaInstrument& n) { n.set_sweep_time(SweepBlock::Block1, 2.0); }},
        {"SWE2:TIME MAX", [](NormaInstrument& n) { n.set_sweep_time_max(SweepBlock::Block2); }},
        {"SWE1:OFFS:TIME -0.5",
         [](NormaInstrument& n) { n.set_sweep_offset_time(SweepBlock::Block1, -0.5); }},
        {"SWE1:COUN 100",
         [](NormaInstrument& n) { n.set_sweep_count(SweepBlock::Block1, 100); }},
        {"SWE1:SFAC 4",
         [](NormaInstrument& n) { n.set_sweep_sparsing(SweepBlock::Block1, 4); }},
        {"SWE1:FUNC \"VOLT1\",\"CURR1\"",
         [](NormaInstrument& n) { n.set_sweep_functions(SweepBlock::Block1, {"VOLT1", "CURR1"}); }},
        {"SWE1:STAT?", [](NormaInstrument& n) { n.sweep_enabled(SweepBlock::Block1); }, "0"},
        {"SWE1:TIME?", [](NormaInstrument& n) { n.sweep_time(SweepBlock::Block1); }, "1"},
        {"SWE1:OFFS:TIME?",
         [](NormaInstrument& n) { n.sweep_offset_time(SweepBlock::Block1); }, "0"},
        {"SWE1:POINTS?", [](NormaInstrument& n) { n.sweep_points(SweepBlock::Block1); }, "10"},
        {"SWE1:OFFS:POINTS?",
         [](NormaInstrument& n) { n.sweep_offset_points(SweepBlock::Block1); }, "0"},
        {"SWE1:COUN?", [](NormaInstrument& n) { n.sweep_count(SweepBlock::Block1); }, "1"},
        {"SWE1:SFAC?", [](NormaInstrument& n) { n.sweep_sparsing(SweepBlock::Block1); }, "1"},
        {"SWE1:FUNC?",
         [](NormaInstrument& n) { n.sweep_functions(SweepBlock::Block1); }, "\"VOLT1\""},
        {"TRAC:PRE? 1",
         [](NormaInstrument& n) { n.trace_preamble(SweepBlock::Block1); }, "10,1,0.1,0"},
        {"TRAC? 1", [](NormaInstrument& n) { n.trace_data(SweepBlock::Block1); }, "1,2,3"},
        {"TRAC? 1,10,2,4",
         [](NormaInstrument& n) { n.trace_data(SweepBlock::Block1, 10, 2, 4); }, "1,2"},
        {"TRAC:STAT? 2,5",
         [](NormaInstrument& n) { n.trace_status(SweepBlock::Block2, 5); }, "0,0"},
        {"TRAC:FREE?", [](NormaInstrument& n) { n.trace_free(); }, "4096"},
        {"TRAC:CAT:LEN?", [](NormaInstrument& n) { n.trace_length(); }, "0"},
        {"TRAC:DEL:ALL", [](NormaInstrument& n) { n.delete_traces(); }},
    });
}

TEST_CASE("FORMat", "[conformance]") {
    check_all({
        {"FORM ASC,6", [](NormaInstrument& n) { n.set_data_format(DataFormat::Ascii, 6); }},
        {"FORM ASC", [](NormaInstrument& n) { n.set_data_format(DataFormat::Ascii); }},
        {"FORM REAL,64", [](NormaInstrument& n) { n.set_data_format(DataFormat::Real, 64); }},
        {"FORM:STAT INT,16",
         [](NormaInstrument& n) { n.set_status_format(DataFormat::Integer, 16); }},
        {"FORM:BORD SWAP", [](NormaInstrument& n) { n.set_byte_order(ByteOrder::Swapped); }},
        {"FORM:BORD NORM", [](NormaInstrument& n) { n.set_byte_order(ByteOrder::Normal); }},
        {"FORM:TRAN ON", [](NormaInstrument& n) { n.set_transpose(true); }},
        {"FORM?", [](NormaInstrument& n) { n.data_format(); }, "ASC,6"},
        {"FORM:STAT?", [](NormaInstrument& n) { n.status_format(); }, "ASC,16"},
        {"FORM:BORD?", [](NormaInstrument& n) { n.byte_order(); }, "NORM"},
        {"FORM:TRAN?", [](NormaInstrument& n) { n.transpose(); }, "0"},
    });
}

TEST_CASE("DISPlay and OUTPut", "[conformance]") {
    check_all({
        {"DISP:WIND:STAT OFF", [](NormaInstrument& n) { n.set_display_enabled(false); }},
        {"DISP:USER:FUNC \"VOLT1\",\"CURR1\"",
         [](NormaInstrument& n) { n.set_display_functions({"VOLT1", "CURR1"}); }},
        {"OUTP9:STAT ON", [](NormaInstrument& n) { n.set_output_enabled(true); }},
        {"DISP:WIND:STAT?", [](NormaInstrument& n) { n.display_enabled(); }, "1"},
        {"DISP:USER:FUNC?", [](NormaInstrument& n) { n.display_functions(); }, "\"VOLT1\""},
        {"OUTP9:STAT?", [](NormaInstrument& n) { n.output_enabled(); }, "0"},
    });
}

TEST_CASE("SYSTem and TIMer", "[conformance]") {
    check_all({
        {"SYST:KLOC REM", [](NormaInstrument& n) { n.set_key_lock(KeyLock::Remote); }},
        {"SYST:KLOC OFF", [](NormaInstrument& n) { n.set_key_lock(KeyLock::Off); }},
        {"SYST:DATE 2026,9,26", [](NormaInstrument& n) { n.set_date(kDate); }},
        {"SYST:TIME 12,30,5", [](NormaInstrument& n) { n.set_time(kTime); }},
        {"SYST:COMM:GPIB:ADDR 5", [](NormaInstrument& n) { n.set_gpib_address(5); }},
        {"SYST:COMM:SER:BAUD 115200", [](NormaInstrument& n) { n.set_serial_baud(115200); }},
        {"SYST:LANG \"DEFault\"", [](NormaInstrument& n) { n.set_language("DEFault"); }},
        {"TIM:RES", [](NormaInstrument& n) { n.reset_timer(); }},
        {"SYST:VERS?", [](NormaInstrument& n) { n.scpi_version(); }, "1999.0"},
        {"SYST:KLOC?", [](NormaInstrument& n) { n.key_lock(); }, "OFF"},
        {"SYST:DATE?", [](NormaInstrument& n) { n.date(); }, "2026,9,26"},
        {"SYST:TIME?", [](NormaInstrument& n) { n.time_of_day(); }, "12,30,5"},
        {"SYST:COMM:GPIB:ADDR?", [](NormaInstrument& n) { n.gpib_address(); }, "5"},
        {"SYST:COMM:SER:BAUD?", [](NormaInstrument& n) { n.serial_baud(); }, "115200"},
        {"SYST:LANG?", [](NormaInstrument& n) { n.language(); }, "\"DEFault\""},
        {"SYST:ERR:ALL?", [](NormaInstrument& n) { n.read_errors_at_once(); }, "0,\"No error\""},
        {"TIM:RES:TIME?", [](NormaInstrument& n) { n.timer_reset_time(); }, "0"},
    });
}

TEST_CASE("STATus registers", "[conformance]") {
    check_all({
        {"STAT:OPER:COND?",
         [](NormaInstrument& n) { n.status(StatusRegister::Operation, RegisterPart::Condition); },
         "1792"},
        {"STAT:OPER:COND?", [](NormaInstrument& n) { n.status_operation_condition(); }, "1792"},
        {"STAT:OPER:EVEN?",
         [](NormaInstrument& n) { n.status(StatusRegister::Operation, RegisterPart::Event); }, "0"},
        {"STAT:OPER:ENAB?",
         [](NormaInstrument& n) { n.status(StatusRegister::Operation, RegisterPart::Enable); }, "0"},
        {"STAT:OPER:PTR?",
         [](NormaInstrument& n) {
             n.status(StatusRegister::Operation, RegisterPart::PositiveTransition);
         },
         "65535"},
        {"STAT:OPER:NTR?",
         [](NormaInstrument& n) {
             n.status(StatusRegister::Operation, RegisterPart::NegativeTransition);
         },
         "0"},
        {"STAT:QUES:COND?",
         [](NormaInstrument& n) { n.status(StatusRegister::Questionable, RegisterPart::Condition); },
         "0"},
        {"STAT:QUES:VOLT:COND?",
         [](NormaInstrument& n) {
             n.status(StatusRegister::QuestionableVoltage, RegisterPart::Condition);
         },
         "0"},
        {"STAT:QUES:CURR:EVEN?",
         [](NormaInstrument& n) {
             n.status(StatusRegister::QuestionableCurrent, RegisterPart::Event);
         },
         "0"},
        {"STAT:OPER:ENAB 1024",
         [](NormaInstrument& n) {
             n.set_status(StatusRegister::Operation, RegisterPart::Enable, 1024);
         }},
        {"STAT:QUES:CURR:PTR 63",
         [](NormaInstrument& n) {
             n.set_status(StatusRegister::QuestionableCurrent, RegisterPart::PositiveTransition, 63);
         }},
        {"STAT:QUES:NTR 32",
         [](NormaInstrument& n) {
             n.set_status(StatusRegister::Questionable, RegisterPart::NegativeTransition, 32);
         }},
        {"STAT:QUES:VOLT:ENAB 1",
         [](NormaInstrument& n) {
             n.set_status(StatusRegister::QuestionableVoltage, RegisterPart::Enable, 1);
         }},
    });
}

TEST_CASE("prepare() puts the link into a state this library can parse",
          "[conformance]") {
    // A previous session may have left FORMat on REAL or CONCurrent off, and
    // both survive a disconnect, so connecting is not enough.
    auto transport = std::make_unique<MockTransport>();
    transport->open();
    MockTransport* mock = transport.get();
    NormaInstrument norma(std::move(transport));
    mock->responses.push_back("0,\"No error\"");

    norma.prepare();

    REQUIRE(mock->writes.size() == 4);
    CHECK(mock->writes[0] == "*CLS\n");
    CHECK(mock->writes[1] == "FORM ASC,6\n");
    CHECK(mock->writes[2] == "FUNC:CONC ON\n");
    CHECK(mock->writes[3] == "SYST:ERR?\n");
}

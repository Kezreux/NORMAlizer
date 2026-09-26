"""The pythonic, fully typed facade over the compiled ``flukenorma._core.Norma``."""

from __future__ import annotations

from types import TracebackType
from typing import Iterable, List, Optional, Sequence, Type, TypeVar

from ._bootstrap import core as _core
from ._protocols import CoreEnumLike, NormaLike
from ._types import (
    ByteOrder,
    Coupling,
    DataFormat,
    DataFormatSetting,
    DataPreamble,
    Date,
    HarmonicGrouping,
    Identification,
    IntegralStartSource,
    IntegralStopSource,
    KeyLock,
    LevelUnit,
    PowerCorrection,
    Reading,
    RegisterPart,
    ScpiErrorInfo,
    Shunt,
    Slope,
    StatusRegister,
    SweepBlock,
    Time,
    TransformMode,
    WiringSystem,
)

#: The instrument's TCP port is fixed to 23 (the telnet port).
DEFAULT_PORT: int = int(_core.DEFAULT_PORT)

#: Bounds of the averaging time, in seconds (``APERture``).
MIN_APERTURE: float = float(_core.MIN_APERTURE)
MAX_APERTURE: float = float(_core.MAX_APERTURE)

#: Highest phase index and hardware input channel the largest model has.
MAX_PHASE: int = int(_core.MAX_PHASE)
MAX_INPUT_CHANNEL: int = int(_core.MAX_INPUT_CHANNEL)

_E = TypeVar("_E")


def _to_core(value: object, core_type_name: str) -> CoreEnumLike:
    """Bridges one of this package's enums to the matching ``_core`` enum by name.

    Member names are kept identical on both sides, which makes the mapping
    mechanical and keeps a renamed member a loud ``AttributeError`` rather than a
    silently wrong command.
    """
    return getattr(getattr(_core, core_type_name), getattr(value, "name"))


def _from_core(enum_type: Type[_E], value: CoreEnumLike) -> _E:
    """Bridges a ``_core`` enum value back to this package's enum, by name."""
    return enum_type[value.name]  # type: ignore[index]


class Norma:
    """A Fluke NORMA 4000/5000 power analyzer, controlled over TCP/SCPI.

    Compared to the raw binding, this class returns plain dataclasses
    (:class:`Identification`, :class:`Reading`, :class:`ScpiErrorInfo`,
    :class:`Date`, :class:`Time`, :class:`DataFormatSetting`,
    :class:`DataPreamble`), uses Python enums and flags, and exposes instrument
    state as properties (``timeout``, ``aperture``, ``wiring_system``,
    ``functions``, ``continuous``, ``concurrent``, ``harmonic_order``, ...).
    All timeouts, apertures and intervals are in seconds.

    Typical workflow (matches the manual)::

        with Norma.connect("192.168.1.100") as norma:
            norma.prepare()                 # ASCII transfer format, clean queue
            norma.reset()
            norma.wiring_system = WiringSystem.THREE_WATTMETER
            norma.sync_to_voltage(1)
            norma.set_voltage_range(1, 300.0)
            norma.set_current_autorange(1)
            norma.aperture = 1.0
            norma.functions = [fn.voltage(1), fn.current(1), fn.active_power(1)]
            norma.continuous = True
            ...
            reading = norma.read()
            print(reading["VOLT1"].value)

    ``phase`` follows the manual's phase suffixes (1..6 = L1..L6,
    12/23/31/... = phase-to-phase, 0 = totals of the first three-phase system);
    ``channel`` is a hardware input channel 1..12, where the current inputs are
    the odd numbers.

    Any object satisfying :class:`NormaLike` can be wrapped, so tests can
    inject a fake instead of a live instrument: ``Norma(FakeInstrument())``.
    """

    def __init__(self, instrument: NormaLike) -> None:
        self._instrument = instrument

    @classmethod
    def connect(
        cls, host: str, port: int = DEFAULT_PORT, *, timeout: float = 5.0
    ) -> Norma:
        """Connect over TCP/Ethernet (the port is fixed to 23 on the instrument).

        The instrument's settings are left alone — it may already be configured
        for a running measurement. Call :meth:`prepare` (or :meth:`reset`) when
        known-good defaults are wanted.
        """
        return cls(_core.Norma(host, port, timeout))

    @property
    def instrument(self) -> NormaLike:
        """The underlying compiled instrument object (escape hatch)."""
        return self._instrument

    def __repr__(self) -> str:
        state = "open" if self.is_open else "closed"
        return f"<Norma {state}>"

    # -- Lifecycle -----------------------------------------------------------
    def close(self) -> None:
        self._instrument.close()

    @property
    def is_open(self) -> bool:
        return self._instrument.is_open

    def prepare(self) -> None:
        """Bring the link into a state this library can parse.

        Clears the status registers, switches the transfer format to ASCii,
        enables concurrent functions and drains a stale error queue — without
        touching the measurement configuration. ``FORMat`` survives a
        disconnect, so an instrument left in ``REAL,64`` by a previous session
        would otherwise answer every measurement query with a binary block.
        """
        self._instrument.prepare()

    def __enter__(self) -> Norma:
        return self

    def __exit__(
        self,
        exc_type: Optional[Type[BaseException]],
        exc: Optional[BaseException],
        tb: Optional[TracebackType],
    ) -> None:
        self.close()

    # -- Raw SCPI escape hatches ----------------------------------------------
    def write(self, scpi: str) -> None:
        """Send a raw SCPI setting command, e.g. ``"*RST"``.

        Several commands can be combined into one line with ``;`` as the manual
        describes. A string containing a line terminator is refused: it would put
        a second command on the wire whose response nobody reads, leaving every
        later query one answer behind.
        """
        self._instrument.write(scpi)

    def query(self, scpi: str) -> str:
        """Send a raw SCPI query, e.g. ``"*IDN?"``, and return the response line."""
        return self._instrument.query(scpi)

    # -- IEEE 488.2 common commands ---------------------------------------------
    def identify(self) -> Identification:
        """``*IDN?``"""
        return Identification.from_core(self._instrument.identify())

    def reset(self) -> None:
        """``*RST``"""
        self._instrument.reset()

    def clear_status(self) -> None:
        """``*CLS``"""
        self._instrument.clear_status()

    def options(self) -> str:
        """``*OPT?`` — the installed options."""
        return self._instrument.options()

    def learn(self) -> str:
        """``*LRN?`` — the current setup as a command line."""
        return self._instrument.learn()

    def wait_operation_complete(self, timeout: float = 30.0) -> None:
        """``*OPC?`` — blocks until all previous commands have completed."""
        self._instrument.wait_operation_complete(timeout)

    def set_operation_complete_flag(self) -> None:
        """``*OPC`` — sets the ESR bit when done; does not block."""
        self._instrument.set_operation_complete_flag()

    def wait_pending_operations(self) -> None:
        """``*WAI``"""
        self._instrument.wait_pending_operations()

    def trigger(self) -> None:
        """``*TRG``"""
        self._instrument.trigger()

    @property
    def event_status_enable(self) -> int:
        """``*ESE`` — mask of the event status register feeding the ESB bit."""
        return self._instrument.event_status_enable()

    @event_status_enable.setter
    def event_status_enable(self, mask: int) -> None:
        self._instrument.set_event_status_enable(mask)

    def event_status(self) -> int:
        """``*ESR?`` — the event status register; clears on read."""
        return self._instrument.event_status()

    @property
    def service_request_enable(self) -> int:
        """``*SRE`` — mask of the status byte that raises a service request."""
        return self._instrument.service_request_enable()

    @service_request_enable.setter
    def service_request_enable(self, mask: int) -> None:
        self._instrument.set_service_request_enable(mask)

    def status_byte(self) -> int:
        """``*STB?`` — see :class:`StatusByte` for the bit meanings."""
        return self._instrument.status_byte()

    def save_setup(self, slot: int) -> None:
        """``*SAV`` — store the current setup in slot 10..24."""
        self._instrument.save_setup(slot)

    def recall_setup(self, slot: int) -> None:
        """``*RCL`` — load a stored setup (slots 1, 2, 10..24)."""
        self._instrument.recall_setup(slot)

    # -- ROUTe ------------------------------------------------------------------
    @property
    def wiring_system(self) -> WiringSystem:
        """``ROUTe:SYSTem`` — assign to change, read to query."""
        return _from_core(WiringSystem, self._instrument.wiring_system())

    @wiring_system.setter
    def wiring_system(self, system: WiringSystem) -> None:
        self._instrument.set_wiring_system(_to_core(system, "WiringSystem"))

    # -- INPut -------------------------------------------------------------------
    def set_input_coupling(self, channel: int, coupling: Coupling) -> None:
        """``INPut<n>:COUPling``"""
        self._instrument.set_input_coupling(channel, _to_core(coupling, "Coupling"))

    def input_coupling(self, channel: int) -> Coupling:
        """``INPut<n>:COUPling?``"""
        return _from_core(Coupling, self._instrument.input_coupling(channel))

    def set_input_gain(self, channel: int, gain: float) -> None:
        """``INPut<n>:GAIN`` — external shunt factor; current channels only."""
        self._instrument.set_input_gain(channel, gain)

    def input_gain(self, channel: int) -> float:
        """``INPut<n>:GAIN?``"""
        return self._instrument.input_gain(channel)

    def set_input_filter(self, channel: int, on: bool = True) -> None:
        """``INPut<n>:FILTer:STATe``"""
        self._instrument.set_input_filter(channel, on)

    def input_filter(self, channel: int) -> bool:
        """``INPut<n>:FILTer:STATe?``"""
        return self._instrument.input_filter(channel)

    def input_filter_frequency(self, channel: int) -> float:
        """``INPut<n>:FILTer:LPASs:FREQuency?`` — the cut-off frequency in hertz."""
        return self._instrument.input_filter_frequency(channel)

    def set_input_shunt(self, channel: int, shunt: Shunt) -> None:
        """``INPut<n>:SHUNt`` — current channels only."""
        self._instrument.set_input_shunt(channel, _to_core(shunt, "Shunt"))

    def input_shunt(self, channel: int) -> Shunt:
        """``INPut<n>:SHUNt?``"""
        return _from_core(Shunt, self._instrument.input_shunt(channel))

    # -- SENSe: ranging, scaling, averaging ---------------------------------------
    def set_voltage_range(self, phase: int, volts: float) -> None:
        """``VOLT<n>:RANGe`` (turns autorange off)."""
        self._instrument.set_voltage_range(phase, volts)

    def voltage_range(self, phase: int) -> float:
        """``VOLT<n>:RANGe?``"""
        return self._instrument.voltage_range(phase)

    def set_voltage_autorange(self, phase: int, on: bool = True) -> None:
        """``VOLT<n>:RANGe:AUTO``"""
        self._instrument.set_voltage_autorange(phase, on)

    def voltage_autorange(self, phase: int) -> bool:
        """``VOLT<n>:RANGe:AUTO?``"""
        return self._instrument.voltage_autorange(phase)

    def voltage_ranges(self, phase: int) -> List[float]:
        """``VOLT<n>:RANGe:LIST?`` — the ranges the instrument can select."""
        return list(self._instrument.voltage_ranges(phase))

    def set_voltage_scale(self, phase: int, ratio: float) -> None:
        """``VOLT<n>:SCALe`` — transducer ratio."""
        self._instrument.set_voltage_scale(phase, ratio)

    def voltage_scale(self, phase: int) -> float:
        """``VOLT<n>:SCALe?``"""
        return self._instrument.voltage_scale(phase)

    def set_current_range(self, phase: int, amps: float) -> None:
        """``CURR<n>:RANGe`` (turns autorange off)."""
        self._instrument.set_current_range(phase, amps)

    def current_range(self, phase: int) -> float:
        """``CURR<n>:RANGe?``"""
        return self._instrument.current_range(phase)

    def set_current_autorange(self, phase: int, on: bool = True) -> None:
        """``CURR<n>:RANGe:AUTO``"""
        self._instrument.set_current_autorange(phase, on)

    def current_autorange(self, phase: int) -> bool:
        """``CURR<n>:RANGe:AUTO?``"""
        return self._instrument.current_autorange(phase)

    def current_ranges(self, phase: int) -> List[float]:
        """``CURR<n>:RANGe:LIST?``"""
        return list(self._instrument.current_ranges(phase))

    def set_current_scale(self, phase: int, ratio: float) -> None:
        """``CURR<n>:SCALe`` — transducer ratio."""
        self._instrument.set_current_scale(phase, ratio)

    def current_scale(self, phase: int) -> float:
        """``CURR<n>:SCALe?``"""
        return self._instrument.current_scale(phase)

    @property
    def aperture(self) -> float:
        """``APERture`` — averaging time in seconds (0.015..3600)."""
        return self._instrument.aperture()

    @aperture.setter
    def aperture(self, seconds: float) -> None:
        self._instrument.set_aperture(seconds)

    @property
    def sampling_frequency(self) -> float:
        """``SWEep:FREQuency?`` — the instrument's sampling rate in hertz."""
        return self._instrument.sampling_frequency()

    # -- SYNC ----------------------------------------------------------------------
    @property
    def sync_enabled(self) -> bool:
        """``SYNC:STATe``"""
        return self._instrument.sync_enabled()

    @sync_enabled.setter
    def sync_enabled(self, on: bool) -> None:
        self._instrument.set_sync_enabled(on)

    @property
    def sync_source(self) -> str:
        """``SYNC:SOURce`` — e.g. ``"VOLT1"``, ``"CURR3"``, ``"EXT"``."""
        return self._instrument.sync_source()

    @sync_source.setter
    def sync_source(self, source: str) -> None:
        self._instrument.set_sync_source(source)

    def sync_to_voltage(self, phase: int) -> None:
        """``SYNC:SOURce VOLTage<phase>``"""
        self._instrument.sync_to_voltage(phase)

    def sync_to_current(self, phase: int) -> None:
        """``SYNC:SOURce CURRent<phase>``"""
        self._instrument.sync_to_current(phase)

    def sync_external(self) -> None:
        """``SYNC:SOURce EXTernal``"""
        self._instrument.sync_external()

    @property
    def sync_level(self) -> float:
        """``SYNC:SOURce:LEVel`` — in the unit set by :attr:`sync_level_unit`."""
        return self._instrument.sync_level()

    @sync_level.setter
    def sync_level(self, level: float) -> None:
        self._instrument.set_sync_level(level)

    @property
    def sync_level_unit(self) -> LevelUnit:
        """``SYNC:LEVel:UNIT``"""
        return _from_core(LevelUnit, self._instrument.sync_level_unit())

    @sync_level_unit.setter
    def sync_level_unit(self, unit: LevelUnit) -> None:
        self._instrument.set_sync_level_unit(_to_core(unit, "LevelUnit"))

    @property
    def sync_slope(self) -> Slope:
        """``SYNC:SOURce:SLOPe``"""
        return _from_core(Slope, self._instrument.sync_slope())

    @sync_slope.setter
    def sync_slope(self, slope: Slope) -> None:
        self._instrument.set_sync_slope(_to_core(slope, "Slope"))

    @property
    def sync_filter(self) -> bool:
        """``SYNC:SOURce:FILTer:LPASs:STATe``"""
        return self._instrument.sync_filter()

    @sync_filter.setter
    def sync_filter(self, on: bool) -> None:
        self._instrument.set_sync_filter(on)

    @property
    def sync_filter_frequency(self) -> float:
        """``SYNC:SOURce:FILTer:LPASs:FREQuency`` — 100, 1e3 or 10e3 hertz."""
        return self._instrument.sync_filter_frequency()

    @sync_filter_frequency.setter
    def sync_filter_frequency(self, hertz: float) -> None:
        self._instrument.set_sync_filter_frequency(hertz)

    @property
    def sync_timeout(self) -> float:
        """``SYNC:TIMeout`` — seconds to wait for a synchronization signal."""
        return self._instrument.sync_timeout()

    @sync_timeout.setter
    def sync_timeout(self, seconds: float) -> None:
        self._instrument.set_sync_timeout(seconds)

    # -- SENSe: measurement functions ------------------------------------------------
    @property
    def functions(self) -> List[str]:
        """``SENSe:FUNCtion`` — the configured ``<function>`` names (see :mod:`.fn`)."""
        return list(self._instrument.functions())

    @functions.setter
    def functions(self, functions: Iterable[str]) -> None:
        self._instrument.set_functions(list(functions))

    def enable_all_functions(self) -> None:
        """``SENSe:FUNCtion:ON:ALL`` — everything measurable at once."""
        self._instrument.enable_all_functions()

    def clear_functions(self) -> None:
        """``SENSe:FUNCtion:OFF:ALL``"""
        self._instrument.clear_functions()

    @property
    def function_count(self) -> int:
        """``SENSe:FUNCtion:COUNt?``"""
        return self._instrument.function_count()

    @property
    def concurrent(self) -> bool:
        """``SENSe:FUNCtion:CONCurrent`` — several functions at once when true.

        With this off, assigning :attr:`functions` acts as a one-of-n switch.
        """
        return self._instrument.concurrent()

    @concurrent.setter
    def concurrent(self, on: bool) -> None:
        self._instrument.set_concurrent(on)

    # -- Acquisition -------------------------------------------------------------------
    @property
    def continuous(self) -> bool:
        """``INITiate:CONTinuous`` — free-run measurement when true."""
        return self._instrument.continuous()

    @continuous.setter
    def continuous(self, on: bool) -> None:
        self._instrument.set_continuous(on)

    def set_continuous(self, on: bool = True) -> None:
        """``INITiate:CONTinuous`` (the method form of :attr:`continuous`)."""
        self._instrument.set_continuous(on)

    def initiate(self) -> None:
        """``INITiate`` — a single measurement."""
        self._instrument.initiate()

    def initiate_sweep(self, block: SweepBlock) -> None:
        """``INITiate:SEQuence1|2`` — start a memory recording."""
        self._instrument.initiate_sweep(_to_core(block, "SweepBlock"))

    def abort(self) -> None:
        """``ABORt``"""
        self._instrument.abort()

    # -- TRIGger --------------------------------------------------------------------------
    @property
    def trigger_start_source(self) -> str:
        """``TRIGger:STARt:SOURce`` — ``BUS``, ``TIME``, ``IMM``, ``MAN``, ``SYNC``
        or a ``<function>`` name."""
        return self._instrument.trigger_start_source()

    @trigger_start_source.setter
    def trigger_start_source(self, source: str) -> None:
        self._instrument.set_trigger_start_source(source)

    @property
    def trigger_start_level(self) -> float:
        """``TRIGger:STARt:LEVel``"""
        return self._instrument.trigger_start_level()

    @trigger_start_level.setter
    def trigger_start_level(self, level: float) -> None:
        self._instrument.set_trigger_start_level(level)

    @property
    def trigger_start_slope(self) -> Slope:
        """``TRIGger:STARt:SLOPe``"""
        return _from_core(Slope, self._instrument.trigger_start_slope())

    @trigger_start_slope.setter
    def trigger_start_slope(self, slope: Slope) -> None:
        self._instrument.set_trigger_start_slope(_to_core(slope, "Slope"))

    def set_trigger_start_time(self, date: Date, time: Time) -> None:
        """``TRIGger:STARt:TIME``"""
        self._instrument.set_trigger_start_time(
            _core.Date(date.year, date.month, date.day),
            _core.Time(time.hours, time.minutes, time.seconds),
        )

    @property
    def trigger_stop_source(self) -> str:
        """``TRIGger:STOP:SOURce``"""
        return self._instrument.trigger_stop_source()

    @trigger_stop_source.setter
    def trigger_stop_source(self, source: str) -> None:
        self._instrument.set_trigger_stop_source(source)

    @property
    def trigger_stop_level(self) -> float:
        """``TRIGger:STOP:LEVel``"""
        return self._instrument.trigger_stop_level()

    @trigger_stop_level.setter
    def trigger_stop_level(self, level: float) -> None:
        self._instrument.set_trigger_stop_level(level)

    @property
    def trigger_stop_slope(self) -> Slope:
        """``TRIGger:STOP:SLOPe``"""
        return _from_core(Slope, self._instrument.trigger_stop_slope())

    @trigger_stop_slope.setter
    def trigger_stop_slope(self, slope: Slope) -> None:
        self._instrument.set_trigger_stop_slope(_to_core(slope, "Slope"))

    def set_trigger_stop_time(self, date: Date, time: Time) -> None:
        """``TRIGger:STOP:TIME``"""
        self._instrument.set_trigger_stop_time(
            _core.Date(date.year, date.month, date.day),
            _core.Time(time.hours, time.minutes, time.seconds),
        )

    # -- Data ---------------------------------------------------------------------------------
    def data(self, *functions: str) -> List[float]:
        """``DATA?`` — averaged values of the configured (or given) functions.

        Undefined/unavailable values come back as NaN. Passing an explicit list
        also *replaces* the configured :attr:`functions` on the instrument, which
        is what the manual means by ``DATA?`` invalidating ``FUNCtion[:ON]``.
        """
        return list(self._instrument.data(list(functions)))

    def read(self, *functions: str) -> Reading:
        """``DATA:STATus?`` — values plus status, labelled with their function names.

        When called without arguments the configured function list is queried
        first (one extra SCPI round-trip) so each value can be labelled. Passing
        an explicit list replaces the configured list, as :meth:`data` does.
        """
        names = list(functions) if functions else list(self._instrument.functions())
        core_reading = self._instrument.data_with_status(list(functions))
        return Reading.from_core(core_reading, names)

    # -- CALCulate: spectrum and harmonics -------------------------------------------------------
    @property
    def harmonic_order(self) -> int:
        """``CALCulate:HARMonic:ORDer`` — which harmonic ``fn.harmonic()`` reports."""
        return self._instrument.harmonic_order()

    @harmonic_order.setter
    def harmonic_order(self, order: int) -> None:
        self._instrument.set_harmonic_order(order)

    def transform_once(self) -> None:
        """``CALCulate:TRANsform:FREQuency ONCE`` — compute one spectrum."""
        self._instrument.transform_once()

    @property
    def transform_mode(self) -> TransformMode:
        """``CALCulate:TRANsform:FREQuency:MODE``"""
        return _from_core(TransformMode, self._instrument.transform_mode())

    @transform_mode.setter
    def transform_mode(self, mode: TransformMode) -> None:
        self._instrument.set_transform_mode(_to_core(mode, "TransformMode"))

    @property
    def transform_functions(self) -> List[str]:
        """``CALCulate:TRANsform:FREQuency:FUNCtion``"""
        return list(self._instrument.transform_functions())

    @transform_functions.setter
    def transform_functions(self, functions: Iterable[str]) -> None:
        self._instrument.set_transform_functions(list(functions))

    @property
    def transform_start(self) -> float:
        """``CALCulate:TRANsform:FREQuency:STARt`` — in hertz (FFT/DFT only)."""
        return self._instrument.transform_start()

    @transform_start.setter
    def transform_start(self, hertz: float) -> None:
        self._instrument.set_transform_start(hertz)

    @property
    def transform_stop(self) -> float:
        """``CALCulate:TRANsform:FREQuency:STOP`` — in hertz (FFT/DFT only)."""
        return self._instrument.transform_stop()

    @transform_stop.setter
    def transform_stop(self, hertz: float) -> None:
        self._instrument.set_transform_stop(hertz)

    @property
    def transform_cycles(self) -> int:
        """``CALCulate:TRANsform:FREQuency:CYCLes`` — 4, 6, 8, 10 or 12 (STD mode)."""
        return self._instrument.transform_cycles()

    @transform_cycles.setter
    def transform_cycles(self, cycles: int) -> None:
        self._instrument.set_transform_cycles(cycles)

    @property
    def transform_grouping(self) -> HarmonicGrouping:
        """``CALCulate:TRANsform:FREQuency:GROuping`` (STD mode)."""
        return _from_core(HarmonicGrouping, self._instrument.transform_grouping())

    @transform_grouping.setter
    def transform_grouping(self, grouping: HarmonicGrouping) -> None:
        self._instrument.set_transform_grouping(_to_core(grouping, "HarmonicGrouping"))

    def transform_data(self, count: int = 0, offset: int = 0) -> List[float]:
        """``CALCulate:DATA?`` — the spectrum. ``count`` 0 asks for everything."""
        return list(self._instrument.transform_data(count, offset))

    def transform_preamble(self) -> DataPreamble:
        """``CALCulate:DATA:PREamble?`` — the shape of the spectrum block."""
        return DataPreamble.from_core(self._instrument.transform_preamble())

    def transform_thd(self) -> List[float]:
        """``CALCulate:DATA:THD?`` — total harmonic distortion (STD mode only)."""
        return list(self._instrument.transform_thd())

    # -- CALCulate: integration (energy) -----------------------------------------------------------
    @property
    def integral_enabled(self) -> bool:
        """``CALCulate:INTegral:STATe``.

        The ``fn.integral*`` function modifiers only work while this is on;
        otherwise those values come back flagged Not available.
        """
        return self._instrument.integral_enabled()

    @integral_enabled.setter
    def integral_enabled(self, on: bool) -> None:
        self._instrument.set_integral_enabled(on)

    @property
    def integral_functions(self) -> List[str]:
        """``CALCulate:INTegral:FUNCtion``"""
        return list(self._instrument.integral_functions())

    @integral_functions.setter
    def integral_functions(self, functions: Iterable[str]) -> None:
        self._instrument.set_integral_functions(list(functions))

    def clear_integral(self) -> None:
        """``CALCulate:INTegral:CLEar``"""
        self._instrument.clear_integral()

    @property
    def integral_auto_clear(self) -> bool:
        """``CALCulate:INTegral:CLEar:AUTO``"""
        return self._instrument.integral_auto_clear()

    @integral_auto_clear.setter
    def integral_auto_clear(self, on: bool) -> None:
        self._instrument.set_integral_auto_clear(on)

    @property
    def integral_start_source(self) -> IntegralStartSource:
        """``CALCulate:INTegral:STARt:SOURce``"""
        return _from_core(IntegralStartSource, self._instrument.integral_start_source())

    @integral_start_source.setter
    def integral_start_source(self, source: IntegralStartSource) -> None:
        self._instrument.set_integral_start_source(_to_core(source, "IntegralStartSource"))

    def start_integral(self) -> None:
        """``CALCulate:INTegral:STARt``"""
        self._instrument.start_integral()

    def set_integral_start_time(self, date: Date, time: Time) -> None:
        """``CALCulate:INTegral:STARt:TIME``"""
        self._instrument.set_integral_start_time(
            _core.Date(date.year, date.month, date.day),
            _core.Time(time.hours, time.minutes, time.seconds),
        )

    @property
    def integral_stop_source(self) -> IntegralStopSource:
        """``CALCulate:INTegral:STOP:SOURce``"""
        return _from_core(IntegralStopSource, self._instrument.integral_stop_source())

    @integral_stop_source.setter
    def integral_stop_source(self, source: IntegralStopSource) -> None:
        self._instrument.set_integral_stop_source(_to_core(source, "IntegralStopSource"))

    def stop_integral(self) -> None:
        """``CALCulate:INTegral:STOP``"""
        self._instrument.stop_integral()

    def set_integral_stop_time(self, date: Date, time: Time) -> None:
        """``CALCulate:INTegral:STOP:TIME``"""
        self._instrument.set_integral_stop_time(
            _core.Date(date.year, date.month, date.day),
            _core.Time(time.hours, time.minutes, time.seconds),
        )

    @property
    def integral_stop_interval(self) -> float:
        """``CALCulate:INTegral:STOP:TINTerval`` — integration length in seconds."""
        return self._instrument.integral_stop_interval()

    @integral_stop_interval.setter
    def integral_stop_interval(self, seconds: float) -> None:
        self._instrument.set_integral_stop_interval(seconds)

    # -- CALCulate: power -----------------------------------------------------------------------------
    @property
    def power_correction(self) -> PowerCorrection:
        """``CALCulate:POWer:CORRected`` (firmware >= 1.4)."""
        return _from_core(PowerCorrection, self._instrument.power_correction())

    @power_correction.setter
    def power_correction(self, correction: PowerCorrection) -> None:
        self._instrument.set_power_correction(_to_core(correction, "PowerCorrection"))

    def set_efficiency_reference(
        self, input: str, output: str, phase: int = 0
    ) -> None:
        """``CALCulate:POWer[460]:EFFiciency:REFerence``.

        The two power functions whose ratio ``fn.efficiency()`` reports.
        """
        self._instrument.set_efficiency_reference(input, output, phase)

    # -- Memory recording -------------------------------------------------------------------------------
    def set_sweep_enabled(self, block: SweepBlock, on: bool = True) -> None:
        """``SENSe:SWEep<n>:STATe``"""
        self._instrument.set_sweep_enabled(_to_core(block, "SweepBlock"), on)

    def sweep_enabled(self, block: SweepBlock) -> bool:
        """``SENSe:SWEep<n>:STATe?``"""
        return self._instrument.sweep_enabled(_to_core(block, "SweepBlock"))

    def set_sweep_time(self, block: SweepBlock, seconds: float) -> None:
        """``SENSe:SWEep<n>:TIME`` — how long to record."""
        self._instrument.set_sweep_time(_to_core(block, "SweepBlock"), seconds)

    def set_sweep_time_max(self, block: SweepBlock) -> None:
        """``SENSe:SWEep<n>:TIME MAX`` — record for as long as memory allows."""
        self._instrument.set_sweep_time_max(_to_core(block, "SweepBlock"))

    def sweep_time(self, block: SweepBlock) -> float:
        """``SENSe:SWEep<n>:TIME?``"""
        return self._instrument.sweep_time(_to_core(block, "SweepBlock"))

    def set_sweep_offset_time(self, block: SweepBlock, seconds: float) -> None:
        """``SENSe:SWEep<n>:OFFSet:TIME`` — pretrigger length when negative."""
        self._instrument.set_sweep_offset_time(_to_core(block, "SweepBlock"), seconds)

    def sweep_offset_time(self, block: SweepBlock) -> float:
        """``SENSe:SWEep<n>:OFFSet:TIME?``"""
        return self._instrument.sweep_offset_time(_to_core(block, "SweepBlock"))

    def sweep_points(self, block: SweepBlock) -> int:
        """``SENSe:SWEep<n>:POINTS?``"""
        return self._instrument.sweep_points(_to_core(block, "SweepBlock"))

    def sweep_offset_points(self, block: SweepBlock) -> int:
        """``SENSe:SWEep<n>:OFFSet:POINTS?``"""
        return self._instrument.sweep_offset_points(_to_core(block, "SweepBlock"))

    def set_sweep_count(self, block: SweepBlock, count: int) -> None:
        """``SENSe:SWEep<n>:COUNt`` — how many recordings to make."""
        self._instrument.set_sweep_count(_to_core(block, "SweepBlock"), count)

    def sweep_count(self, block: SweepBlock) -> int:
        """``SENSe:SWEep<n>:COUNt?``"""
        return self._instrument.sweep_count(_to_core(block, "SweepBlock"))

    def set_sweep_sparsing(self, block: SweepBlock, factor: int) -> None:
        """``SENSe:SWEep<n>:SFACtor`` — keep every n-th point (1..65535)."""
        self._instrument.set_sweep_sparsing(_to_core(block, "SweepBlock"), factor)

    def sweep_sparsing(self, block: SweepBlock) -> int:
        """``SENSe:SWEep<n>:SFACtor?``"""
        return self._instrument.sweep_sparsing(_to_core(block, "SweepBlock"))

    def set_sweep_functions(self, block: SweepBlock, functions: Iterable[str]) -> None:
        """``SENSe:SWEep<n>:FUNCtion`` — what to record."""
        self._instrument.set_sweep_functions(_to_core(block, "SweepBlock"), list(functions))

    def sweep_functions(self, block: SweepBlock) -> List[str]:
        """``SENSe:SWEep<n>:FUNCtion?``"""
        return list(self._instrument.sweep_functions(_to_core(block, "SweepBlock")))

    def trace_preamble(self, block: SweepBlock) -> DataPreamble:
        """``TRACe:DATA:PREamble?`` — the shape of the recorded block."""
        return DataPreamble.from_core(
            self._instrument.trace_preamble(_to_core(block, "SweepBlock"))
        )

    def trace_data(
        self, block: SweepBlock, count: int = 0, offset: int = 0, sparsing: int = 0
    ) -> List[float]:
        """``TRACe:DATA?`` — recorded values. ``count`` 0 asks for everything."""
        return list(
            self._instrument.trace_data(
                _to_core(block, "SweepBlock"), count, offset, sparsing
            )
        )

    def trace_status(
        self, block: SweepBlock, count: int = 0, offset: int = 0, sparsing: int = 0
    ) -> List[int]:
        """``TRACe:DATA:STATus?`` — the status flags of :meth:`trace_data`."""
        return list(
            self._instrument.trace_status(
                _to_core(block, "SweepBlock"), count, offset, sparsing
            )
        )

    @property
    def trace_free(self) -> int:
        """``TRACe:FREE?`` — points of recording memory still available."""
        return self._instrument.trace_free()

    @property
    def trace_length(self) -> int:
        """``TRACe:CATalog:LENgth?`` — points currently recorded."""
        return self._instrument.trace_length()

    def delete_traces(self) -> None:
        """``TRACe:DELete:ALL``"""
        self._instrument.delete_traces()

    # -- FORMat ------------------------------------------------------------------------------------------
    @property
    def data_format(self) -> DataFormatSetting:
        """``FORMat[:DATA]`` — read it, or assign a :class:`DataFormat`.

        Only :attr:`DataFormat.ASCII` can be parsed; :meth:`prepare` restores it.
        """
        return DataFormatSetting.from_core(self._instrument.data_format())

    @data_format.setter
    def data_format(self, format: DataFormat) -> None:
        self._instrument.set_data_format(_to_core(format, "DataFormat"), 0)

    def set_data_format(self, format: DataFormat, length: int = 0) -> None:
        """``FORMat[:DATA]`` with an explicit length (ASCii: mantissa digits)."""
        self._instrument.set_data_format(_to_core(format, "DataFormat"), length)

    @property
    def status_format(self) -> DataFormatSetting:
        """``FORMat[:DATA]:STATus``"""
        return DataFormatSetting.from_core(self._instrument.status_format())

    @status_format.setter
    def status_format(self, format: DataFormat) -> None:
        self._instrument.set_status_format(_to_core(format, "DataFormat"), 0)

    def set_status_format(self, format: DataFormat, length: int = 0) -> None:
        """``FORMat[:DATA]:STATus`` with an explicit length."""
        self._instrument.set_status_format(_to_core(format, "DataFormat"), length)

    @property
    def byte_order(self) -> ByteOrder:
        """``FORMat:BORDer`` — byte order of the binary transfer formats."""
        return _from_core(ByteOrder, self._instrument.byte_order())

    @byte_order.setter
    def byte_order(self, order: ByteOrder) -> None:
        self._instrument.set_byte_order(_to_core(order, "ByteOrder"))

    @property
    def transpose(self) -> bool:
        """``FORMat:TRANspose``"""
        return self._instrument.transpose()

    @transpose.setter
    def transpose(self, on: bool) -> None:
        self._instrument.set_transpose(on)

    # -- DISPlay and OUTPut ---------------------------------------------------------------------------------
    @property
    def display_enabled(self) -> bool:
        """``DISPlay:WINDow:STATe`` — the instrument's own screen."""
        return self._instrument.display_enabled()

    @display_enabled.setter
    def display_enabled(self, on: bool) -> None:
        self._instrument.set_display_enabled(on)

    @property
    def display_functions(self) -> List[str]:
        """``DISPlay:USER:FUNCtion`` — what the instrument shows on screen."""
        return list(self._instrument.display_functions())

    @display_functions.setter
    def display_functions(self, functions: Iterable[str]) -> None:
        self._instrument.set_display_functions(list(functions))

    @property
    def output_enabled(self) -> bool:
        """``OUTPut9:STATe`` — the process interface's analog outputs."""
        return self._instrument.output_enabled()

    @output_enabled.setter
    def output_enabled(self, on: bool) -> None:
        self._instrument.set_output_enabled(on)

    # -- SYSTem ----------------------------------------------------------------------------------------------
    def scpi_version(self) -> str:
        """``SYSTem:VERSion?``"""
        return self._instrument.scpi_version()

    @property
    def key_lock(self) -> KeyLock:
        """``SYSTem:KLOCk`` — whether the front panel can return to manual mode."""
        return _from_core(KeyLock, self._instrument.key_lock())

    @key_lock.setter
    def key_lock(self, lock: KeyLock) -> None:
        self._instrument.set_key_lock(_to_core(lock, "KeyLock"))

    @property
    def date(self) -> Date:
        """``SYSTem:DATE``"""
        return Date.from_core(self._instrument.date())

    @date.setter
    def date(self, date: Date) -> None:
        self._instrument.set_date(_core.Date(date.year, date.month, date.day))

    @property
    def time(self) -> Time:
        """``SYSTem:TIME``"""
        return Time.from_core(self._instrument.time_of_day())

    @time.setter
    def time(self, time: Time) -> None:
        self._instrument.set_time(_core.Time(time.hours, time.minutes, time.seconds))

    @property
    def gpib_address(self) -> int:
        """``SYSTem:COMMunicate:GPIB:ADDRess`` (1..30)."""
        return self._instrument.gpib_address()

    @gpib_address.setter
    def gpib_address(self, address: int) -> None:
        self._instrument.set_gpib_address(address)

    @property
    def serial_baud(self) -> int:
        """``SYSTem:COMMunicate:SERial:BAUD`` (1200..115200)."""
        return self._instrument.serial_baud()

    @serial_baud.setter
    def serial_baud(self, baud: int) -> None:
        self._instrument.set_serial_baud(baud)

    @property
    def language(self) -> str:
        """``SYSTem:LANGuage`` — the command-set dialect."""
        return self._instrument.language()

    @language.setter
    def language(self, language: str) -> None:
        self._instrument.set_language(language)

    # -- TIMer ------------------------------------------------------------------------------------------------
    def reset_timer(self) -> None:
        """``TIMer:RESet`` — restart the clock ``fn.time_relative()`` reports."""
        self._instrument.reset_timer()

    @property
    def timer_reset_time(self) -> float:
        """``TIMer:RESet:TIME?`` — seconds since the timer was reset."""
        return self._instrument.timer_reset_time()

    # -- STATus ------------------------------------------------------------------------------------------------
    def status(self, register: StatusRegister, part: RegisterPart) -> int:
        """Read one part of one ``STATus`` register.

        The bit meanings are in :class:`OperationStatus`,
        :class:`QuestionableStatus` and :class:`ChannelStatus`::

            if norma.status(StatusRegister.OPERATION, RegisterPart.CONDITION) \\
                    & OperationStatus.SYNCHRONIZED:
                ...
        """
        return self._instrument.status(
            _to_core(register, "StatusRegister"), _to_core(part, "RegisterPart")
        )

    def set_status(self, register: StatusRegister, part: RegisterPart, mask: int) -> None:
        """Write one of the settable parts (``ENABLE``, the transition masks).

        ``CONDITION`` and ``EVENT`` are read-only and raise :exc:`ValueError`.
        """
        self._instrument.set_status(
            _to_core(register, "StatusRegister"), _to_core(part, "RegisterPart"), mask
        )

    def status_operation_condition(self) -> int:
        """``STATus:OPERation:CONDition?`` — the register a measurement loop polls."""
        return self._instrument.status_operation_condition()

    # -- Errors ------------------------------------------------------------------------------------------------
    def read_errors(self) -> List[ScpiErrorInfo]:
        """Drain ``SYSTem:ERRor?`` and return every queued error."""
        return [ScpiErrorInfo.from_core(e) for e in self._instrument.read_errors()]

    def read_errors_at_once(self) -> List[ScpiErrorInfo]:
        """``SYSTem:ERRor:ALL?`` — drain the queue in a single round-trip."""
        return [ScpiErrorInfo.from_core(e) for e in self._instrument.read_errors_at_once()]

    def check_errors(self) -> None:
        """Raise ``ScpiError`` if the instrument error queue is non-empty."""
        self._instrument.check_errors()

    # -- Plumbing ----------------------------------------------------------------------------------------------
    @property
    def timeout(self) -> float:
        """Default I/O timeout in seconds — assign to change, read to query."""
        return self._instrument.timeout

    @timeout.setter
    def timeout(self, seconds: float) -> None:
        self._instrument.set_timeout(seconds)

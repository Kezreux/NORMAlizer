"""The pythonic, fully typed facade over the compiled ``flukenorma._core.Norma``."""

from __future__ import annotations

from types import TracebackType
from typing import Iterable, Optional, Type

from ._bootstrap import core as _core
from ._protocols import NormaLike
from ._types import Identification, Reading, ScpiErrorInfo, WiringSystem

#: The instrument's TCP port is fixed to 23 (the telnet port).
DEFAULT_PORT: int = int(_core.DEFAULT_PORT)


class Norma:
    """A Fluke NORMA 4000/5000 power analyzer, controlled over TCP/SCPI.

    Compared to the raw binding, this class returns plain dataclasses
    (:class:`Identification`, :class:`Reading`, :class:`ScpiErrorInfo`),
    uses Python enums and flags, and exposes instrument state as properties
    (``timeout``, ``aperture``, ``wiring_system``, ``functions``).
    All timeouts and the aperture are in seconds.

    Typical workflow (matches the manual)::

        with Norma.connect("192.168.1.100") as norma:
            norma.reset()
            norma.wiring_system = WiringSystem.THREE_WATTMETER
            norma.sync_to_voltage(1)
            norma.set_voltage_range(1, 300.0)
            norma.set_current_autorange(1)
            norma.aperture = 1.0
            norma.functions = [fn.voltage(1), fn.current(1), fn.active_power(1)]
            norma.set_continuous(True)
            ...
            reading = norma.read()
            print(reading["VOLT1"].value)

    Any object satisfying :class:`NormaLike` can be wrapped, so tests can
    inject a fake instead of a live instrument: ``Norma(FakeInstrument())``.
    """

    def __init__(self, instrument: NormaLike) -> None:
        self._instrument = instrument

    @classmethod
    def connect(
        cls, host: str, port: int = DEFAULT_PORT, *, timeout: float = 5.0
    ) -> Norma:
        """Connect over TCP/Ethernet (the port is fixed to 23 on the instrument)."""
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
        """Send a raw SCPI setting command, e.g. ``"*RST"``."""
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
        """``*OPT?``"""
        return self._instrument.options()

    def scpi_version(self) -> str:
        """``SYSTem:VERSion?``"""
        return self._instrument.scpi_version()

    def wait_operation_complete(self, timeout: float = 30.0) -> None:
        """``*OPC?`` — blocks until all previous commands have completed."""
        self._instrument.wait_operation_complete(timeout)

    def trigger(self) -> None:
        """``*TRG``"""
        self._instrument.trigger()

    # -- Configuration ------------------------------------------------------------
    @property
    def wiring_system(self) -> WiringSystem:
        """``ROUTe:SYSTem`` — assign to change, read to query."""
        return WiringSystem[self._instrument.wiring_system().name]

    @wiring_system.setter
    def wiring_system(self, system: WiringSystem) -> None:
        self._instrument.set_wiring_system(getattr(_core.WiringSystem, system.name))

    def set_sync_source(self, source: str) -> None:
        """``SYNC:SOURce <source>``"""
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

    def set_voltage_range(self, phase: int, volts: float) -> None:
        """``VOLT<n>:RANGe`` (disables autorange)."""
        self._instrument.set_voltage_range(phase, volts)

    def set_voltage_autorange(self, phase: int, on: bool = True) -> None:
        """``VOLT<n>:RANGe:AUTO``"""
        self._instrument.set_voltage_autorange(phase, on)

    def set_current_range(self, phase: int, amps: float) -> None:
        """``CURR<n>:RANGe`` (disables autorange)."""
        self._instrument.set_current_range(phase, amps)

    def set_current_autorange(self, phase: int, on: bool = True) -> None:
        """``CURR<n>:RANGe:AUTO``"""
        self._instrument.set_current_autorange(phase, on)

    @property
    def aperture(self) -> float:
        """``APERture`` — averaging time in seconds (0.015..3600)."""
        return self._instrument.aperture()

    @aperture.setter
    def aperture(self, seconds: float) -> None:
        self._instrument.set_aperture(seconds)

    @property
    def functions(self) -> list[str]:
        """``SENSe:FUNCtion`` — the configured <function> names (see :mod:`.fn`)."""
        return self._instrument.functions()

    @functions.setter
    def functions(self, functions: Iterable[str]) -> None:
        self._instrument.set_functions(list(functions))

    @property
    def function_count(self) -> int:
        """``SENSe:FUNCtion:COUNt?``"""
        return self._instrument.function_count()

    def clear_functions(self) -> None:
        """``SENSe:FUNCtion:OFF:ALL``"""
        self._instrument.clear_functions()

    # -- Acquisition ---------------------------------------------------------------
    def set_continuous(self, on: bool = True) -> None:
        """``INITiate:CONTinuous``"""
        self._instrument.set_continuous(on)

    def initiate(self) -> None:
        """``INITiate`` (single shot)."""
        self._instrument.initiate()

    def abort(self) -> None:
        """``ABORt``"""
        self._instrument.abort()

    def data(self, *functions: str) -> list[float]:
        """``DATA?`` — averaged values of the configured (or given) functions.

        Undefined/unavailable values come back as NaN.
        """
        return self._instrument.data(list(functions))

    def read(self, *functions: str) -> Reading:
        """``DATA:STATus?`` — values plus status, labelled with their function names.

        When called without arguments the configured function list is queried
        first (one extra SCPI round-trip) so each value can be labelled.
        """
        names = list(functions) if functions else self._instrument.functions()
        core_reading = self._instrument.data_with_status(list(functions))
        return Reading.from_core(core_reading, names)

    # -- Errors & status --------------------------------------------------------------
    def read_errors(self) -> list[ScpiErrorInfo]:
        """Drain ``SYSTem:ERRor?`` and return every queued error."""
        return [ScpiErrorInfo.from_core(e) for e in self._instrument.read_errors()]

    def check_errors(self) -> None:
        """Raise ``ScpiError`` if the instrument error queue is non-empty."""
        self._instrument.check_errors()

    def status_operation_condition(self) -> int:
        """``STATus:OPERation:CONDition?``"""
        return self._instrument.status_operation_condition()

    # -- Plumbing --------------------------------------------------------------------
    @property
    def timeout(self) -> float:
        """Default I/O timeout in seconds — assign to change, read to query."""
        return self._instrument.timeout

    @timeout.setter
    def timeout(self, seconds: float) -> None:
        self._instrument.set_timeout(seconds)

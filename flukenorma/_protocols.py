"""Structural types (PEP 544 Protocols) for the compiled ``flukenorma._core`` objects.

The pybind11 extension ships no type stubs, so every attribute on it is ``Any``
to a static type checker.  The Protocols below describe the exact shapes of the
objects the extension returns and accepts (see ``module/bindings/python/src/
module.cpp``), which gives the wrapper — and code written against it — full
static typing without needing the compiled module at type-check time.

``NormaLike`` doubles as a seam for testing: any object that satisfies it
(e.g. a hand-written fake) can be handed to :class:`flukenorma.Norma` in place
of a live instrument.
"""

from __future__ import annotations

from typing import Protocol, Sequence, runtime_checkable


@runtime_checkable
class CoreEnumLike(Protocol):
    """A pybind11 enum value (``flukenorma._core.WiringSystem`` etc.).

    Python :class:`enum.Enum` members satisfy this too, which lets fakes
    return the wrapper's own enums directly.
    """

    @property
    def name(self) -> str: ...

    @property
    def value(self) -> int: ...


@runtime_checkable
class IdentificationLike(Protocol):
    """Parsed ``*IDN?`` response as returned by ``flukenorma._core.Identification``."""

    @property
    def manufacturer(self) -> str: ...

    @property
    def model(self) -> str: ...

    @property
    def serial_number(self) -> str: ...

    @property
    def firmware_version(self) -> str: ...


@runtime_checkable
class ScpiErrorInfoLike(Protocol):
    """One entry from the instrument error queue (``SYSTem:ERRor?``)."""

    @property
    def code(self) -> int: ...

    @property
    def message(self) -> str: ...


@runtime_checkable
class ReadingLike(Protocol):
    """Result of ``DATA:STATus?``: one status flag per measurement value."""

    @property
    def values(self) -> list[float]: ...

    @property
    def status(self) -> list[int]: ...


@runtime_checkable
class NormaLike(Protocol):
    """The interface of ``flukenorma._core.Norma`` (the compiled instrument class).

    Timeouts and the aperture are in seconds; ``phase`` follows the manual's
    phase suffixes (1..6 = L1..L6, 12/23/31/... = phase-to-phase, 0 = totals).
    """

    # -- Lifecycle -----------------------------------------------------------
    def close(self) -> None: ...

    @property
    def is_open(self) -> bool: ...

    # -- Raw SCPI escape hatches ----------------------------------------------
    def write(self, scpi: str) -> None: ...

    def query(self, scpi: str) -> str: ...

    # -- IEEE 488.2 common commands ---------------------------------------------
    def identify(self) -> IdentificationLike: ...

    def reset(self) -> None: ...

    def clear_status(self) -> None: ...

    def options(self) -> str: ...

    def scpi_version(self) -> str: ...

    def wait_operation_complete(self, timeout: float = ...) -> None: ...

    def trigger(self) -> None: ...

    # -- Configuration ------------------------------------------------------------
    def set_wiring_system(self, system: CoreEnumLike) -> None: ...

    def wiring_system(self) -> CoreEnumLike: ...

    def set_sync_source(self, source: str) -> None: ...

    def sync_to_voltage(self, phase: int) -> None: ...

    def sync_to_current(self, phase: int) -> None: ...

    def sync_external(self) -> None: ...

    def set_voltage_range(self, phase: int, volts: float) -> None: ...

    def set_voltage_autorange(self, phase: int, on: bool = ...) -> None: ...

    def set_current_range(self, phase: int, amps: float) -> None: ...

    def set_current_autorange(self, phase: int, on: bool = ...) -> None: ...

    def set_aperture(self, seconds: float) -> None: ...

    def aperture(self) -> float: ...

    def set_functions(self, functions: Sequence[str]) -> None: ...

    def functions(self) -> list[str]: ...

    def function_count(self) -> int: ...

    def clear_functions(self) -> None: ...

    # -- Acquisition ---------------------------------------------------------------
    def set_continuous(self, on: bool = ...) -> None: ...

    def initiate(self) -> None: ...

    def abort(self) -> None: ...

    def data(self, functions: Sequence[str] = ...) -> list[float]: ...

    def data_with_status(self, functions: Sequence[str] = ...) -> ReadingLike: ...

    # -- Errors & status --------------------------------------------------------------
    def read_errors(self) -> list[ScpiErrorInfoLike]: ...

    def check_errors(self) -> None: ...

    def status_operation_condition(self) -> int: ...

    # -- Plumbing --------------------------------------------------------------------
    def set_timeout(self, timeout: float) -> None: ...

    @property
    def timeout(self) -> float: ...

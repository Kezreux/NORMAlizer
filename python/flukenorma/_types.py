"""Pythonic value types mirroring the C++ structs and enums in ``fluke/norma/types.hpp``.

The dataclasses here are plain, immutable Python objects built from the
pybind11 counterparts via their ``from_core`` constructors, so they can be
created, compared and unpacked without the compiled module.
"""

from __future__ import annotations

import enum
import math
from dataclasses import dataclass
from typing import Iterator, Sequence, Union

from ._protocols import IdentificationLike, ReadingLike, ScpiErrorInfoLike

# -- Enums ---------------------------------------------------------------------
# Member names match the pybind11 enum members in flukenorma._core, so values
# can be bridged both ways by name.


class WiringSystem(enum.Enum):
    """``ROUTe:SYSTem`` — how the measurement channels are wired."""

    THREE_WATTMETER = "3W"
    TWO_WATTMETER = "2W"  # Aron circuit; requires firmware >= 1.4


class Coupling(enum.Enum):
    """``INPut:COUPling``."""

    AC = "AC"
    DC = "DC"


class Shunt(enum.Enum):
    """``INPut:SHUNt``."""

    INTERNAL = "INTERNAL"
    EXTERNAL = "EXTERNAL"


class MeasurementStatus(enum.IntFlag):
    """Bit flags of the per-value status returned by ``DATA:STATus?``."""

    NORMAL = 0
    UNDERRANGE = 1
    OVERRANGE = 2
    UNDEFINED = 8
    NOT_AVAILABLE = 16
    POWER_FACTOR_CAPACITIVE = 128


#: Status bits that mean the accompanying value cannot be trusted.
_INVALID_STATUS = (
    MeasurementStatus.UNDERRANGE
    | MeasurementStatus.OVERRANGE
    | MeasurementStatus.UNDEFINED
    | MeasurementStatus.NOT_AVAILABLE
)


# -- Value types ---------------------------------------------------------------


@dataclass(frozen=True)
class Identification:
    """Parsed ``*IDN?`` response, e.g. ``Fluke,NORMA4000,KN34512BA,01.00``."""

    manufacturer: str
    model: str
    serial_number: str
    firmware_version: str

    @classmethod
    def from_core(cls, ident: IdentificationLike) -> Identification:
        return cls(
            manufacturer=ident.manufacturer,
            model=ident.model,
            serial_number=ident.serial_number,
            firmware_version=ident.firmware_version,
        )


@dataclass(frozen=True)
class ScpiErrorInfo:
    """One entry from the instrument error queue (``SYSTem:ERRor?``)."""

    code: int
    message: str

    @classmethod
    def from_core(cls, error: ScpiErrorInfoLike) -> ScpiErrorInfo:
        return cls(code=error.code, message=error.message)


@dataclass(frozen=True)
class Measurement:
    """A single value from ``DATA:STATus?``, labelled with its <function> name."""

    function: str
    value: float
    status: MeasurementStatus

    @property
    def is_valid(self) -> bool:
        """False if the instrument flagged the value under/overrange, undefined
        or not available (such values also come back as NaN)."""
        return not (self.status & _INVALID_STATUS) and not math.isnan(self.value)


@dataclass(frozen=True)
class Reading:
    """Result of ``DATA:STATus?``: one :class:`Measurement` per configured function.

    Supports ``len()``, iteration, indexing by position and lookup by
    <function> name::

        reading = norma.read()
        for m in reading:
            print(m.function, m.value, m.status)
        volts = reading["VOLT1"].value
    """

    measurements: tuple[Measurement, ...]

    @classmethod
    def from_core(cls, reading: ReadingLike, functions: Sequence[str]) -> Reading:
        values = list(reading.values)
        status = list(reading.status)
        if len(values) != len(status):
            raise ValueError(
                f"instrument returned {len(values)} values but {len(status)} status flags"
            )
        if len(values) != len(functions):
            raise ValueError(
                f"instrument returned {len(values)} values for {len(functions)} functions"
            )
        return cls(
            measurements=tuple(
                Measurement(function=f, value=v, status=MeasurementStatus(s))
                for f, v, s in zip(functions, values, status)
            )
        )

    @property
    def functions(self) -> tuple[str, ...]:
        return tuple(m.function for m in self.measurements)

    @property
    def values(self) -> tuple[float, ...]:
        return tuple(m.value for m in self.measurements)

    @property
    def status(self) -> tuple[MeasurementStatus, ...]:
        return tuple(m.status for m in self.measurements)

    def __len__(self) -> int:
        return len(self.measurements)

    def __iter__(self) -> Iterator[Measurement]:
        return iter(self.measurements)

    def __getitem__(self, key: Union[int, str]) -> Measurement:
        if isinstance(key, str):
            for m in self.measurements:
                if m.function.upper() == key.upper():
                    return m
            raise KeyError(key)
        return self.measurements[key]

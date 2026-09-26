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

from ._protocols import (
    DataFormatSettingLike,
    DataPreambleLike,
    DateLike,
    IdentificationLike,
    ReadingLike,
    ScpiErrorInfoLike,
    TimeLike,
)

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


class Slope(enum.Enum):
    """``SLOPe`` — which edge of the synchronization or trigger signal counts."""

    POSITIVE = "POS"
    NEGATIVE = "NEG"


class LevelUnit(enum.Enum):
    """``SYNC:LEVel:UNIT`` — how the synchronization level is expressed."""

    ABSOLUTE = "ABS"
    PERCENT = "PCT"


class DataFormat(enum.Enum):
    """``FORMat[:DATA]`` — the transfer format of measurement data.

    Only :attr:`ASCII` can be parsed by this library: the binary formats arrive
    as definite-length blocks. :meth:`flukenorma.Norma.prepare` switches an
    instrument back to ASCII.
    """

    ASCII = "ASC"
    INTEGER = "INT"
    REAL = "REAL"


class ByteOrder(enum.Enum):
    """``FORMat:BORDer`` — byte order of the binary transfer formats."""

    NORMAL = "NORM"
    SWAPPED = "SWAP"


class KeyLock(enum.Enum):
    """``SYSTem:KLOCk`` — front-panel key lock."""

    OFF = "OFF"
    ON = "ON"
    REMOTE = "REM"


class TransformMode(enum.Enum):
    """``CALCulate:TRANsform:FREQuency:MODE``."""

    FFT = "FFT"
    DFT = "DFT"
    STD = "STD"  # firmware >= 1.5


class HarmonicGrouping(enum.Enum):
    """``CALCulate:TRANsform:FREQuency:GROuping`` (STD mode, firmware >= 1.5)."""

    COMPONENT = "COMP"
    HARMONIC = "HARM"
    H_GROUP = "HGR"
    HS_GROUP = "HSGR"
    IS_GROUP = "ISGR"
    S_GROUP = "SGR"


class PowerCorrection(enum.Enum):
    """``CALCulate:POWer:CORRected`` (firmware >= 1.4)."""

    STAR = "STAR"
    DELTA = "DELT"


class IntegralStartSource(enum.Enum):
    """``CALCulate:INTegral:STARt:SOURce``."""

    COMMAND = "CMD"
    TIME = "TIME"
    MANUAL = "MAN"


class IntegralStopSource(enum.Enum):
    """``CALCulate:INTegral:STOP:SOURce``."""

    COMMAND = "CMD"
    TIME = "TIME"
    MANUAL = "MAN"
    TIME_INTERVAL = "TINT"


class SweepBlock(enum.Enum):
    """The two memory-recording blocks (``SENSe:SWEep1|2`` and ``TRACe``)."""

    BLOCK1 = 1
    BLOCK2 = 2


class StatusRegister(enum.Enum):
    """Which ``STATus`` register a query addresses."""

    OPERATION = "OPER"
    QUESTIONABLE = "QUES"
    QUESTIONABLE_VOLTAGE = "QUES:VOLT"
    QUESTIONABLE_CURRENT = "QUES:CURR"


class RegisterPart(enum.Enum):
    """Which part of a SCPI status register a query addresses.

    ``CONDITION`` is the current state and ``EVENT`` what has been latched since
    the last read; both are read-only. ``ENABLE`` masks what feeds the summary
    bit, and the two transition masks decide which edges latch into ``EVENT``.
    """

    CONDITION = "COND"
    EVENT = "EVEN"
    ENABLE = "ENAB"
    POSITIVE_TRANSITION = "PTR"
    NEGATIVE_TRANSITION = "NTR"


class StatusByte(enum.IntFlag):
    """Bits of the Status Byte (``*STB?``) and the Service Request Enable mask."""

    ERROR_QUEUE_NOT_EMPTY = 1 << 2
    QUESTIONABLE_SUMMARY = 1 << 3
    MESSAGE_AVAILABLE = 1 << 4
    EVENT_STATUS_SUMMARY = 1 << 5
    MASTER_STATUS_SUMMARY = 1 << 6


class OperationStatus(enum.IntFlag):
    """Bits of ``STATus:OPERation`` (manual table 2-3)."""

    RANGING = 1 << 2
    SWEEPING = 1 << 3
    WAITING_FOR_TRIGGER = 1 << 5
    SYNCHRONIZED = 1 << 8
    SYNC_AVAILABLE = 1 << 9
    AVERAGING = 1 << 10
    CALCULATING = 1 << 12


class QuestionableStatus(enum.IntFlag):
    """Bits of ``STATus:QUEStionable`` (manual table 2-4)."""

    VOLTAGE_SUMMARY = 1 << 0
    CURRENT_SUMMARY = 1 << 1
    FREQUENCY = 1 << 5


class ChannelStatus(enum.IntFlag):
    """Bits of ``STATus:QUEStionable:VOLTage`` and ``:CURRent``.

    Both registers hold six overrange bits (0..5) followed by six underrange bits
    (8..13). ``index`` selects the channel within the register: for the voltage
    register that is INPut 2, 4, ..., 12 and for the current register INPut 1, 3,
    ..., 11.
    """

    OVERRANGE_MASK = 0x003F
    UNDERRANGE_MASK = 0x3F00

    @staticmethod
    def overrange(index: int) -> int:
        """Overrange bit of the channel at `index` (0..5)."""
        return 1 << index

    @staticmethod
    def underrange(index: int) -> int:
        """Underrange bit of the channel at `index` (0..5)."""
        return 1 << (index + 8)


class MeasurementStatus(enum.IntFlag):
    """Bit flags of the per-value status returned by ``DATA:STATus?``.

    The manual documents the status as a bitmask, so a firmware revision may set
    a bit this table does not name. Such a value is kept rather than rejected:
    ``enum.IntFlag`` only tolerates unnamed bits from Python 3.11 on (via
    ``boundary=KEEP``), and this package supports 3.9, where the default is to
    raise ``ValueError``. Losing the whole reading over one unknown bit would be
    the wrong trade for a status field.
    """

    NORMAL = 0
    UNDERRANGE = 1
    OVERRANGE = 2
    UNDEFINED = 8
    NOT_AVAILABLE = 16
    POWER_FACTOR_CAPACITIVE = 128

    @classmethod
    def _missing_(cls, value: object) -> MeasurementStatus:
        if not isinstance(value, int) or value < 0:
            return None  # type: ignore[return-value]  # let Enum raise ValueError
        # Keep the numeric value intact, unknown bits included, so it still
        # round-trips and the named bits can be tested with `&`.
        pseudo = int.__new__(cls, value)
        pseudo._name_ = None
        pseudo._value_ = value
        cls._value2member_map_.setdefault(value, pseudo)
        return pseudo


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


@dataclass(frozen=True)
class DataFormatSetting:
    """``FORMat[:DATA]?`` — the transfer format and its length in bits.

    For :attr:`DataFormat.ASCII` the length is the number of mantissa digits
    (0 means the instrument chooses).
    """

    format: DataFormat
    length: int

    @classmethod
    def from_core(cls, setting: "DataFormatSettingLike") -> DataFormatSetting:
        return cls(format=DataFormat[setting.format.name], length=setting.length)

    @property
    def is_parsable(self) -> bool:
        """False for the binary formats, whose block responses this library
        cannot read (see :meth:`flukenorma.Norma.prepare`)."""
        return self.format is DataFormat.ASCII


@dataclass(frozen=True)
class Date:
    """A calendar date, as ``SYSTem:DATE`` and the time-based triggers use it."""

    year: int
    month: int
    day: int

    @classmethod
    def from_core(cls, date: "DateLike") -> Date:
        return cls(year=date.year, month=date.month, day=date.day)


@dataclass(frozen=True)
class Time:
    """A time of day, as ``SYSTem:TIME`` and the time-based triggers use it."""

    hours: int
    minutes: int
    seconds: int

    @classmethod
    def from_core(cls, time: "TimeLike") -> Time:
        return cls(hours=time.hours, minutes=time.minutes, seconds=time.seconds)


@dataclass(frozen=True)
class DataPreamble:
    """``CALCulate:DATA:PREamble?`` / ``TRACe:DATA:PREamble?``.

    Describes the shape of the block the matching data query returns. The fields
    the instrument reports vary with the block, so :attr:`raw` keeps the whole
    response for anything the parsed fields do not cover.
    """

    count: int
    function_count: int
    interval: float
    start: float
    functions: tuple[str, ...]
    raw: str

    @classmethod
    def from_core(cls, preamble: "DataPreambleLike") -> DataPreamble:
        return cls(
            count=preamble.count,
            function_count=preamble.function_count,
            interval=preamble.interval,
            start=preamble.start,
            functions=tuple(preamble.functions),
            raw=preamble.raw,
        )

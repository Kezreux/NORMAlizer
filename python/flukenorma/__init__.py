"""Pythonic, fully typed wrapper for the Fluke NORMA 4000/5000 power analyzers.

The compiled pybind11 extension (``flukenorma._core``) does the actual
TCP/SCPI work; this package wraps it with:

* PEP 544 :class:`~._protocols.NormaLike` & friends — Protocols that describe
  the compiled objects, so everything type-checks and fakes can stand in for
  the real instrument in tests.
* Plain dataclasses (:class:`Identification`, :class:`Reading`,
  :class:`Measurement`, :class:`ScpiErrorInfo`) instead of opaque C++ types.
* Real Python enums (:class:`WiringSystem`, :class:`Coupling`,
  :class:`Shunt`) and an :class:`enum.IntFlag` (:class:`MeasurementStatus`)
  for the ``DATA:STATus?`` bit flags.
* Instrument state as properties: ``timeout``, ``aperture``,
  ``wiring_system`` and ``functions`` are read/assigned instead of paired
  getter/setter calls.
* :meth:`Norma.read`, which pairs each value with its <function> name.

Example::

    import time

    from flukenorma import Norma, WiringSystem, fn

    with Norma.connect("192.168.1.100") as norma:
        print(norma.identify())
        norma.prepare()          # ASCII transfer format, concurrent, clean queue
        norma.reset()
        norma.wiring_system = WiringSystem.THREE_WATTMETER
        norma.sync_to_voltage(1)
        norma.set_voltage_range(1, 300.0)
        norma.set_current_autorange(1)
        norma.aperture = 1.0
        norma.functions = [fn.voltage(1), fn.current(1), fn.active_power(1)]
        norma.continuous = True
        time.sleep(2)
        for measurement in norma.read():
            print(measurement.function, measurement.value, measurement.status)
"""

from . import fn
from ._bootstrap import core as _core
from ._norma import (
    DEFAULT_PORT,
    MAX_APERTURE,
    MAX_INPUT_CHANNEL,
    MAX_PHASE,
    MIN_APERTURE,
    Norma,
)
from ._protocols import (
    CoreEnumLike,
    DataFormatSettingLike,
    DataPreambleLike,
    DateLike,
    IdentificationLike,
    NormaLike,
    ReadingLike,
    ScpiErrorInfoLike,
    TimeLike,
)
from ._types import (
    ByteOrder,
    ChannelStatus,
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
    Measurement,
    MeasurementStatus,
    OperationStatus,
    PowerCorrection,
    QuestionableStatus,
    Reading,
    RegisterPart,
    ScpiErrorInfo,
    Shunt,
    Slope,
    StatusByte,
    StatusRegister,
    SweepBlock,
    Time,
    TransformMode,
    WiringSystem,
)

# The extension's exception classes, re-exported unchanged so except-clauses
# interoperate with code that uses flukenorma._core directly. ConnectionError
# and TimeoutError shadow the builtins by design, mirroring the binding.
NormaError = _core.NormaError
ConnectionError = _core.ConnectionError  # noqa: A001
TimeoutError = _core.TimeoutError  # noqa: A001
ProtocolError = _core.ProtocolError
ScpiError = _core.ScpiError

__version__: str = _core.__version__

__all__ = [
    # Instrument
    "DEFAULT_PORT",
    "MAX_APERTURE",
    "MAX_INPUT_CHANNEL",
    "MAX_PHASE",
    "MIN_APERTURE",
    "Norma",
    "fn",
    # Exceptions
    "NormaError",
    "ConnectionError",
    "TimeoutError",
    "ProtocolError",
    "ScpiError",
    # Value types
    "DataFormatSetting",
    "DataPreamble",
    "Date",
    "Identification",
    "Measurement",
    "Reading",
    "ScpiErrorInfo",
    "Time",
    # Enums
    "ByteOrder",
    "Coupling",
    "DataFormat",
    "HarmonicGrouping",
    "IntegralStartSource",
    "IntegralStopSource",
    "KeyLock",
    "LevelUnit",
    "PowerCorrection",
    "RegisterPart",
    "Shunt",
    "Slope",
    "StatusRegister",
    "SweepBlock",
    "TransformMode",
    "WiringSystem",
    # Status bit flags
    "ChannelStatus",
    "MeasurementStatus",
    "OperationStatus",
    "QuestionableStatus",
    "StatusByte",
    # Protocols (structural types for the compiled objects / test fakes)
    "CoreEnumLike",
    "DataFormatSettingLike",
    "DataPreambleLike",
    "DateLike",
    "IdentificationLike",
    "NormaLike",
    "ReadingLike",
    "ScpiErrorInfoLike",
    "TimeLike",
    "__version__",
]

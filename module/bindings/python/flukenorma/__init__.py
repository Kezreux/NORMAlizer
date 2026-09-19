"""Python bindings for the Fluke NORMA 4000/5000 TCP/SCPI wrapper.

Example:
    import time

    import flukenorma as norma

    with norma.Norma("192.168.1.100") as instrument:
        print(instrument.identify())
        instrument.reset()
        instrument.write('ROUT:SYST "3W"')
        instrument.sync_to_voltage(1)
        instrument.set_voltage_range(1, 300.0)
        instrument.set_current_autorange(1)
        instrument.set_aperture(1.0)
        instrument.set_functions([
            norma.fn.voltage(1),
            norma.fn.current(1),
            norma.fn.active_power(1),
        ])
        instrument.set_continuous(True)
        time.sleep(2)
        print(instrument.data())
"""

from ._core import (
    DEFAULT_PORT,
    ConnectionError,
    Coupling,
    Identification,
    Norma,
    NormaError,
    ProtocolError,
    Reading,
    ScpiError,
    ScpiErrorInfo,
    Shunt,
    TimeoutError,
    WiringSystem,
    __version__,
    fn,
    measurement_status,
)

__all__ = [
    "DEFAULT_PORT",
    "ConnectionError",
    "Coupling",
    "Identification",
    "Norma",
    "NormaError",
    "ProtocolError",
    "Reading",
    "ScpiError",
    "ScpiErrorInfo",
    "Shunt",
    "TimeoutError",
    "WiringSystem",
    "__version__",
    "fn",
    "measurement_status",
]

"""Typed builders for SENSe <function> names (mirrors ``flukenorma._core.fn``).

``phase`` follows the manual's phase suffixes: 1..6 = L1..L6, 12/23/31/... =
phase-to-phase, 460 = totals of the 2nd three-phase system, and 0 = no suffix
(average/total of the 1st system).
"""

from __future__ import annotations

__all__ = [
    "voltage",
    "voltage_ac",
    "voltage_mean",
    "current",
    "current_ac",
    "current_mean",
    "active_power",
    "apparent_power",
    "reactive_power",
    "power_factor",
    "phase_angle",
    "frequency",
    "time_interval",
]


def _suffixed(base: str, phase: int) -> str:
    return f"{base}{phase}" if phase > 0 else base


def voltage(phase: int = 0) -> str:
    """True-RMS voltage (``VOLT<n>``)."""
    return _suffixed("VOLT", phase)


def voltage_ac(phase: int = 0) -> str:
    """RMS voltage without the DC component (``VOLT<n>:AC``)."""
    return _suffixed("VOLT", phase) + ":AC"


def voltage_mean(phase: int = 0) -> str:
    """Mean (DC) voltage (``VOLT<n>:MEAN``)."""
    return _suffixed("VOLT", phase) + ":MEAN"


def current(phase: int = 0) -> str:
    """True-RMS current (``CURR<n>``)."""
    return _suffixed("CURR", phase)


def current_ac(phase: int = 0) -> str:
    """RMS current without the DC component (``CURR<n>:AC``)."""
    return _suffixed("CURR", phase) + ":AC"


def current_mean(phase: int = 0) -> str:
    """Mean (DC) current (``CURR<n>:MEAN``)."""
    return _suffixed("CURR", phase) + ":MEAN"


def active_power(phase: int = 0) -> str:
    """Active power P (``POW<n>:ACT``)."""
    return _suffixed("POW", phase) + ":ACT"


def apparent_power(phase: int = 0) -> str:
    """Apparent power S (``POW<n>:APP``)."""
    return _suffixed("POW", phase) + ":APP"


def reactive_power(phase: int = 0) -> str:
    """Reactive power Q (``POW<n>:REAC``)."""
    return _suffixed("POW", phase) + ":REAC"


def power_factor(phase: int = 0) -> str:
    """Power factor (``POW<n>:FACT``)."""
    return _suffixed("POW", phase) + ":FACT"


def phase_angle(phase: int = 0) -> str:
    """Phase angle (``PHAS<n>``)."""
    return _suffixed("PHAS", phase)


def frequency() -> str:
    """Frequency of the SYNC source (``FREQ``)."""
    return "FREQ"


def time_interval() -> str:
    """Averaging interval in seconds (``TIME``)."""
    return "TIME"

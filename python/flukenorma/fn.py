"""Typed builders for SENSe ``<function>`` names.

Every builder delegates to the compiled ``flukenorma._core.fn``, so there is one
implementation of the naming rules rather than two that can drift apart — and the
suffix validation comes along with it: a suffix the manual does not define raises
``ValueError`` here instead of coming back from the instrument as SCPI error -113.

``phase`` follows the manual's phase suffixes:

===========  ====================================================================
``0``        no suffix — average/total of the 1st three-phase system
``1``–``6``  L1..L6 (4..6 on the NORMA 5000 only)
``12,13``    phase-to-phase within the 1st system (13 is 2W, 31 is 3W)
``23,31``
``45,56,64`` phase-to-phase within the 2nd system
``123,456``  average phase-to-phase voltage of the 1st/2nd system
``460``      average/total of the 2nd three-phase system
===========  ====================================================================

The trailing modifiers compose onto any name::

    fn.minimum(fn.active_power(1))   # "POW1:ACT:MIN"
    fn.harmonic(fn.voltage(1))       # "VOLT1:HAR"
"""

from __future__ import annotations

from ._bootstrap import core as _core

_fn = _core.fn

__all__ = [
    # Suffix predicates
    "is_phase",
    "is_phase_to_phase",
    "is_aggregate",
    "is_average_phase_to_phase",
    "is_valid_suffix",
    # Voltage
    "voltage",
    "voltage_ac",
    "voltage_mean",
    "voltage_rectified_mean",
    "voltage_rectified_mean_corrected",
    "voltage_peak_to_peak",
    "voltage_peak_high",
    "voltage_peak_low",
    "voltage_crest_factor",
    "voltage_form_factor",
    "voltage_harmonic_content",
    "voltage_fundamental_content",
    "voltage_thd",
    "voltage_phase",
    # Phase-to-phase voltage
    "voltage_line",
    "voltage_line_mean",
    "voltage_line_rectified_mean",
    "voltage_line_rectified_mean_corrected",
    "voltage_line_form_factor",
    "voltage_line_thd",
    "voltage_line_harmonic_content",
    "voltage_line_fundamental_content",
    "voltage_line_phase",
    # Current
    "current",
    "current_ac",
    "current_mean",
    "current_rectified_mean",
    "current_rectified_mean_corrected",
    "current_peak_to_peak",
    "current_peak_high",
    "current_peak_low",
    "current_crest_factor",
    "current_form_factor",
    "current_harmonic_content",
    "current_fundamental_content",
    "current_thd",
    "current_phase",
    # Power and derived quantities
    "active_power",
    "apparent_power",
    "reactive_power",
    "power_factor",
    "corrected_power",
    "efficiency",
    "phase_angle",
    "apparent_impedance",
    "series_resistance",
    "parallel_resistance",
    "series_reactance",
    "parallel_reactance",
    # Instrument-wide
    "frequency",
    "time_interval",
    "time_relative",
    # Modifiers
    "harmonic",
    "minimum",
    "maximum",
    "integral",
    "integral_positive",
    "integral_negative",
]


# -- Suffix predicates ---------------------------------------------------------


def is_phase(phase: int) -> bool:
    """True for a single-phase suffix (L1..L6)."""
    return bool(_fn.is_phase(phase))


def is_phase_to_phase(phase: int) -> bool:
    """True for a phase-to-phase voltage suffix (12, 13, 23, 31, 45, 56, 64)."""
    return bool(_fn.is_phase_to_phase(phase))


def is_aggregate(phase: int) -> bool:
    """True for an aggregate suffix: 0 (1st system) or 460 (2nd system)."""
    return bool(_fn.is_aggregate(phase))


def is_average_phase_to_phase(phase: int) -> bool:
    """True for an average phase-to-phase suffix (123, 456)."""
    return bool(_fn.is_average_phase_to_phase(phase))


def is_valid_suffix(phase: int) -> bool:
    """True for any suffix the manual defines."""
    return bool(_fn.is_valid_suffix(phase))


# -- Voltage -------------------------------------------------------------------


def voltage(phase: int = 0) -> str:
    """True-RMS voltage (``VOLT<n>``)."""
    return _fn.voltage(phase)


def voltage_ac(phase: int = 0) -> str:
    """RMS voltage without the DC component (``VOLT<n>:AC``)."""
    return _fn.voltage_ac(phase)


def voltage_mean(phase: int = 0) -> str:
    """Mean (DC) voltage (``VOLT<n>:MEAN``)."""
    return _fn.voltage_mean(phase)


def voltage_rectified_mean(phase: int = 0) -> str:
    """Rectified mean voltage (``VOLT<n>:RMEAN``)."""
    return _fn.voltage_rectified_mean(phase)


def voltage_rectified_mean_corrected(phase: int = 0) -> str:
    """Corrected rectified mean voltage (``VOLT<n>:RMCORR``)."""
    return _fn.voltage_rectified_mean_corrected(phase)


def voltage_peak_to_peak(phase: int) -> str:
    """Peak-to-peak voltage (``VOLT<n>:PTP``); per channel only."""
    return _fn.voltage_peak_to_peak(phase)


def voltage_peak_high(phase: int) -> str:
    """Highest sampled voltage in the averaging interval (``VOLT<n>:PHIGH``)."""
    return _fn.voltage_peak_high(phase)


def voltage_peak_low(phase: int) -> str:
    """Lowest sampled voltage in the averaging interval (``VOLT<n>:PLOW``)."""
    return _fn.voltage_peak_low(phase)


def voltage_crest_factor(phase: int) -> str:
    """Voltage crest factor (``VOLT<n>:CFAC``)."""
    return _fn.voltage_crest_factor(phase)


def voltage_form_factor(phase: int) -> str:
    """Voltage form factor (``VOLT<n>:FFAC``)."""
    return _fn.voltage_form_factor(phase)


def voltage_harmonic_content(phase: int) -> str:
    """Voltage harmonic content (``VOLT<n>:HCONT``)."""
    return _fn.voltage_harmonic_content(phase)


def voltage_fundamental_content(phase: int) -> str:
    """Voltage fundamental content (``VOLT<n>:FCONT``)."""
    return _fn.voltage_fundamental_content(phase)


def voltage_thd(phase: int) -> str:
    """Voltage total harmonic distortion (``VOLT<n>:THD``)."""
    return _fn.voltage_thd(phase)


def voltage_phase(phase: int) -> str:
    """Absolute voltage phase against the SYNC signal (``VOLT<n>:PHAS``)."""
    return _fn.voltage_phase(phase)


# -- Phase-to-phase voltage ----------------------------------------------------


def voltage_line(phase: int) -> str:
    """Phase-to-phase voltage (``VOLT12``, ``VOLT23``, ..., ``VOLT123``)."""
    return _fn.voltage_line(phase)


def voltage_line_mean(phase: int) -> str:
    """Mean phase-to-phase voltage (``VOLT<nn>:MEAN``)."""
    return _fn.voltage_line_mean(phase)


def voltage_line_rectified_mean(phase: int) -> str:
    """Rectified mean phase-to-phase voltage (``VOLT<nn>:RMEAN``)."""
    return _fn.voltage_line_rectified_mean(phase)


def voltage_line_rectified_mean_corrected(phase: int) -> str:
    """Corrected rectified mean phase-to-phase voltage (``VOLT<nn>:RMCORR``)."""
    return _fn.voltage_line_rectified_mean_corrected(phase)


def voltage_line_form_factor(phase: int) -> str:
    """Phase-to-phase voltage form factor (``VOLT<nn>:FFAC``)."""
    return _fn.voltage_line_form_factor(phase)


def voltage_line_thd(phase: int) -> str:
    """Phase-to-phase voltage THD (``VOLT<nn>:THD``)."""
    return _fn.voltage_line_thd(phase)


def voltage_line_harmonic_content(phase: int) -> str:
    """Phase-to-phase voltage harmonic content (``VOLT<nn>:HCONT``)."""
    return _fn.voltage_line_harmonic_content(phase)


def voltage_line_fundamental_content(phase: int) -> str:
    """Phase-to-phase voltage fundamental content (``VOLT<nn>:FCONT``)."""
    return _fn.voltage_line_fundamental_content(phase)


def voltage_line_phase(phase: int) -> str:
    """Absolute phase-to-phase voltage phase (``VOLT<nn>:PHAS``)."""
    return _fn.voltage_line_phase(phase)


# -- Current -------------------------------------------------------------------


def current(phase: int = 0) -> str:
    """True-RMS current (``CURR<n>``)."""
    return _fn.current(phase)


def current_ac(phase: int = 0) -> str:
    """RMS current without the DC component (``CURR<n>:AC``)."""
    return _fn.current_ac(phase)


def current_mean(phase: int = 0) -> str:
    """Mean (DC) current (``CURR<n>:MEAN``)."""
    return _fn.current_mean(phase)


def current_rectified_mean(phase: int = 0) -> str:
    """Rectified mean current (``CURR<n>:RMEAN``)."""
    return _fn.current_rectified_mean(phase)


def current_rectified_mean_corrected(phase: int = 0) -> str:
    """Corrected rectified mean current (``CURR<n>:RMCORR``)."""
    return _fn.current_rectified_mean_corrected(phase)


def current_peak_to_peak(phase: int) -> str:
    """Peak-to-peak current (``CURR<n>:PTP``); per channel only."""
    return _fn.current_peak_to_peak(phase)


def current_peak_high(phase: int) -> str:
    """Highest sampled current in the averaging interval (``CURR<n>:PHIGH``)."""
    return _fn.current_peak_high(phase)


def current_peak_low(phase: int) -> str:
    """Lowest sampled current in the averaging interval (``CURR<n>:PLOW``)."""
    return _fn.current_peak_low(phase)


def current_crest_factor(phase: int) -> str:
    """Current crest factor (``CURR<n>:CFAC``)."""
    return _fn.current_crest_factor(phase)


def current_form_factor(phase: int) -> str:
    """Current form factor (``CURR<n>:FFAC``)."""
    return _fn.current_form_factor(phase)


def current_harmonic_content(phase: int) -> str:
    """Current harmonic content (``CURR<n>:HCONT``)."""
    return _fn.current_harmonic_content(phase)


def current_fundamental_content(phase: int) -> str:
    """Current fundamental content (``CURR<n>:FCONT``)."""
    return _fn.current_fundamental_content(phase)


def current_thd(phase: int) -> str:
    """Current total harmonic distortion (``CURR<n>:THD``)."""
    return _fn.current_thd(phase)


def current_phase(phase: int) -> str:
    """Absolute current phase against the SYNC signal (``CURR<n>:PHAS``)."""
    return _fn.current_phase(phase)


# -- Power and derived quantities ----------------------------------------------


def active_power(phase: int = 0) -> str:
    """Active power P (``POW<n>:ACT``)."""
    return _fn.active_power(phase)


def apparent_power(phase: int = 0) -> str:
    """Apparent power S (``POW<n>:APP``)."""
    return _fn.apparent_power(phase)


def reactive_power(phase: int = 0) -> str:
    """Reactive power Q (``POW<n>:REAC``).

    Note the short forms: ``POWer:REACtive`` is ``REAC``, while the reactance
    quantity (:func:`series_reactance`) is ``REACT``.
    """
    return _fn.reactive_power(phase)


def power_factor(phase: int = 0) -> str:
    """Power factor (``POW<n>:FACT``)."""
    return _fn.power_factor(phase)


def corrected_power(phase: int = 0) -> str:
    """Corrected power (``POW<n>:CORR``)."""
    return _fn.corrected_power(phase)


def efficiency(phase: int = 0) -> str:
    """Electrical efficiency (``POW<460>:EFF``); aggregate suffixes only.

    The two reference functions are chosen with
    :meth:`flukenorma.Norma.set_efficiency_reference`.
    """
    return _fn.efficiency(phase)


def phase_angle(phase: int = 0) -> str:
    """Phase angle between U and I, arccos of the power factor (``PHAS<n>``)."""
    return _fn.phase_angle(phase)


def apparent_impedance(phase: int = 0) -> str:
    """Apparent impedance (``IMP<n>:APP``)."""
    return _fn.apparent_impedance(phase)


def series_resistance(phase: int = 0) -> str:
    """Series resistance (``RES<n>:SER``)."""
    return _fn.series_resistance(phase)


def parallel_resistance(phase: int = 0) -> str:
    """Parallel resistance (``RES<n>:PAR``)."""
    return _fn.parallel_resistance(phase)


def series_reactance(phase: int = 0) -> str:
    """Series reactance (``REACT<n>:SER``)."""
    return _fn.series_reactance(phase)


def parallel_reactance(phase: int = 0) -> str:
    """Parallel reactance (``REACT<n>:PAR``)."""
    return _fn.parallel_reactance(phase)


# -- Instrument-wide quantities -------------------------------------------------


def frequency() -> str:
    """Frequency of the SYNC source (``FREQ``)."""
    return _fn.frequency()


def time_interval() -> str:
    """Length of the averaging interval in seconds (``TIME``)."""
    return _fn.time_interval()


def time_relative() -> str:
    """Seconds since the timer was reset (``TIME:REL``)."""
    return _fn.time_relative()


# -- Modifiers ------------------------------------------------------------------


def harmonic(function: str) -> str:
    """The harmonic of a function (``...:HAR``).

    The order is selected with :attr:`flukenorma.Norma.harmonic_order`.
    """
    return _fn.harmonic(function)


def minimum(function: str) -> str:
    """Smallest value of a function since the MIN/MAX registers were cleared."""
    return _fn.minimum(function)


def maximum(function: str) -> str:
    """Largest value of a function since the MIN/MAX registers were cleared."""
    return _fn.maximum(function)


def integral(function: str) -> str:
    """Sum of all values of a function (``...:INT``).

    Requires :attr:`flukenorma.Norma.integral_enabled`.
    """
    return _fn.integral(function)


def integral_positive(function: str) -> str:
    """Sum of the positive values of a function (``...:IPOS``)."""
    return _fn.integral_positive(function)


def integral_negative(function: str) -> str:
    """Sum of the negative values of a function (``...:INEG``)."""
    return _fn.integral_negative(function)

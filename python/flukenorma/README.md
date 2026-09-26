# flukenorma

**Talk to a Fluke NORMA 4000/5000 power analyzer in three lines of Python.**

SCPI over TCP, wrapped in a fully typed, pythonic package on top of the compiled
`flukenorma._core` (pybind11) extension — dataclasses instead of opaque C++ types,
real enums instead of magic strings, properties instead of getter/setter pairs.

```python
from flukenorma import Norma, fn

with Norma.connect("192.168.1.100") as norma:
    norma.functions = [fn.voltage(1), fn.current(1), fn.active_power(1)]
    print(norma.read()["VOLT1"].value)
```

## Install

```bash
pip install .          # from the repository root — builds the C++ core + extension
```

Requires Python ≥ 3.9, CMake ≥ 3.24 and a C++ compiler. The package ships a
`py.typed` marker, so mypy and pyright see the whole API.

## The full workflow

```python
import time
from flukenorma import Norma, WiringSystem, fn

with Norma.connect("192.168.1.100", timeout=5.0) as norma:
    print(norma.identify())            # Identification(manufacturer='Fluke', model='NORMA4000', ...)

    norma.prepare()                    # ASCII transfer format, concurrent, clean queue
    norma.reset()
    norma.wiring_system = WiringSystem.THREE_WATTMETER
    norma.sync_to_voltage(1)
    norma.set_voltage_range(1, 300.0)
    norma.set_current_autorange(1)
    norma.aperture = 1.0               # seconds of averaging

    norma.functions = [fn.voltage(1), fn.current(1), fn.active_power(1)]
    norma.continuous = True
    time.sleep(2)

    for m in norma.read():
        if m.is_valid:
            print(f"{m.function:>10} = {m.value:g}")
        else:
            print(f"{m.function:>10}   {m.status!r}")
```

## What you get

| Instead of | You write |
|---|---|
| `set_aperture(1.0)` / `aperture()` | `norma.aperture = 1.0` |
| `set_functions([...])` / `functions()` | `norma.functions = [...]` |
| `"POW1:ACT"` string soup | `fn.active_power(1)` |
| A bare list of floats | `Reading` — iterate, index, or look up by name |
| Integer status bits | `MeasurementStatus` (`IntFlag`) + `Measurement.is_valid` |
| Manual `close()` | `with Norma.connect(...) as norma:` |
| Raw register integers | `OperationStatus`, `QuestionableStatus`, `ChannelStatus` (`IntFlag`) |

### Readings that label themselves

```python
reading = norma.read()
len(reading)                  # 3
reading[0].value              # by position
reading["VOLT1"].value        # by <function> name (case-insensitive)
reading.values                # (229.97, 4.31, 989.5)
reading["POW1:ACT"].status    # <MeasurementStatus.NORMAL: 0>
```

### Function names, typed

`fn` mirrors the manual's `<function>` grammar — phase `1..6` = L1..L6,
`12/23/31` = phase-to-phase, `0` (default) = total/average:

```python
fn.voltage(1)             # "VOLT1"        fn.apparent_power()      # "POW:APP"
fn.current_ac(2)          # "CURR2:AC"     fn.power_factor(1)       # "POW1:FACT"
fn.voltage_line_mean(31)  # "VOLT31:MEAN"  fn.frequency()           # "FREQ"
fn.voltage_thd(1)         # "VOLT1:THD"    fn.series_reactance(1)   # "REACT1:SER"
```

The whole function table is covered, and the trailing modifiers compose::

```python
fn.harmonic(fn.active_power(1))   # "POW1:ACT:HAR"  — order set by norma.harmonic_order
fn.minimum(fn.voltage(1))         # "VOLT1:MIN"
fn.integral(fn.active_power())    # "POW:ACT:INT"   — needs norma.integral_enabled
```

A suffix the manual does not define raises `ValueError` here rather than coming
back from the instrument as SCPI error -113:

```python
fn.voltage(99)        # ValueError: invalid phase suffix for function "VOLT": 99
fn.voltage(12)        # ValueError — phase-to-phase belongs to fn.voltage_line()
```

### Errors

```python
from flukenorma import NormaError, TimeoutError, ScpiError

try:
    norma.check_errors()               # raises ScpiError if the queue is non-empty
except ScpiError:
    for e in norma.read_errors():
        print(e.code, e.message)
```

All of them derive from `NormaError`; `ConnectionError` and `TimeoutError`
deliberately shadow the builtins to mirror the binding.

### Testing without hardware

The repository ships a simulated instrument. Start it and connect as usual —
it speaks the real protocol, so the code under test needs no changes:

```bash
./build/linux-make/module/simulator/norma_sim --port 2300
```

```python
with Norma.connect("127.0.0.1", 2300) as norma:
    norma.functions = [fn.voltage(1), fn.current(1), fn.active_power(1)]
    print(norma.read())
```

For a unit test with no process at all, `Norma` accepts anything satisfying the
`NormaLike` protocol, so a hand-written fake drops straight in:

```python
norma = Norma(FakeInstrument())        # no instrument, no network, still typed
```

### Escape hatches

Nothing is locked away — every command in the manual is one call away:

```python
norma.write("SENS:FUNC:ON 'VOLT1'")
norma.write("INP1:SHUN EXT;GAIN 25.0")  # a ';'-separated command line
norma.query("*IDN?")
norma.instrument                        # the raw compiled object
```

A string containing a line terminator is refused with `ValueError`: it would put
a second command on the wire whose response nobody reads, leaving every later
query one answer behind.

## Reference

* Protocol: [`docs/Fluke-NORMA-TCP-API.md`](../../docs/Fluke-NORMA-TCP-API.md)
* C++ core, C ABI and bindings: [`module/README.md`](../../module/README.md)
* The simulated instrument: [`module/simulator/README.md`](../../module/simulator/README.md)
* Project overview and roadmap: [`README.md`](../../README.md)

MIT licensed.

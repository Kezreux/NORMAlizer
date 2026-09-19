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

    norma.reset()
    norma.wiring_system = WiringSystem.THREE_WATTMETER
    norma.sync_to_voltage(1)
    norma.set_voltage_range(1, 300.0)
    norma.set_current_autorange(1)
    norma.aperture = 1.0               # seconds of averaging

    norma.functions = [fn.voltage(1), fn.current(1), fn.active_power(1)]
    norma.set_continuous(True)
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
fn.voltage(1)        # "VOLT1"       fn.apparent_power(0)  # "POW:APP"
fn.current_ac(2)     # "CURR2:AC"    fn.power_factor(1)    # "POW1:FACT"
fn.voltage_mean(31)  # "VOLT31:MEAN" fn.frequency()        # "FREQ"
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

`Norma` accepts anything satisfying the `NormaLike` protocol, so a hand-written
fake drops straight in:

```python
norma = Norma(FakeInstrument())        # no instrument, no network, still typed
```

### Escape hatches

Nothing is locked away — every command in the manual is one call away:

```python
norma.write("SENS:FUNC:ON 'VOLT1'")
norma.query("*IDN?")
norma.instrument                       # the raw compiled object
```

## Reference

* Protocol: [`docs/Fluke-NORMA-TCP-API.md`](../docs/Fluke-NORMA-TCP-API.md)
* C++ core, C ABI and bindings: [`module/README.md`](../module/README.md)

MIT licensed.

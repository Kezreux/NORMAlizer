# NORMAlizer

**Cross-platform desktop application and library for remote-controlling Fluke NORMA 4000/5000 power analyzers.**

> ⚠️ **Status: work in progress.** The library, its bindings and its test suite are
> in place and green; the desktop application is not built yet. There is no
> release and the API is not stable — expect things to move. See the
> [roadmap](#roadmap) for what is done and what is not.

## What is this?

The [Fluke NORMA 4000/5000](https://www.fluke.com/) precision power analyzers can be fully
remote-controlled with **SCPI** commands over **TCP/IP** (fixed port 23), as well as over
RS-232, USB and GPIB — the command set is identical across all of them.

What the instruments do *not* come with is a pleasant, modern, cross-platform way of doing
that. The vendor examples are ANSI C against a VISA library from 2007. NORMAlizer aims to
fix that:

- **A reusable library** that speaks the NORMA protocol properly — connect, configure,
  measure, read back values, handle the SCPI error queue — so you never have to hand-roll
  socket code and string parsing again.
- **A desktop application** on top of it, so you can connect to an analyzer, watch live
  U/I/P measurements and send raw SCPI commands without writing a line of code.

## It ships with the instrument

A power analyzer is an awkward thing to develop against: there is one of it, it is
expensive, and it is usually doing something else. So the repository contains a
**simulated NORMA** — a state machine that parses the real SCPI grammar, keeps the
instrument's settings, rejects what a real firmware would reject, and computes
measurements from a signal you configure.

```console
$ norma_sim --port 2300
listening on 127.0.0.1:2300
identifies as Fluke,NORMA5000,KN34512BA,01.05

$ printf '*IDN?\nFUNC "VOLT1","CURR1","POW1:ACT"\nDATA?\nSYSTEM:NOPE\nSYST:ERR?\n' | nc 127.0.0.1 2300
Fluke,NORMA5000,KN34512BA,01.05
+2.30000E+02,+1.00000E+00,+2.30000E+02
-113,"Undefined header;SYSTEM:NOPE"
```

Two things follow from it. The library's test suite verifies the whole stack —
including the real TCP transport, over loopback — on a laptop with nothing plugged
in. And the desktop app can be written against a working instrument long before an
analyzer is free on the bench. See
[`module/simulator/README.md`](module/simulator/README.md).

## Architecture

One protocol implementation, reused everywhere, rather than a separate client per
language:

| Layer | What it is | Consumers |
| --- | --- | --- |
| **Core library** | C++17, SCPI-over-TCP, transport abstraction so RS-232/USB can be added later | everything below |
| **Simulator** | The same protocol, offline: instrument model, in-process transport, loopback server, `norma_sim` | tests, application development |
| **C ABI** | Stable C interface + shared library (`flukenorma_c`) | C#, Rust, LabVIEW, MATLAB, … |
| **Python bindings** | pybind11 extension wrapped in a typed `flukenorma` package | scripts, lab automation, notebooks |
| **Desktop app** | Avalonia (.NET), MVVM — connection, live measurements, SCPI console | end users on Windows/Linux/macOS |

```
├── CMakeLists.txt        Top-level C++ build
├── CMakePresets.json
├── docs/                 Protocol documentation
├── module/               Core C++ library, simulator, C ABI, Python bindings, examples, tests
├── python/               Typed Python package
└── apps/desktop/         Avalonia desktop application
```

## Building

Requirements: CMake ≥ 3.24 and a C++17 compiler (MSVC 2022, GCC, Clang). Asio,
pybind11 and Catch2 are fetched during configuration, or taken from vcpkg or the
system when available — a plain checkout builds with no setup.

```bash
# Linux
cmake --preset linux            # or linux-make, if Ninja is not installed
cmake --build --preset linux
ctest --preset linux            # 202 tests, no instrument required
```

```powershell
# Windows
cmake --preset windows-msvc
cmake --build --preset windows-msvc
ctest --preset windows-msvc
```

```bash
# Python
pip install .
```

The .NET SDK is needed for the desktop app, which is not wired up yet.

## Where to look

| | |
|---|---|
| [`docs/Fluke-NORMA-TCP-API.md`](docs/Fluke-NORMA-TCP-API.md) | The instrument's remote-control API: interfaces, SCPI syntax, every subsystem and command, the status registers, the error codes, the original programming examples |
| [`module/README.md`](module/README.md) | The C++ library, the C ABI, building, and the design decisions behind them |
| [`module/simulator/README.md`](module/simulator/README.md) | What the offline instrument models, the SCPI it accepts, the signal model, how to extend it |
| [`module/tests/README.md`](module/tests/README.md) | The test layers, running against real hardware, sanitizers, CI |
| [`python/flukenorma/README.md`](python/flukenorma/README.md) | The Python package |
| [`apps/desktop/README.md`](apps/desktop/README.md) | The desktop application |

The protocol reference is compiled and translated from the *Fluke NORMA 4000/5000
Remote Control Users Guide* (June 2007, Rev. 2).

If all you want is to talk to an analyzer right now:

```sh
telnet 192.168.1.100 23
*IDN?
→ Fluke,NORMA4000,KN34512BA,01.00
```

Commands are plain ASCII lines terminated with `\n`; queries answer with a single line.
A typical measurement flow is
`*RST` → configure (`ROUT:SYST`, `SYNC:SOUR`, ranges, `APER`, `FUNC`) → `INIT:CONT ON` → `DATA?`.

## Roadmap

- [x] Core C++ library: TCP transport, SCPI client, high-level instrument facade
- [x] Simulated instrument, so the library and the app can be developed and tested without hardware
- [x] Test suite: unit, wire-format conformance, workflow and socket-level tests, plus a suite that runs against either the simulator or a real analyzer
- [x] C ABI shared library
- [x] Python bindings and package
- [x] CI: build and test on Linux and Windows, sanitizers, warnings as errors, Python wheels
- [ ] Verify against a real NORMA 4000/5000 — everything so far is checked against the manual and the simulator
- [ ] Python (pytest) and C# (xunit) test suites; `NORMAlizer.Core.Tests` currently references an empty project rather than `NORMAlizer.Interop`
- [ ] `SENSe2`/`SOURce` — the PI1 process-interface option (torque, speed, analog outputs)
- [ ] Avalonia desktop app: connection, live measurements, SCPI console
- [ ] Releases for Windows/Linux/macOS

## Contributing

Issues, ideas and pull requests are welcome — especially from anyone with a NORMA 4000/5000
on their bench who can test against real hardware. The library is written against the
manual and verified against the simulator, so the most valuable thing anyone can
do right now is run the hardware suite on a real instrument:

```bash
NORMA_HOST=<ip> ctest --test-dir build -L hardware --output-on-failure
```

Start with `norma_hardware_tests "[hardware]~[state]"`, which changes nothing on
the instrument. A failure there is a finding worth reporting.

## License

Not chosen yet — until a license file is added, the usual "all rights reserved" applies.
If you want to use this for something, open an issue and it will be sorted out.

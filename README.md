# NORMAlizer

**Cross-platform desktop application and library for remote-controlling Fluke NORMA 4000/5000 power analyzers.**

> ⚠️ **Status: early work in progress.** The repository is brand new — the structure is
> still being laid out, there is no release, no stable API and no test coverage worth the
> name yet. Expect things to move around. This README exists to explain the idea; it will
> be rewritten once there is an actual application to document.

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

## Planned architecture

The goal is one protocol implementation, reused everywhere, rather than a separate client
per language:

| Layer | What it is | Consumers |
| --- | --- | --- |
| **Core library** | C++17, SCPI-over-TCP, transport abstraction so RS-232/USB can be added later | everything below |
| **C ABI** | Stable C interface + shared library (`flukenorma_c`) | C#, Rust, LabVIEW, MATLAB, … |
| **Python bindings** | pybind11 extension wrapped in a typed `flukenorma` package | scripts, lab automation, notebooks |
| **Desktop app** | Avalonia (.NET), MVVM — connection, live measurements, SCPI console | end users on Windows/Linux/macOS |

Rough repository layout (in flux):

```
├── CMakeLists.txt        Top-level C++ build
├── CMakePresets.json
├── docs/                 Protocol documentation
├── module/               Core C++ library, C ABI, Python bindings, examples, tests
├── python/               Typed Python package
└── apps/desktop/         Avalonia desktop application
```

## Documentation

[`docs/Fluke-NORMA-TCP-API.md`](docs/Fluke-NORMA-TCP-API.md) is a complete, structured
reference for the instrument's remote-control API, compiled from the *Fluke NORMA 4000/5000
Remote Control Users Guide* (June 2007, Rev. 2). It covers the interfaces, the SCPI syntax,
every subsystem and command, the status reporting system, the error codes and the original
programming examples — including a raw TCP socket example without VISA.

It is written in Norwegian; the SCPI commands and code examples are of course language-neutral.

If all you want is to talk to an analyzer right now:

```sh
telnet 192.168.1.100 23
*IDN?
→ Fluke,NORMA4000,KN34512BA,01.00
```

Commands are plain ASCII lines terminated with `\n`; queries answer with a single line.
A typical measurement flow is
`*RST` → configure (`ROUT:SYST`, `SYNC:SOUR`, ranges, `APER`, `FUNC`) → `INIT:CONT ON` → `DATA?`.

## Building

There is no supported build yet — the pieces are still being assembled and the build
files are in flux. When it settles, the requirements will be CMake ≥ 3.24 with a C++17
compiler for the native side, and the .NET SDK for the desktop app.

## Roadmap

- [ ] Core C++ library: TCP transport, SCPI client, high-level instrument facade
- [ ] Unit tests against a mock transport + optional tests against real hardware
- [ ] C ABI shared library
- [ ] Python bindings and package
- [ ] Avalonia desktop app: connection, live measurements, SCPI console
- [ ] CI builds and releases for Windows/Linux/macOS

## Contributing

Issues, ideas and pull requests are welcome — especially from anyone with a NORMA 4000/5000
on their bench who can test against real hardware. The project is early enough that
feedback on the design still shapes it.

## License

Not chosen yet — until a license file is added, the usual "all rights reserved" applies.
If you want to use this for something, open an issue and it will be sorted out.

# FlukeNorma — C++/Python wrapper for the Fluke NORMA 4000/5000 TCP API

A C++ library that wraps the remote control API (SCPI over TCP, port 23) for
**Fluke NORMA 4000/5000** power analyzers, with bindings that expose the same
API to multiple languages:

| Language | Mechanism | Target/package |
|---|---|---|
| **C++** | Modern CMake package (`find_package(FlukeNorma)` or `FetchContent`) | `FlukeNorma::core` |
| **Python** | [pybind11](https://github.com/pybind/pybind11) (`pip install .`) | `flukenorma` |
| **C# / LabVIEW / Rust / Java / MATLAB / ...** | Stable C ABI (shared library + `fluke_norma_c.h`) | `FlukeNorma::c` (`flukenorma_c.dll` / `.so`) |

The protocol reference is available in [Fluke-NORMA-TCP-API.md](../docs/Fluke-NORMA-TCP-API.md).
The TCP layer uses [standalone Asio](https://think-async.com/Asio/) (header-only,
fetched automatically, and hidden behind pimpl so consumers never see Asio headers).

**It ships with the instrument.** `module/simulator/` is a stateful, offline
NORMA: it parses the real SCPI grammar, models the instrument's state and
synthesizes measurements from a configurable signal. The test suite drives the
library against it — including over a real loopback socket — so the whole stack
is verifiable without hardware, and an application can be developed against
`norma_sim` before an analyzer is on the bench.

## Structure

```
├── CMakeLists.txt               Top-level build (includes module/)
├── CMakePresets.json
├── pyproject.toml               pip install . (scikit-build-core)
├── python/
│   └── flukenorma/              Typed, pythonic Python package (wraps flukenorma._core)
└── module/
    ├── include/fluke/norma/     Public C++ headers
    │   ├── transport.hpp        Transport abstraction (TCP now; RS-232/USB possible later)
    │   ├── tcp_transport.hpp    Asio-based TCP transport (port 23, \n-terminated lines)
    │   ├── scpi_client.hpp      SCPI line protocol: send/query, parsing, error queue
    │   ├── instrument.hpp       NormaInstrument — the typed facade over the command set
    │   ├── types.hpp            Enums, status bits, fn:: helpers for <function> names
    │   └── error.hpp            Error hierarchy (ConnectionError, TimeoutError, ScpiError, ...)
    ├── src/                     Core library implementation
    ├── simulator/               Offline instrument — see simulator/README.md
    ├── capi/                    C ABI (fluke_norma_c.h + flukenorma_c shared library)
    ├── bindings/python/         pybind11 extension (built as flukenorma._core)
    ├── examples/                C++ and Python examples (identify, U/I/P measurement)
    ├── tests/                   Catch2 test suite — see tests/README.md
    └── cmake/                   Dependencies (FetchContent), warnings, package exports
```

## The API

`NormaInstrument` covers the command set from the manual's quick reference:
IEEE 488.2 common commands, `ROUTe`, `INPut`, `SENSe` (ranging, scaling,
averaging, functions, data), `SYNC`, `INITiate`/`ABORt`, `TRIGger`, `CALCulate`
(harmonics/spectrum, integration, power), `SENSe:SWEep` + `TRACe` (memory
recording), `FORMat`, `DISPlay`, `OUTPut`, `SYSTem`, `TIMer` and the full
`STATus` register set. Anything not wrapped is still reachable through
`write()`/`query()`.

Each method's doc comment names the SCPI command it sends, so it can be looked
up in the protocol reference. The exact wire format of every one of them is
pinned in `tests/test_command_conformance.cpp` — one table, checked against the
manual.

Two things are worth knowing before the first measurement:

- **`prepare()`** clears the status registers, switches the transfer format to
  ASCii, enables concurrent functions and drains a stale error queue, without
  touching the measurement configuration. `FORMat` survives a disconnect, so an
  instrument left in `REAL,64` by a previous session answers every measurement
  query with a binary block this library cannot parse. `connect()` deliberately
  changes nothing, because the instrument may be mid-measurement.
- **`data()`/`data_with_status()` with an explicit function list also replaces
  the configured `FUNCtion` list** on the instrument — the manual lists `DATA?`
  as invalidating `FUNCtion[:ON]`. A later `data()` with no argument returns
  those functions, not the ones `set_functions()` set.

## Building (C++)

Requirements: CMake ≥ 3.24 and a C++17 compiler (MSVC 2022, GCC, Clang).
Dependencies (Asio, pybind11, Catch2) are fetched automatically during configuration,
or taken from vcpkg/the system if available.

```powershell
# Windows (Visual Studio 2022)
cmake --preset windows-msvc
cmake --build --preset windows-msvc
ctest --preset windows-msvc
```

```bash
# Linux (Ninja)
cmake --preset linux
cmake --build --preset linux
ctest --preset linux

# Linux without Ninja installed
cmake --preset linux-make
cmake --build --preset linux-make
ctest --preset linux-make

# Linux with the sanitizers, which is how the lifetime bugs get caught
cmake --preset linux-asan
cmake --build --preset linux-asan
ctest --preset linux-asan
```

CMake options: `NORMA_BUILD_PYTHON`, `NORMA_BUILD_C_API`,
`NORMA_BUILD_EXAMPLES`, `NORMA_BUILD_TESTS`, `NORMA_BUILD_SIMULATOR`,
`NORMA_INSTALL` (all `ON` by default) and `NORMA_WERROR` (`OFF`; warnings as
errors for this project's own targets, not for the fetched dependencies).

## Testing

`ctest` runs 202 test cases and needs no instrument: the library is driven against
the simulator, including over a real loopback socket.

```bash
ctest --test-dir build --output-on-failure        # everything
ctest --test-dir build -L simulator               # the instrument suite vs. the simulator
NORMA_HOST=192.168.1.100 ctest --test-dir build -L hardware --output-on-failure
```

The suite is layered — parsing helpers, protocol, wire-format conformance,
response parsing and validation, the simulator itself, workflows in process, the
real socket — and `test_hardware_norma.cpp` is built twice, so the same assertions
run against the simulator on every `ctest` and against a real instrument when you
set `NORMA_HOST`. The `[state]`-tagged cases reconfigure the instrument.

**See [`tests/README.md`](tests/README.md)** for what each layer establishes, why
there is both a conformance table and an integration sweep, the sanitizer and CI
setup, and what to touch when adding a command.

## The simulator

`module/simulator/` is a stateful, offline NORMA: it parses the real SCPI grammar,
keeps the instrument's state, rejects what a real firmware would reject, and
computes measurements from a configurable signal. It is what makes the library
verifiable without hardware — and what lets an application be written before an
analyzer is free.

```console
$ norma_sim --port 2300
listening on 127.0.0.1:2300
identifies as Fluke,NORMA5000,KN34512BA,01.05

$ printf '*IDN?\nFUNC "VOLT1","CURR1","POW1:ACT"\nDATA?\nSYSTEM:NOPE\nSYST:ERR?\n' | nc 127.0.0.1 2300
Fluke,NORMA5000,KN34512BA,01.05
+2.30000E+02,+1.00000E+00,+2.30000E+02
-113,"Undefined header;SYSTEM:NOPE"
```

In C++ it comes in three pieces — the instrument model, an in-process `Transport`
(fast, deterministic, with fault injection) and a loopback TCP server (which is
what exercises `TcpTransport`):

```cpp
#include <fluke/norma/simulator/simulator_transport.hpp>
#include <fluke/norma/simulator/simulator_server.hpp>

auto transport = std::make_unique<sim::SimulatorTransport>();
transport->simulator().signal().phases[0] = {230.0, 5.0, 30.0, 0.0, 0.0, 0.0, 0.0};
transport->open();
NormaInstrument norma(std::move(transport));

sim::SimulatorServer server;                       // 127.0.0.1, OS-chosen port
auto over_tcp = NormaInstrument::connect(server.host(), server.port());
```

The examples take a host and a port, so they work against it unchanged:

```bash
./build/linux-make/module/examples/norma_identify 127.0.0.1 2300
python module/examples/python/simple_power_measurement.py 127.0.0.1 2300
```

**See [`simulator/README.md`](simulator/README.md)** for what it models (and what
it deliberately does not — timing), the SCPI forms and error codes it handles, the
signal model behind the status bits, and how to add a command.

## Using from another C++ project

```cmake
# Option 1: installed package
find_package(FlukeNorma REQUIRED)
target_link_libraries(app PRIVATE FlukeNorma::core)

# Option 2: directly from the source tree
include(FetchContent)
FetchContent_Declare(FlukeNorma SOURCE_DIR "path/to/Fluke")
FetchContent_MakeAvailable(FlukeNorma)
target_link_libraries(app PRIVATE FlukeNorma::core)
```

```cpp
#include <fluke/norma/norma.hpp>
using namespace fluke::norma;

auto norma = NormaInstrument::connect("192.168.1.100"); // TCP port 23
norma.prepare();                                        // ASCII format, clean queue
auto id = norma.identify();                             // *IDN?
norma.reset();
norma.set_wiring_system(WiringSystem::ThreeWattmeter);
norma.sync_to_voltage(1);
norma.set_voltage_range(1, 300.0);
norma.set_current_autorange(1, true);
norma.set_aperture(1.0);
norma.set_functions({fn::voltage(1), fn::current(1), fn::active_power(1)});
norma.set_continuous(true);
auto reading = norma.data_with_status();                // DATA:STAT?
norma.check_errors();                                   // SYST:ERR?
```

## Python

```powershell
pip install .          # builds the C++ core + pybind11 module via scikit-build-core
```

```python
from flukenorma import Norma, WiringSystem, fn

with Norma.connect("192.168.1.100") as instrument:
    instrument.prepare()
    print(instrument.identify())
    instrument.wiring_system = WiringSystem.THREE_WATTMETER
    instrument.aperture = 1.0
    instrument.functions = [fn.voltage(1), fn.current(1), fn.active_power(1)]
    instrument.continuous = True
    for measurement in instrument.read():
        print(measurement.function, measurement.value, measurement.status)
```

Without `pip install`, the package can be imported from the build tree — the
build stages it next to the compiled extension:

```bash
PYTHONPATH=build/linux-make/module/bindings/python python -c "import flukenorma"
```

## Other languages (C ABI)

`capi/` builds `flukenorma_c` (a shared library) with a flat C API
(`fluke_norma_c.h`): `norma_connect`, `norma_close`, `norma_disconnect`,
`norma_is_open`, `norma_prepare`, `norma_identify`, `norma_write`, `norma_query`,
`norma_set_functions`, `norma_read_data`, `norma_read_data_with_status`,
`norma_check_errors`, `norma_set_timeout`, `norma_last_error`,
`norma_last_scpi_code`, `norma_version`. Every call returns `norma_status`;
the rest of the command set is reachable through `norma_write`/`norma_query`.

```csharp
[DllImport("flukenorma_c")] static extern int norma_connect(
    string host, ushort port, uint timeoutMs, out IntPtr instrument);
[DllImport("flukenorma_c")] static extern int norma_query(
    IntPtr instrument, string scpi, byte[] buffer, UIntPtr size);
```

## Design overview

- **Layers:** `Transport` (bytes/lines) → `ScpiClient` (protocol, parsing,
  error queue, thread-safe queries) → `NormaInstrument` (typed facade). New transports
  (RS-232/USB-VCP) can be added by implementing `Transport`; the simulator's
  in-process transport is one such implementation.
- **Timeouts:** all I/O has a timeout (default 5 s, configurable); `*OPC?` has
  its own, longer timeout. A timed-out query closes the socket on purpose, so a
  late response can never be mistaken for the answer to the next query — the
  session reports itself closed instead of silently returning stale values.
- **Errors:** network errors → `ConnectionError`/`TimeoutError`; unrecognized responses
  → `ProtocolError`; the instrument's error queue (`SYST:ERR?`) → `ScpiError` through
  `check_errors()`, which carries the whole queue in `all()`. Arguments the
  instrument would reject (a phase outside 1..6, an aperture outside
  0.015..3600 s, an undefined `fn::` suffix, a command containing a line
  terminator) raise `std::invalid_argument` before anything is sent.
- **NaN:** the SCPI representation `9.91E+37` (Undefined/Not available) is mapped
  to `NaN`, and `Reading::is_valid()` cross-checks it against the status bits.
- **Testability:** the simulator is a first-class part of the library, not a test
  fixture bolted on the side.

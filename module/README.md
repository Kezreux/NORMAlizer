# FlukeNorma — C++/Python wrapper for the Fluke NORMA 4000/5000 TCP API

A C++ library that wraps the remote control API (SCPI over TCP, port 23) for
**Fluke NORMA 4000/5000** power analyzers, with bindings that expose the same
API to multiple languages:

| Language | Mechanism | Target/package |
|---|---|---|
| **C++** | Modern CMake package (`find_package(FlukeNorma)` or `FetchContent`) | `FlukeNorma::core` |
| **Python** | [pybind11](https://github.com/pybind/pybind11) (`pip install .`) | `flukenorma` |
| **C# / LabVIEW / Rust / Java / MATLAB / ...** | Stable C ABI (shared library + `fluke_norma_c.h`) | `FlukeNorma::c` (`flukenorma_c.dll` / `.so`) |

The protocol reference is available in [Fluke-NORMA-TCP-API.md](Fluke-NORMA-TCP-API.md).
The TCP layer uses [standalone Asio](https://think-async.com/Asio/) (header-only,
fetched automatically, and hidden behind pimpl so consumers never see Asio headers).

## Structure

```
├── CMakeLists.txt               Top-level build (includes module/)
├── CMakePresets.json
├── pyproject.toml               pip install . (scikit-build-core)
├── flukenorma/                  Typed, pythonic Python package (wraps flukenorma._core)
└── module/
    ├── include/fluke/norma/     Public C++ headers
    │   ├── transport.hpp        Transport abstraction (TCP now; RS-232/USB possible later)
    │   ├── tcp_transport.hpp    Asio-based TCP transport (port 23, \n-terminated lines)
    │   ├── scpi_client.hpp      SCPI line protocol: send/query, parsing, error queue
    │   ├── instrument.hpp       NormaInstrument — high-level facade (typical measurement workflow)
    │   ├── types.hpp            Enums, status flags, fn:: helpers for <function> names
    │   └── error.hpp            Error hierarchy (ConnectionError, TimeoutError, ScpiError, ...)
    ├── src/                     Core library implementation
    ├── capi/                    C ABI (fluke_norma_c.h + flukenorma_c shared library)
    ├── bindings/python/         pybind11 extension (built as flukenorma._core)
    ├── examples/                C++ and Python examples (identify, U/I/P measurement)
    ├── tests/                   Catch2 tests: unit tests (mock transport) + hardware tests (real TCP)
    └── cmake/                   Dependencies (FetchContent) and package exports
```

All commands in the manual are accessible through the `write()`/`query()` methods;
the facade covers the standard workflow: `*RST` → `ROUT:SYST` → `SYNC:SOUR` → ranges →
`APER` → `FUNC` → `INIT:CONT ON` → `DATA?`/`DATA:STAT?` → `SYST:ERR?`.

## Building (C++)

Requirements: CMake ≥ 3.24 and a C++17 compiler (MSVC 2022, GCC, Clang).
Dependencies (Asio, pybind11, Catch2) are fetched automatically during configuration,
or taken from vcpkg/the system if available.

```powershell
# Windows (Visual Studio 2022)
cmake --preset windows-msvc
cmake --build --preset windows-msvc
ctest --preset windows-msvc

# Windows with Ninja (run from "Developer PowerShell for VS 2022")
cmake --preset windows-ninja
cmake --build --preset windows-ninja
```

```bash
# Linux
cmake --preset linux
cmake --build --preset linux
ctest --preset linux
```

CMake options: `NORMA_BUILD_PYTHON`, `NORMA_BUILD_C_API`,
`NORMA_BUILD_EXAMPLES`, `NORMA_BUILD_TESTS`, `NORMA_INSTALL` (all `ON` by
default).

### Hardware tests (real instrument)

`norma_hardware_tests` runs against a real NORMA 4000/5000 over TCP (no
mock). The tests are skipped (Skipped) if `NORMA_HOST` is not set,
so a regular `ctest` run does not require an instrument.

```powershell
# Windows
$env:NORMA_HOST = "192.168.1.100"          # NORMA_PORT (23) and NORMA_TIMEOUT_MS (5000) are optional
ctest --preset windows-msvc -L hardware --output-on-failure

# or run the executable directly:
.\build\windows-msvc\module\tests\Release\norma_hardware_tests.exe
```

```bash
# Linux
NORMA_HOST=192.168.1.100 ctest --preset linux -L hardware --output-on-failure
```

Tests tagged `[state]` reconfigure the instrument (`*RST`, `FUNC`, `APER`,
...). Run only the read-only tests with:

```
norma_hardware_tests "[hardware]~[state]"
```

### Using from another C++ project

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
from flukenorma import Norma, fn

with Norma.connect("192.168.1.100") as instrument:
    print(instrument.identify())
    instrument.reset()
    instrument.functions = [fn.voltage(1), fn.current(1), fn.active_power(1)]
    instrument.set_continuous(True)
    print(instrument.data())
```

Without `pip install`, the module can also be imported directly from the build directory,
for example by adding `build/windows-msvc/module/bindings/python/Release` to `PYTHONPATH`.

## Other languages (C ABI)

`capi/` builds `flukenorma_c` (a shared library) with a flat C API
(`fluke_norma_c.h`): `norma_connect`, `norma_write`, `norma_query`,
`norma_read_data`, `norma_last_error`, `norma_disconnect`. Example (C#):

```csharp
[DllImport("flukenorma_c")] static extern int norma_connect(
    string host, ushort port, uint timeoutMs, out IntPtr instrument);
[DllImport("flukenorma_c")] static extern int norma_query(
    IntPtr instrument, string scpi, StringBuilder buffer, UIntPtr size);
```

## Design overview

- **Layers:** `Transport` (bytes/lines) → `ScpiClient` (protocol, parsing,
  error queue, thread-safe queries) → `NormaInstrument` (typed facade). New transports
  (RS-232/USB-VCP) can be added by implementing `Transport`.
- **Timeouts:** all I/O has a timeout (default 5 s, configurable); `*OPC?` has
  its own, longer timeout.
- **Errors:** network errors → `ConnectionError`/`TimeoutError`; unrecognized responses
  → `ProtocolError`; the instrument's error queue (`SYST:ERR?`) → `ScpiError` through
  `check_errors()`.
- **NaN:** the SCPI representation `9.91E+37` (Undefined/Not available) is mapped
  to `NaN`.
- **Testability:** unit tests run against a scripted `MockTransport` and
  verify command formatting and parsing without an instrument.

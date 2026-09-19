# FlukeNorma — C++/Python-wrapper for Fluke NORMA 4000/5000 TCP-API

C++-bibliotek som pakker inn fjernstyrings-API-et (SCPI over TCP, port 23) til
effektanalysatorene **Fluke NORMA 4000/5000**, med bindinger som gjør det samme
API-et tilgjengelig fra flere språk:

| Språk | Mekanisme | Target/pakke |
|---|---|---|
| **C++** | Moderne CMake-pakke (`find_package(FlukeNorma)` eller `FetchContent`) | `FlukeNorma::core` |
| **Python** | [pybind11](https://github.com/pybind/pybind11) (`pip install .`) | `flukenorma` |
| **C# / LabVIEW / Rust / Java / MATLAB / ...** | Stabil C-ABI (delt bibliotek + `fluke_norma_c.h`) | `FlukeNorma::c` (`flukenorma_c.dll` / `.so`) |

Protokollreferansen ligger i [Fluke-NORMA-TCP-API.md](Fluke-NORMA-TCP-API.md).
TCP-laget bruker [standalone Asio](https://think-async.com/Asio/) (header-only,
hentes automatisk, skjult bak pimpl så konsumenter aldri ser Asio-headere).

## Struktur

```
├── CMakeLists.txt               Toppnivå-bygg (peker inn i module/)
├── CMakePresets.json
├── pyproject.toml               pip install . (scikit-build-core)
└── module/
    ├── include/fluke/norma/     Offentlige C++-headere
    │   ├── transport.hpp        Transport-abstraksjon (TCP nå; RS-232/USB mulig senere)
    │   ├── tcp_transport.hpp    Asio-basert TCP-transport (port 23, \n-terminerte linjer)
    │   ├── scpi_client.hpp      SCPI-linjeprotokoll: send/query, parsing, feilkø
    │   ├── instrument.hpp       NormaInstrument — høynivåfasade (typisk måleflyt)
    │   ├── types.hpp            Enums, statusflagg, fn::-hjelpere for <function>-navn
    │   └── error.hpp            Feilhierarki (ConnectionError, TimeoutError, ScpiError, ...)
    ├── src/                     Implementasjon av kjernebiblioteket
    ├── capi/                    C-ABI (fluke_norma_c.h + flukenorma_c delt bibliotek)
    ├── bindings/python/         pybind11-modul + flukenorma-pakken
    ├── examples/                C++- og Python-eksempler (identify, U/I/P-måling)
    ├── tests/                   Catch2-tester: enhetstester (mock-transport) + hardware-tester (ekte TCP)
    └── cmake/                   Avhengigheter (FetchContent) og pakke-eksport
```

Alle kommandoer i manualen er tilgjengelige via `write()`/`query()`-lukene;
fasaden dekker standardflyten: `*RST` → `ROUT:SYST` → `SYNC:SOUR` → områder →
`APER` → `FUNC` → `INIT:CONT ON` → `DATA?`/`DATA:STAT?` → `SYST:ERR?`.

## Bygging (C++)

Krav: CMake ≥ 3.24 og en C++17-kompilator (MSVC 2022, GCC, Clang).
Avhengigheter (Asio, pybind11, Catch2) hentes automatisk ved konfigurering,
eller brukes fra vcpkg/systemet om de finnes.

```powershell
# Windows (Visual Studio 2022)
cmake --preset windows-msvc
cmake --build --preset windows-msvc
ctest --preset windows-msvc

# Windows med Ninja (kjør fra "Developer PowerShell for VS 2022")
cmake --preset windows-ninja
cmake --build --preset windows-ninja
```

```bash
# Linux
cmake --preset linux
cmake --build --preset linux
ctest --preset linux
```

CMake-opsjoner: `NORMA_BUILD_PYTHON`, `NORMA_BUILD_C_API`,
`NORMA_BUILD_EXAMPLES`, `NORMA_BUILD_TESTS`, `NORMA_INSTALL` (alle `ON` som
standard).

### Hardware-tester (ekte instrument)

`norma_hardware_tests` kjører mot et ekte NORMA 4000/5000 over TCP (ingen
mock). Testene hopper over seg selv (Skipped) hvis `NORMA_HOST` ikke er satt,
så en vanlig `ctest`-kjøring krever ikke instrument.

```powershell
# Windows
$env:NORMA_HOST = "192.168.1.100"          # NORMA_PORT (23) og NORMA_TIMEOUT_MS (5000) er valgfrie
ctest --preset windows-msvc -L hardware --output-on-failure

# eller kjør binæren direkte:
.\build\windows-msvc\module\tests\Release\norma_hardware_tests.exe
```

```bash
# Linux
NORMA_HOST=192.168.1.100 ctest --preset linux -L hardware --output-on-failure
```

Tester merket `[state]` rekonfigurerer instrumentet (`*RST`, `FUNC`, `APER`,
...). Kjør kun de lesende testene med:

```
norma_hardware_tests "[hardware]~[state]"
```

### Bruk fra et annet C++-prosjekt

```cmake
# Alternativ 1: installert pakke
find_package(FlukeNorma REQUIRED)
target_link_libraries(app PRIVATE FlukeNorma::core)

# Alternativ 2: rett fra kildetreet
include(FetchContent)
FetchContent_Declare(FlukeNorma SOURCE_DIR "sti/til/Fluke")
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
pip install .          # bygger C++-kjernen + pybind11-modulen via scikit-build-core
```

```python
import flukenorma as norma

with norma.Norma("192.168.1.100") as instrument:
    print(instrument.identify())
    instrument.reset()
    instrument.set_functions([norma.fn.voltage(1), norma.fn.current(1),
                              norma.fn.active_power(1)])
    instrument.set_continuous(True)
    print(instrument.data())
```

Uten `pip install` kan modulen også importeres rett fra byggekatalogen, f.eks.
`build/windows-msvc/module/bindings/python/Release` på `PYTHONPATH`.

## Andre språk (C-ABI)

`capi/` bygger `flukenorma_c` (delt bibliotek) med et flatt C-API
(`fluke_norma_c.h`): `norma_connect`, `norma_write`, `norma_query`,
`norma_read_data`, `norma_last_error`, `norma_disconnect`. Eksempel (C#):

```csharp
[DllImport("flukenorma_c")] static extern int norma_connect(
    string host, ushort port, uint timeoutMs, out IntPtr instrument);
[DllImport("flukenorma_c")] static extern int norma_query(
    IntPtr instrument, string scpi, StringBuilder buffer, UIntPtr size);
```

## Design i korte trekk

- **Lagdeling:** `Transport` (bytes/linjer) → `ScpiClient` (protokoll, parsing,
  feilkø, trådsikker query) → `NormaInstrument` (typet fasade). Nye transporter
  (RS-232/USB-VCP) legges til ved å implementere `Transport`.
- **Timeouts:** all I/O har timeout (standard 5 s, konfigurerbar); `*OPC?` har
  egen, lengre timeout.
- **Feil:** nettverksfeil → `ConnectionError`/`TimeoutError`; uforståelige svar
  → `ProtocolError`; instrumentets egen feilkø (`SYST:ERR?`) → `ScpiError` via
  `check_errors()`.
- **NaN:** SCPI-representasjonen `9.91E+37` (Undefined/Not available) mappes
  til `NaN`.
- **Testbarhet:** enhetstestene kjører mot en skriptet `MockTransport` og
  verifiserer kommandoformatering og parsing uten instrument.

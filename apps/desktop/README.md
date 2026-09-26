# NORMAlizer Desktop

Avalonia desktop app for remote-controlling Fluke NORMA 4000/5000 power analyzers
over TCP/SCPI. Talks to the instrument through the native `flukenorma_c` library
(built from [`module/capi`](../../module/capi)) via P/Invoke.

## Projects

| Project | Purpose |
| --- | --- |
| `src/NORMAlizer.Desktop` | Avalonia UI (MVVM, CommunityToolkit) |
| `src/NORMAlizer.Interop` | P/Invoke bindings for `flukenorma_c` (see [`fluke_norma_c.h`](../../module/capi/include/fluke_norma_c.h) for the full C surface) |
| `src/NORMAlizer.Core` | Shared domain logic (currently empty) |
| `tests/NORMAlizer.Core.Tests` | xUnit tests |

## Prerequisites

- .NET 10 SDK
- CMake 3.24+ and a C++17 toolchain (MSVC on Windows) for the native library

## Build the native library

From the repository root:

```sh
cmake --preset windows-msvc        # or: linux
cmake --build build/windows-msvc --config Release --target norma_c
# -> build/windows-msvc/module/capi/Release/flukenorma_c.dll
```

The app loads `flukenorma_c` from the standard probing paths (next to the exe,
`PATH`/`LD_LIBRARY_PATH`). During development, point the `NORMA_C_LIBRARY`
environment variable at the built file instead:

```powershell
$env:NORMA_C_LIBRARY = "F:\Git\NORMAlizer\build\windows-msvc\module\capi\Release\flukenorma_c.dll"
```

```sh
export NORMA_C_LIBRARY=$PWD/build/linux/module/capi/libflukenorma_c.so
```

## Develop

All commands run from `apps/desktop/`:

```sh
dotnet build                                     # build the whole solution
dotnet run --project src/NORMAlizer.Desktop      # start the app
```

Without a native library the app still starts; the Connection tab reports that
`flukenorma_c` was not found.

## Develop without an analyzer

The repository ships a simulated NORMA, so the UI can be built and driven end to
end with no instrument on the bench. Build and start it, then connect the app to
the host and port it prints:

```sh
# from the repository root
cmake --build build/linux --target norma_sim
./build/linux/module/simulator/norma_sim --port 2300 --verbose
```

```
listening on 127.0.0.1:2300
identifies as Fluke,NORMA5000,KN34512BA,01.05
```

It speaks the real protocol — the same SCPI grammar, the same error codes, the
same `DATA?` response format — so nothing in the app needs to know it is not an
analyzer. `--verbose` logs each connection and command count, which is handy when
a view is sending more round-trips than you expected.

The SCPI console tab is worth pointing at it first: an unknown command comes back
as `-113 "Undefined header"` from the simulator exactly as it would from the
instrument, so error handling can be built against real behaviour.

See [`module/simulator/README.md`](../../module/simulator/README.md) for what it
models, and how to set up a signal so the measurement views show plausible values.

## Test

```sh
dotnet test
```

> **Note:** the test project currently references `NORMAlizer.Core`, which is
> empty — so `NORMAlizer.Interop`, the layer that actually matters, has no
> coverage. Adding that is on the [roadmap](../../README.md#roadmap); the
> simulator above is what such tests would run against.

## Publish

Self-contained, single-file build:

```sh
dotnet publish src/NORMAlizer.Desktop -c Release -r win-x64 --self-contained -p:PublishSingleFile=true
```

Use `-r linux-x64` or `-r osx-arm64` for other platforms. Output lands in
`src/NORMAlizer.Desktop/bin/Release/net10.0/<rid>/publish/` — copy the native
library (`flukenorma_c.dll` / `libflukenorma_c.so`) into that folder before
distributing.

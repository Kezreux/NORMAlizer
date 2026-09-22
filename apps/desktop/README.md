# NORMAlizer Desktop

Avalonia desktop app for remote-controlling Fluke NORMA 4000/5000 power analyzers
over TCP/SCPI. Talks to the instrument through the native `flukenorma_c` library
(built from [`module/capi`](../../module/capi)) via P/Invoke.

## Projects

| Project | Purpose |
| --- | --- |
| `src/NORMAlizer.Desktop` | Avalonia UI (MVVM, CommunityToolkit) |
| `src/NORMAlizer.Interop` | P/Invoke bindings for `flukenorma_c` |
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

## Test

```sh
dotnet test
```

## Publish

Self-contained, single-file build:

```sh
dotnet publish src/NORMAlizer.Desktop -c Release -r win-x64 --self-contained -p:PublishSingleFile=true
```

Use `-r linux-x64` or `-r osx-arm64` for other platforms. Output lands in
`src/NORMAlizer.Desktop/bin/Release/net10.0/<rid>/publish/` — copy the native
library (`flukenorma_c.dll` / `libflukenorma_c.so`) into that folder before
distributing.

# Tests

`ctest` runs 202 test cases and needs no instrument. The library is driven against
the [offline NORMA](../simulator/README.md) — including over a real loopback
socket — so everything from the parsing helpers up to a full measurement workflow
is verified on a laptop.

```bash
ctest --test-dir build --output-on-failure      # everything
ctest --test-dir build -L simulator             # the instrument suite vs. the simulator
ctest --test-dir build -L hardware              # needs NORMA_HOST; see below
```

## The layers

Each file answers a different question, cheapest first. The split matters: when
something breaks, the layer that fails tells you where to look.

| File | Establishes |
|---|---|
| `test_scpi_client.cpp` | The pure formatting and parsing functions at their edges: quoting, CSV and `;` splitting, the SCPI NaN encoding and the threshold around it, malformed responses, the numeric extremes a caller can pass. No transport. |
| `test_scpi_client_protocol.cpp` | `ScpiClient` against a scripted transport: line framing, timeout propagation, use-while-closed, error-queue draining and its safety cap, and — with two threads — that a query's response cannot be read by the other thread. |
| `test_command_conformance.cpp` | Every facade call against the exact command string the manual defines for it. One table, so adding a command means adding a row. |
| `test_instrument.cpp` | What the facade does with responses (parsing, short `*IDN?`, truncated dates, unknown keywords), which arguments it refuses to put on the wire at all, and the whole `fn::` name grammar. |
| `test_simulator.cpp` | The simulator itself: mnemonic forms, optional nodes, compound lines, `*RST` defaults, each documented error code, and the signal/status model. |
| `test_integration_sim.cpp` | The library against the simulated instrument, in process: the canonical workflow, round-tripping every setter against its getter, `prepare()` rescuing a `FORMat REAL` link, link failures, and that no query leaves a response behind. |
| `test_integration_tcp.cpp` | The real `TcpTransport` on a real socket: connect, close, reconnect, a port nobody listens on, an unresolvable host, a peer that disappears mid-session, a stalled instrument, a response larger than one TCP segment, two responses in one segment, concurrent queries. |
| `test_hardware_norma.cpp` | One suite, two endpoints — see below. |

`mock_transport.hpp` is the scripted transport: use it when the point of a test is
the exact bytes on the wire. When the point is a *workflow*, use
`sim::SimulatorTransport`, which models state instead of replaying a script.

### Why both a conformance table and an integration sweep

They fail for different reasons, and neither alone is enough.

The conformance table pins the wire format, but both sides of it are written by
hand from the same document — so a misreading of the manual would be copied into
the expectation and the test would pass. The sweep in `test_integration_sim.cpp`
(*"the instrument accepts every command the facade sends"*) sends the whole facade
at a SCPI parser written independently from the command reference and asserts the
error queue stayed empty. A header the library spells wrongly lands there as
`-113`.

That pairing is what found the two real bugs in the simulator when this suite was
first written.

## The instrument suite, run twice

`test_hardware_norma.cpp` is written as assertions about what a NORMA *must do* —
identify as a Fluke, report an ascending list of selectable ranges, flag a NaN
measurement in its status, accept the canonical workflow. It is built into two
executables:

| Executable | `NORMA_HOST` | Label | When it runs |
|---|---|---|---|
| `norma_simulated_tests` | `simulator` (set by ctest) | `simulator` | every `ctest` run |
| `norma_hardware_tests` | an address you provide | `hardware` | only when you set `NORMA_HOST`; skips itself otherwise |

So the suite that will validate your instrument on site is exercised continuously
against the simulator. If the simulator drifts away from what a real NORMA does,
this is where it shows up first.

```bash
# Against a real instrument
NORMA_HOST=192.168.1.100 ctest --test-dir build -L hardware --output-on-failure

# Read-only cases only — nothing is reconfigured
NORMA_HOST=192.168.1.100 ./build/module/tests/norma_hardware_tests "[hardware]~[state]"
```

Environment: `NORMA_HOST` (an address, or `simulator`), `NORMA_PORT` (default 23),
`NORMA_TIMEOUT_MS` (default 5000). Cases tagged `[state]` reconfigure the
instrument (`*RST`, `FUNC`, `APER`, …) — do not run them while it is measuring
something you care about.

(Two executables rather than two `catch_discover_tests()` calls on one: Catch2
writes a single discovery file per target, so registering a target twice makes the
second call replace the first — silently, with the hardware label ending up on
nothing.)

## Sanitizers

The assertions cannot see a lifetime mistake, and this library has the two shapes
that produce them: a transport owning a socket and an `io_context` behind a pimpl,
and a simulator server running its own thread.

```bash
cmake --preset linux-asan
cmake --build build/linux-asan
ctest --preset linux-asan
```

## Warnings

`-DNORMA_WERROR=ON` turns `-Wall -Wextra -Wpedantic` (or `/W4`) into errors for
this project's own targets. It deliberately does not reach the fetched
dependencies: a warning in Catch2's sources is not something this build can fix.

## CI

[`.github/workflows/ci.yml`](../../.github/workflows/ci.yml) runs all of the above
on every push, with no instrument anywhere:

| Job | |
|---|---|
| `build-test` | Ubuntu + Windows × Debug + Release; configure, build, full `ctest`; asserts the hardware tests are registered even though they skip |
| `sanitizers` | clang with ASan + UBSan over the whole suite |
| `warnings` | `NORMA_WERROR=ON` |
| `python` | `pip install .` on Python 3.9 and 3.13, then walks the package's public surface — every `__all__` name, every `NormaLike` member against the compiled class, every enum bridged by name |

## Adding a command

Three places, so coverage cannot quietly regress:

1. A row in the table in `test_command_conformance.cpp` — the call and the exact
   command string from the manual's quick reference.
2. A line in the matching `SECTION` of the *"accepts every command"* sweep in
   `test_integration_sim.cpp`, ideally setting a value and reading it back.
3. Simulator support, so step 2 can pass — see
   [`../simulator/README.md`](../simulator/README.md#adding-a-command).

If the command is one a client would check on real hardware, add it to
`test_hardware_norma.cpp` too, as an assertion about the instrument rather than
about a string.

# The offline NORMA

`FlukeNorma::simulator` is a Fluke NORMA 4000/5000 that runs on your machine. It
exists because the alternative was to verify a protocol library by plugging in a
€10k instrument and hoping — and because a desktop application needs something to
talk to long before an analyzer is free on the bench.

It is a **state machine, not a recording**. It parses the SCPI grammar from
[`docs/Fluke-NORMA-TCP-API.md`](../../docs/Fluke-NORMA-TCP-API.md), keeps the
instrument's settings, rejects what a real firmware would reject, and computes
measurements from a signal you configure. So a command the library formats wrongly
comes back as SCPI error `-113` here, exactly as it would from the instrument —
rather than matching a string that the same author wrote into a test.

## What it models

| Subsystem | State it keeps |
|---|---|
| `ROUTe` | wiring system (3W/2W) |
| `INPut` | per channel (1..12): coupling, shunt, gain, filter |
| `SENSe` ranging | per phase (1..6): range, autorange, transducer scale; the selectable range list |
| `SENSe` averaging | aperture, sampling frequency |
| `SENSe` functions | the function list, `CONCurrent` |
| `SYNC` | state, source, level + unit, slope, filter + cut-off, timeout |
| `INITiate`/`ABORt` | continuous mode, single-shot arming |
| `TRIGger` | start/stop source, level, slope, time |
| `CALCulate` | harmonic order, transform mode/range/cycles/grouping, integration state and sources, power correction, efficiency references |
| `SENSe:SWEep` + `TRACe` | per block (1, 2): enabled, time, offset, count, sparsing, functions; a synthesized recording |
| `FORMat` | data and status format, length, byte order, transpose — including the coupling between the two |
| `DISPlay`/`OUTPut` | screen state, user functions, analog output |
| `SYSTem` | key lock, date, time, GPIB address, serial baud, language |
| `TIMer` | the relative-time clock |
| `STATus` + IEEE 488.2 | all four registers × CONDition/EVENt/ENABle/PTRansition/NTRansition, the ESR/ESE/SRE, the error queue |

121 command patterns in all — the manual's quick reference minus the PI1 process
interface (`SENSe2`, `SOURce`, torque/speed), which is optional hardware nobody
here can check against.

**It models state, not timing.** A measurement is always immediately available,
and the averaging interval only affects the value reported for the `TIME`
function. Nothing here will catch a race that depends on an instrument taking
300 ms to produce a reading.

## The SCPI it accepts

Faithful parsing is the whole point — a lenient simulator would quietly accept
malformed commands and teach you nothing:

- **Short and long mnemonics**, any casing: `VOLTage1:RANGe:UPPer?` ≡ `VOLT1:RANG?` ≡ `volt1:rang?`
- **Optional nodes**: `[SENSe:]`, `[:UPPer]`, `[:STATe]`, `[:IMMediate]`, `[:EVENt]`, `[AC|DC]`, …
- **Compound command lines** with the shared-path shortening rule:
  `INP1:SHUN EXT;GAIN 25.0` sets both, and several queries answer in order
  separated by `;`
- **Parameter types**: numeric with exponent (and *no* unit — `APER 1.0V` is an
  error, as the manual says), booleans as `ON|OFF|1|0`, character data returned
  in short form, quoted strings for `<function>` names
- **A suffix on a node that takes none** is rejected, so `APER3ture` does not
  silently work

### Errors it raises

Thirteen codes from the manual's error table, with its wording:

| Code | When |
|---|---|
| `-100`/`-101`/`-102` | command, character and syntax errors |
| `-108` | parameter not allowed |
| `-109` | a setting command with no parameter |
| `-113` | **undefined header** — the one an unknown command produces |
| `-120` | numeric data error |
| `-200` | execution error (also used when the simulator itself has no handler for a command in its table — a simulator bug, not yours) |
| `-221` | settings conflict (e.g. `GAIN` on a voltage channel, `FUNCtion` with several names while `CONCurrent` is off) |
| `-222` | data out of range |
| `-224` | illegal parameter value |
| `-350` | queue overflow, once 32 errors are pending |
| `-420` | query unterminated |

## The signal model

`DATA?` and `DATA:STATus?` are computed, not canned. Set the signal and the
numbers follow:

```cpp
auto& signal = simulator.signal();
signal.frequency = 50.0;
signal.phases[0] = {
    /* voltage_rms */ 230.0, /* current_rms */ 5.0, /* phase_shift_deg */ 30.0,
    /* voltage_dc */ 0.0,    /* current_dc */ 0.0,
    /* voltage_thd */ 0.02,  /* current_thd */ 0.05,
};
```

Covered: true-RMS / AC / mean / rectified-mean voltage and current, active,
apparent, reactive and corrected power, power factor, phase angle, apparent
impedance, series/parallel resistance and reactance, crest and form factor, THD,
peak values, frequency, the averaging interval and relative time — for a single
phase or aggregated over a three-phase system (no suffix, or `460`). A trailing
`:HAR` scales by the configured harmonic order.

**The status bits follow from the configuration**, which is what makes them worth
testing against:

| Status | Arises when |
|---|---|
| `1` Underrange | the value is below 5 % of the selected range (and autorange is off) |
| `2` Overrange | the value exceeds the selected range |
| `8` Undefined | `signal.present = false` or `SYNC:STATe OFF` — the value is also NaN |
| `16` Not available | a `MIN`/`MAX` modifier (the manual marks those unimplemented), an integral modifier while `CALCulate:INTegral` is off, or a quantity the model does not provide |
| `128` | a capacitive phase shift, on the power factor |

`signal.present = false` is the "nothing connected to the inputs" case, and it is
worth testing against: it is what a client meets first on a real bench.

## Using it

### In process — fast, deterministic, and able to break the link

```cpp
#include <fluke/norma/simulator/simulator_transport.hpp>

auto transport = std::make_unique<sim::SimulatorTransport>();
transport->simulator().signal().phases[0].voltage_rms = 230.0;
transport->open();
auto* link = transport.get();
NormaInstrument norma(std::move(transport));

norma.set_functions({fn::voltage(1)});
norma.data();

link->fail_reads = true;    // -> ConnectionError
link->stall_reads = true;   // -> TimeoutError
link->fail_writes = true;   // -> ConnectionError on send
link->writes;               // every command line, for assertions
link->pending_responses();  // non-zero means the protocol desynchronized
```

`read_line()` with nothing buffered raises `TimeoutError`, which is what a real
transport does when you query a command that produces no response — so that
mistake fails loudly here too.

### Over a real socket — this is what exercises `TcpTransport`

```cpp
#include <fluke/norma/simulator/simulator_server.hpp>

sim::SimulatorServer server;                       // 127.0.0.1, port chosen by the OS
auto norma = NormaInstrument::connect(server.host(), server.port());

server.disconnect_client();                        // drop the link mid-session
server.set_stalled(true);                          // parse but never answer
server.connections();                              // clients accepted so far
server.with_simulator([](sim::NormaSimulator& s) { // the server owns a thread, so
    s.signal().present = false;                    // reach in under its lock
});
```

One client at a time, like the instrument.

### As a process

```console
$ norma_sim --port 2300 --verbose
listening on 127.0.0.1:2300
identifies as Fluke,NORMA5000,KN34512BA,01.05
```

`--port 0` (the default) lets the OS pick and prints the choice on the first line
of stdout, so a test harness can start it and read the port. `--host 0.0.0.0`
makes it reachable from another machine. Then point anything at it:

```console
$ printf '*IDN?\nFUNC "VOLT1","CURR1","POW1:ACT"\nDATA?\nSYSTEM:NOPE\nSYST:ERR?\n' | nc 127.0.0.1 2300
Fluke,NORMA5000,KN34512BA,01.05
+2.30000E+02,+1.00000E+00,+2.30000E+02
-113,"Undefined header;SYSTEM:NOPE"
```

The examples take a host and a port, so they work against it unchanged:

```bash
norma_identify 127.0.0.1 2300
norma_simple_power 127.0.0.1 2300
python module/examples/python/simple_power_measurement.py 127.0.0.1 2300
```

## Adding a command

Two edits, in `src/`:

1. A row in `kCommandSpecs` (`norma_simulator.cpp`) mapping a stable key to the
   header pattern. `#` marks a node that carries a numeric suffix, `[...]` an
   optional node, `[A|B]` an optional node that may be either:

   ```cpp
   {"VOLT#:RANG:AUTO", "[SENSe]:VOLTage#:[AC|DC]:RANGe:[UPPer]:AUTO"},
   ```

   Order does not matter — a pattern only matches when it consumes the whole
   header — but a longer pattern must exist for a longer header, or it falls
   through to `-113`.

2. A branch in the subsystem handler keyed on that string. `Access` does the
   read/write plumbing, so most commands are one line:

   ```cpp
   } else if (key == "VOLT#:RANG:AUTO") {
       access.boolean(config.autorange);
   ```

   `access.number()`, `integer()`, `boolean()`, `keyword()`, `function_list()`,
   `query_only()` and `command_only()` each handle both directions and queue the
   right SCPI error for a bad parameter.

Then add it to the conformance table and the "accepts every command" sweep in
[`../tests`](../tests/README.md), so both sides move together.

## Files

| File | |
|---|---|
| `include/.../norma_simulator.hpp` | `SimulatorState`, `SimulatedSignal`, `SimulatorOptions`, the error codes |
| `src/norma_simulator.cpp` | the command-line parser, the pattern matcher and the command table |
| `src/simulator_handlers.cpp` | IEEE 488.2, ROUTe, INPut, SENSe, SYNC, INITiate, TRIGger, FORMat, DISPlay, SYSTem, TIMer |
| `src/simulator_calculate.cpp` | CALCulate, TRACe, STATus |
| `src/simulator_measurement.cpp` | the signal model, the status derivation, the register conditions |
| `src/simulator_support.cpp` | string helpers and the `Access` plumbing |
| `src/simulator_server.cpp` | the loopback TCP server |
| `src/main.cpp` | `norma_sim` |

It is tested in its own right — see `tests/test_simulator.cpp`. A simulator bug
would otherwise read as a library bug.

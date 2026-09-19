# Fluke NORMA 4000/5000 – TCP/Socket API (Remote Control)

This document describes the remote control API for the **Fluke NORMA 4000 and NORMA 5000** power analyzers. The instrument is controlled remotely with **SCPI 1999.0** commands (Standard Commands for Programmable Instruments) sent as lines of text over **TCP/IP** via the Ethernet interface, where the TCP port is **fixed at 23**. Exactly the same command set applies to the other interfaces as well — RS-232, USB (virtual COM port) and GPIB (IEC/IEEE bus). If you change the transport layer, the commands remain identical.

The document is assembled so that it can be used directly as a reference while developing your own application (a TCP client) that talks to the instrument.

**Source:** Fluke NORMA 4000/5000 Remote Control Users Guide, June 2007 Rev. 2, 5/12.

> ## Quick start
>
> 1. Connect to the instrument's IP address on **TCP port 23** with an ordinary TCP stream socket (or test with `telnet <ip> 23`). IP address, subnet mask and gateway are configured in the instrument's **General Setup** screen.
> 2. Send SCPI commands as plain ASCII text, one command line at a time, terminated with a line feed `<LF>` (`\n`, 0Ah).
> 3. For queries (commands ending in `?`): read the response back as a single line of text terminated with `\n` (strip any preceding `\r`).
> 4. Test the connection with `*IDN?` — the instrument replies with, for example, `Fluke,NORMA4000,KN34512BA,01.00`.
> 5. Typical measurement setup: `*RST` → configure (`ROUT:SYST`, `SYNC:SOUR`, ranges, `APER`, `FUNC`) → `INIT:CONT ON` → fetch values with `DATA?`.
> 6. Complete, reusable client examples (including a raw TCP socket without VISA) are found in the [Programming examples](#programming-examples) chapter.

## Contents

1. [Connection and interfaces](#connection-and-interfaces)
2. [Protocol and SCPI syntax](#protocol-and-scpi-syntax)
3. [Common commands and measurement functions](#common-commands-and-measurement-functions)
4. [Subsystems: ABORt through ROUTe](#subsystems-abort-through-route)
5. [Subsystems: SENSe, SENSe2 and SOURce](#subsystems-sense-sense2-and-source)
6. [Subsystems: SYNC through STATus](#subsystems-sync-through-status)
7. [Quick reference: all commands](#quick-reference-all-commands)
8. [The status reporting system](#the-status-reporting-system)
9. [Error messages](#error-messages)
10. [Programming examples](#programming-examples)

---

## Connection and interfaces

This chapter describes the hardware interfaces for remote control of the Fluke NORMA 4000/5000 Power Analyzer. By default the instrument is equipped with an RS-232 interface. As an option, the instrument can also be equipped with an IEC/IEEE bus interface (GPIB), IEEE 802.3 (Ethernet) and USB. **The same SCPI command set applies regardless of which interface is used** — for a TCP/Socket application, the Ethernet interface is the relevant one.

### IEEE 802.3 (Ethernet) – option (main interface for TCP/Socket)

The instrument can optionally be equipped with an IEEE 802.3 (Ethernet) interface. The connector for the Ethernet interface (RJ-45) is located on the rear of the instrument. A controller (PC) for remote control can be connected through the interface. The connection is made with twisted pair cable.

#### Interface characteristics (Ethernet)

- Bidirectional TCP/IP data transmission
- 10/100 Mbps operation
- Half/full duplex
- High data transfer rate: max. 240 kB/s (measurement data), 1.3 MB/s (raw data)

#### Signal lines (RJ-45)

| Pin | Signal | Description |
|-----|--------|-------------|
| 1 | TD+ (Transmit Data Plus) | Positive signal in the TD differential pair; carries the serial outgoing data stream the instrument sends onto the network. |
| 2 | TD− (Transmit Data Minus) | Negative signal in the TD differential pair; carries the same output data as pin 1 (TD+). |
| 3 | RD+ (Receive Data Plus) | Positive signal in the RD differential pair; carries the serial incoming data stream the instrument receives from the network. |
| 6 | RD− (Receive Data Minus) | Negative signal in the RD differential pair; carries the same input data as pin 3 (RD+). |

#### Cabling between instrument and controller

Cabling between the instrument and the controller is done with twisted pair cable with RJ-45 plugs. Two connection methods are supported:

- **Via a local network (hub/switch):** an ordinary straight-through ("patch") cable between the instrument and the hub/switch, and between the hub/switch and the controller (NIC).
- **Direct connection:** the controller is connected directly to the instrument with a crossover cable, where the TD+/RD+ pairs are crossed (pin 1↔3, 2↔6).

#### Connection settings (TCP/IP)

To control the instrument over the Ethernet interface, a TCP/IP connection must first be established with these settings:

| Setting | Description |
|---------|-------------|
| IP address | The instrument's Internet Protocol address (for example `192.168.1.100`). |
| TCP port number | Transmission Control Protocol port number. This is currently fixed at **23** (the port assigned to the "telnet" service). |
| IP subnet address mask | Internet Protocol subnet mask (for example `255.255.255.0`). |
| IP gateway address | The Internet Protocol address of the gateway (for example `192.168.1.1`). |

On the instrument side, these settings are configured in the **General Setup** screen. When the connection is established, the controller must use the instrument's IP address and TCP port as the destination address.

Example connection (any TCP socket/telnet client can be used):

```
telnet 192.168.1.100 23
```

### IEC/IEEE bus interface (GPIB) – option

The instrument can optionally be equipped with an IEC/IEEE bus interface. The IEEE 488 connector is located on the rear of the instrument. A controller for remote control is connected through the interface with a shielded cable.

#### Interface characteristics (GPIB)

- 8-bit parallel data transmission
- Bidirectional data transmission
- Three-wire handshake
- High data transfer rate: max. 115 kB/s (measurement data), 1.2 MB/s (raw data)
- Up to 15 devices can be connected
- Maximum connection cable length 15 m (single connection 2 m)
- "Wired OR" if several instruments are connected in parallel

#### Bus lines

**Data bus with 8 lines, DIO 1 to DIO 8**
Transmission is bit-parallel and byte-serial in ASCII/ISO code. DIO1 is the least significant bit, DIO8 is the most significant.

**Control bus with 5 lines**

- `IFC` (Interface Clear): Active LOW resets the interfaces of the connected instruments to the default setting.
- `ATN` (Attention): Active LOW signals the transfer of interface messages. Inactive HIGH signals the transfer of device messages.
- `SRQ` (Service Request): Active LOW lets the instrument send a service request to the controller.
- `REN` (Remote Enable): Active LOW enables switching to remote control.
- `EOI` (End or Identify): Has two functions in combination with ATN:
  - ATN = HIGH: Active LOW marks the end of a data transfer.
  - ATN = LOW: Active LOW triggers a parallel poll.

**Handshake bus with 3 lines**

- `DAV` (Data Valid): Active LOW signals a valid data byte on the data bus.
- `NRFD` (Not Ready For Data): Active LOW signals that one of the connected devices is not ready to receive data.
- `NDAC` (Not Data Accepted): Active LOW for as long as the instrument is accepting data present on the data bus.

#### Interface functions

Instruments that can be remotely controlled via the IEC/IEEE bus may be equipped with various interface functions. Table 3-1 shows the interface functions relevant to the instrument.

**Table 3-1. Interface functions**

| Control character | Interface function |
|-------------------|--------------------|
| SH1 | Source Handshake function. |
| AH1 | Acceptor Handshake function. |
| L4 | Listener function. |
| T6 | Talker function, with the ability to respond to a serial poll. |
| SR1 | Service Request function. |
| PP1 | Parallel poll function *(not implemented)* |
| RL1 | Remote/local switching function *(not implemented)* |
| DC1 | Device Clear function *(not implemented)* |
| DT1 | Device Trigger function *(not implemented)* |

#### Interface messages

Interface messages are transferred to the instrument on the data lines while the ATN (Attention) line is active LOW. These messages are used for communication between the controller and the instrument.

##### Universal commands

Universal commands (see Table 3-2) lie in the code range 10 to 1F hex. They act on all instruments connected to the bus, without being addressed first.

**Table 3-2. Universal commands**

| Command | QuickBASIC command | Effect on the instrument |
|---------|--------------------|--------------------------|
| DCL (Device Clear) *(not implemented)* | `IBCMD (controller%, CHR$(20))` | Aborts processing of the commands just received and sets the command-processing software to a defined initial state. Does not change the instrument setting. |
| IFC (Interface Clear) | `IBSIC (controller%)` | Resets the interfaces to the default state. |
| LLO (Local Lockout) *(not implemented)* | `IBCMD (controller%, CHR$(17))` | Manual switching to LOCAL is disabled. |
| SPE (Serial Poll Enable) | `IBCMD (controller%, CHR$(24))` | Ready for serial polling. |
| SPD (Serial Poll Disable) | `IBCMD (controller%, CHR$(25))` | End of serial polling. |
| PPU (Parallel Poll Unconfigure) | `IBCMD (controller%, CHR$(21))` | End of parallel poll state. |

##### Addressed commands

Addressed commands lie in the code range 00 to 0F hex. They act only on instruments addressed as "listener".

**Table 3-3. Addressed commands**

| Command | QuickBASIC command | Effect on the instrument |
|---------|--------------------|--------------------------|
| SDC (Selected Device Clear) *(not implemented)* | `IBCLR (device%)` | Aborts processing of the commands just received and sets the command-processing software to a defined initial state. Does not change the instrument setting. |
| GET (Group Execute Trigger) *(not implemented)* | `IBTRG (device%)` | Triggers a previously active instrument function (for example a sweep). The effect of the command is identical to a pulse on the external trigger signal input. |
| GTL (Go to Local) *(not implemented)* | `IBLOC (device%)` | Transition to LOCAL state (manual operation). |
| PPC (Parallel Poll Configure) *(not implemented)* | `IBPPC (device%, data%)` | Configures the instrument for parallel polling. The QuickBASIC command additionally performs PPE / PPD. |

### RS-232-C interface (standard)

The instrument is equipped with an RS-232-C interface as standard. The 9-pin connector is located on the rear of the unit. A controller for remote control can be connected through the interface.

#### Interface characteristics (RS-232)

- Serial data transmission in asynchronous mode
- Bidirectional data transmission via two separate lines
- Selectable transmission rate from 1200 to 115200 baud
- Logical 0 signal level from +3 V to +15 V
- Logical 1 signal level from −15 V to −3 V
- One external device (controller) can be connected
- Software handshake (XON, XOFF)
- Hardware handshake

#### Signal lines (9-pin connector)

| Pin | Signal | Description |
|-----|--------|-------------|
| 2 | TxD (Transmit Data) | Data line; transmission from instrument to external controller (DTE). |
| 3 | RxD (Receive Data) | Data line; transmission from external controller to instrument. |
| 4 | DSR (Data Set Ready) | Not used. |
| 5 | GND (Ground) | Interface ground, connected to instrument ground. |
| 6 | DTR (Data Terminal Ready) | Not used. |
| 7 | CTS (Clear To Send) | Input from the DTE/controller. The unit stops sending data to the DTE/controller when it detects that the CTS line goes low. |
| 8 | RTS (Request To Send) | Output to the DTE/controller. The unit sets the RTS line low (logical 0) when it cannot accept any more data from the DTE/controller. |
| 9 | RI | – |

#### Transmission parameters

To ensure error-free and correct data transfer, the transmission parameters on the instrument and the controller must have the same settings. The settings are made in the **General Setup** screen on the instrument.

| Parameter | Description |
|-----------|-------------|
| Transmission rate (baud rate) | Eight different baud rates can be set on the instrument: 1200, 2400, 4800, 9600, 19200, 38400, 57600, 115200. |
| Data bits | Data transfer takes place in 8-bit or 7-bit ASCII code. The LSB (least significant bit) is transmitted as the first bit. |
| Start bit | Transmission of a data byte is introduced by a start bit. The falling edge of the start bit marks the beginning of the data byte. |
| Parity bit | Odd, Even, Zero, One, None |
| Stop bit | Transmission of a data byte is terminated by a stop bit. |

Bit order: bit 01 = start bit, bits 02 to 09 = data bits, bit 10 = stop bit. Bit duration = 1/baud rate.

Example from the manual: transmission of the character `A` (41 hex) in 8-bit ASCII code.

#### Interface functions (control characters)

For interface control, a number of control characters defined from 0 to 20 hex in the ASCII code can be transferred via the interface.

**Table 3-4. Control characters for the RS-232-C interface**

| Control character | Interface function |
|-------------------|--------------------|
| `<Ctrl Q>` 11 hex | Enables character output (XON). |
| `<Ctrl S>` 13 hex | Stops character output (XOFF). |
| Break (at least 1 character of logical 0) | Clears the instrument's input buffer. All pending queries are aborted. Equivalent to IFC on the GPIB interface. |
| 0A hex | Terminator `<LF>`. The instrument enters the remote state on receiving this character together with a valid command. |

#### Handshake

**Software handshake**
Software handshake with the XON/XOFF protocol controls the data transfer. If the receiver (the instrument) wants to stop incoming data, it sends XOFF to the transmitter. The transmitter then interrupts data output until it receives XON from the receiver. The same function also exists on the transmitter side (the controller).

> **Note:** Software handshake is not suitable for transferring binary data — hardware handshake is preferred.

**Hardware handshake**
With hardware handshake, the instrument signals readiness to receive via the DTR and RTS lines. Logical 0 means "ready", logical 1 means "not ready". Whether the controller is ready to receive is signalled to the instrument via the CTS or DSR line (see "Signal lines"). The instrument's transmitter is switched on with logical 0 and off with logical 1. The RTS line stays active for as long as the serial interface is active. The DTR line controls the instrument's readiness to receive.

**Cabling between instrument and controller**
The cabling between the instrument and the controller is an extension cable (with a 9-pin controller connector), that is, the data, control and signal lines are wired straight through. The wiring scheme applies to controllers with a 9-pin or 25-pin connector: the signal pairs DCD/DCD, TxD/RxD, RxD/TxD, DSR/DTR, GND/GND, DTR/DSR, CTS/RTS, RTS/CTS and RI/RI are connected between the instrument's 9-pin female connector and the controller.

### Universal Serial Bus (USB) – option

The instrument can optionally be equipped with a USB interface. The connector for the USB interface (USB series "B" female connector) is located on the rear of the instrument. A controller for remote control can be connected through the interface. The connection is made with a USB A-to-USB B cable (also called a USB A/B cable, or series "A" plug to series "B" plug cable).

#### Interface characteristics (USB)

- Bidirectional USB data transmission
- Compatible with USB 1.1 and USB 2.0
- Data transfer rate 800 kB/s (raw data)

#### Pin assignment (series B female connector)

| Pin | Signal |
|-----|--------|
| 1 | VBUS (power) |
| 2 | D− (data minus) |
| 3 | D+ (data plus) |
| 4 | GND (ground) |

#### Connection settings (VCP)

To control the instrument over the USB interface, a serial port connection must first be established using the VCP name (Virtual COM Port, for example `COM3`) associated with the instrument's USB interface. The VCP name is found in Windows Device Manager under "Ports (COM & LPT)". A port named "NORMA Power Analyzer USB Serial Port" appears there when the instrument is switched on and connected to a PC with a USB cable.

On the instrument side, the **General Setup** screen is used to select the USB interface. No other settings are needed on the instrument side.

## Protocol and SCPI syntax

This chapter provides basic information about remote control of the instrument: interface and device messages, command processing, the status reporting system and so on. The instrument is equipped with an RS-232-C interface and optionally with an IEC/IEEE bus interface conforming to the IEC 625.1/IEEE 488.1 standard. The connectors are on the rear of the instrument and let you connect a controller for remote control.

The instrument supports **SCPI version 1999.0** (Standard Commands for Programmable Instruments). The SCPI standard is based on the IEEE 488.2 standard and aims to standardize device-specific commands, error handling and the status registers.

It is assumed that the user has basic knowledge of IEC/IEEE bus programming and of operating the controller. The IEC bus programming examples in the manual are all written with the VISA C API.

### Getting started

A short operating sequence that quickly demonstrates the instrument's basic functions.

**Prerequisites:**

- The instrument is connected to port COM1 on the controlling computer. Factory settings: Baud Rate = 115200, Data Bits = 8, Stop Bits = 1, Parity = None, Handshake = RTS/CTS.
- The HyperTerminal program is used to communicate with the instrument.

**Procedure:**

1. Connect the instrument and the controlling computer together.
2. Start HyperTerminal on the computer (Start > Programs > Accessories > Communication > HyperTerminal). HyperTerminal is a standard part of Windows.
3. If HyperTerminal has never been used/configured before:
   - In the *Connection Description* window: enter the name `Fluke` and press OK.
   - In the *Connect To* window: select `COM1` under *Connect using* (or another port if you are using one) and press OK.
   - In the *COM1 Properties* window: set the correct properties and press OK:
     - Bits per second = 115200
     - Data bits = 8
     - Parity = None
     - Stop bits = 1
     - Flow control = None
4. Go to File > Properties > Settings > ASCII Setup, tick the following, and press OK twice:
   - Send line ends with line feeds
   - Echo typed characters locally
   - Append line feeds to incoming line ends
5. Type `*IDN?` in the main (white) window and press Enter. (Do not make typing mistakes — every character is sent to the instrument immediately as you press a key. Backspace does not delete mistyped characters. If you do make a mistake, press Enter several times; that puts things right again.)
6. The instrument returns the identification string, for example:

   ```
   Fluke,NORMA4000,KN34512BA,01.00
   ```

7. Type `DATA? "POW"` in the main window and press Enter. This asks the instrument to return the last valid power measurement.
8. The instrument returns the last valid power measurement, for example:

   ```
   +1.23456E+02
   ```

### Switching to remote control

At power-up the instrument is always in manual operating mode ("LOCAL" state) and can be operated from the front panel.

The instrument is switched to remote control ("REMOTE" state) as follows:

- **IEC/IEEE bus:** when it receives an addressed command from the controller with the REN line asserted.
- **Other interfaces:** when it receives a valid command terminated with a line feed `<LF>` (= 0Ah) from the controller in the `SYSTem:KLOCk REM` state, or explicitly via that command.

During remote control, operation from the front panel is disabled. The instrument stays in the remote control state until it is returned to the manual state from the front panel or via remote control. Switching from manual to remote control and back does not affect the instrument settings.

#### Indications during remote control

The remote control state is shown by a two-way radio icon in the leftmost cell of the status bar on the instrument display. A key icon in the third cell of the status bar indicates that the [LOCAL] key (F6/Esc) is disabled and that switching to manual operation can only be done via remote control. If the key icon is not shown, switching to manual operation can be done with the [LOCAL] key (F6/Esc).

### Returning to manual operation

Returning to manual operation can be done from the front panel or over the IEC/IEEE bus.

**Manually:** Press the [LOCAL] key.

Note:

- Command processing must be complete before switching, otherwise the instrument switches back to remote control immediately.
- The [LOCAL] key can be disabled with the command `SYSTem:KLOCk ON` or the universal command LLO (GPIB only) to prevent accidental switching. Switching to manual operation is then only possible via remote control.
- The [LOCAL] key can be enabled again with the command `SYSTem:KLOCk OFF` or by deasserting the REN control line (GPIB only).

**Remotely:**

- GTL interface message (GPIB only)
- With the command `SYSTem:KLOCk OFF`

### Commands and instrument responses

Instrument commands are transferred via the selected interface. With the exception of certain device responses (binary data), ASCII code is used. On the IEC/IEEE bus (GPIB), commands and instrument responses are referred to as device messages. Commands and responses are essentially identical for all interface types. A distinction is made according to the direction in which device messages are sent on the interface.

**Commands** are messages the controller sends to the instrument. They operate the device functions and request information. Commands are divided according to two criteria:

1. By the effect they have on the instrument:
   - *Setting commands* cause instrument settings, for example resetting the instrument or setting the output level to 1 V.
   - *Queries* cause data to be made available for reading on the interface, for example device identification or reading the active input.
2. By their definition in the IEEE 488.2 standard:
   - *Common commands* are exactly defined with respect to function and notation in IEEE 488.2. They cover functions such as handling of the standardized status registers, reset and self-test.
   - *Device-specific commands* cover functions that depend on the instrument's characteristics, for example frequency setting. The majority of these commands are also standardized by the SCPI committee.

**Device responses** are messages the instrument sends to the controller in reply to a query. They may contain measurement results or information about the instrument's status.

### Structure and syntax of device messages

#### Introduction to SCPI

SCPI (Standard Commands for Programmable Instruments) describes a standard command set for programming instruments, independently of instrument type or manufacturer. The goal of the SCPI consortium is to standardize the device-specific commands as far as possible. A model has been developed that defines identical functions within one device or across different devices, and command systems are designed so that identical functions can be addressed with identical commands. The command systems have a hierarchical (tree) structure.

SCPI is based on the IEEE 488.2 standard and uses the same basic syntax elements and common commands defined there. Parts of the syntax for device responses are defined in more detail than in IEEE 488.2 (see the section "Responses to queries").

#### Command structure

Commands consist of a *header* and, in most cases, one or more *parameters*. Header and parameters are separated by a "white space" (ASCII code 0 to 9, 11 to 32 decimal, blank). Headers may consist of several keywords. Queries are formed by appending a question mark directly after the header.

#### Common commands

Common commands (device-independent) consist of a header introduced by an asterisk `*`, and optionally one or more parameters.

Examples:

```
*RST      RESET, resets the instrument
*ESE 253  EVENT STATUS ENABLE, sets the bits in the event status enable register
*ESR?     EVENT STATUS QUERY, reads the contents of the event status register
```

#### Device-specific commands

##### Hierarchy

Device-specific commands have a hierarchical structure. The various levels are represented by compound headers. Headers at the highest level (the root level) have only one keyword, which designates an entire command system.

Example:

```
:SYSTem
```

This keyword designates the `:SYSTem` command system. For commands at lower levels the full path must be given, from left to right with the highest level first, and the individual keywords separated by a colon `:`.

Example:

```
INPut:COUPling AC
```

This command is at the second level of the `INPut` subsystem and selects AC coupling for the input channel.

The tree structure of the `INPut` system (figure 1-1 in the manual) looks like this:

```
INPut
├── COUPling
├── GAIN
├── SHUNt
└── FILTer
    ├── STATe
    └── LPASs
        ├── STATe
        └── FREQuency
```

##### Optional keyword

Some command systems allow certain keywords to be inserted into the header or omitted. These keywords are marked with square brackets in the description. The instrument must recognize the full command length for compatibility with the SCPI standard. Some commands can be shortened considerably by omitting optional keywords.

Example:

```
INPut:FILTer[:STATe] ON
```

This command enables the anti-aliasing filter inserted into the signal path before the signal is processed by the `SENSe` subsystem. The following command has the same effect:

```
INPut:FILTer ON
```

> **Note:** An optional keyword must not be omitted if its effect is further specified by a numeric suffix.

##### Long and short form

Example:

```
STATus:QUEStionable:ENABle 1
STAT:QUES:ENAB 1
```

> **Note:** The short form is indicated by upper-case letters; the long form corresponds to the complete word. Upper and lower case serve only this purpose in the documentation — the instrument itself does not distinguish between upper and lower case.

##### Parameters

A parameter must be separated from the header by a "white space". If a command has several parameters, they are separated by commas `,`.

Example:

```
FORMat:READings:DATA REAL,32
```

This command selects binary 32-bit floating point format for data transfers.

##### Numeric suffix

If a device has several functions or features of the same kind, for example inputs, the desired one can be selected by appending a suffix to the command. Entries without a suffix are interpreted as suffix 1, unless explicitly stated otherwise.

Example:

```
INPut:COUPling DC
```

This command sets the input coupling on channel 1 to DC.

Measurement functions (parameters to the `SENSe:FUNCtion` command) use a numeric suffix to select the phase. If no suffix is given, the total value is configured.

#### Structure of command lines

A command line may contain one or more commands.

**Message termination — a command line is terminated by one of the following:**

- `<New Line>` (line feed, `<LF>` = 0Ah)
- `<New Line>` together with EOI
- EOI together with the last data byte (EOI applies to the GPIB interface only)

VISA automatically produces EOI together with the last data byte.

Several commands in a single command line are separated by a semicolon `;`. If the next command belongs to a different command system, the semicolon is followed by a colon.

Example:

```
INPut1:COUPling DC;:SENSe:CURRent1:DC:RANGe 1.0
```

This command line contains two commands. The first belongs to the `INPut` subsystem and sets the input coupling for channel 1. The second belongs to the `SENSe` subsystem and sets the current range on phase 1 to 1.0 A. (`DC` could have been omitted since it is an optional keyword. For this instrument there is no difference between AC and DC range — both set the same range.)

If consecutive commands belong to the same system and share one or more levels, the command line can be shortened. The second command (after the semicolon) then starts at the level below the shared levels. The colon after the semicolon must then be omitted.

Example (full length — two commands in the `INPut` subsystem with one shared level):

```
INPut1:SHUNt EXTernal;:INPut1:GAIN 25.0
```

Shortened form of the command line:

```
INPut1:SHUNt EXTernal;GAIN 25.0
```

A new command line must, however, always start with the complete path:

```
INPut1:SHUNt EXTernal
INPut1:GAIN 25.0
```

#### Responses to queries

For each setting command a query is defined, unless explicitly stated otherwise. The query is formed by appending a question mark to the corresponding setting command. Responses to queries under the SCPI standard are in part subject to stricter rules than responses under IEEE 488.2:

1. **The requested parameter is transferred without a header.**

   ```
   INPut:COUPling?
   Response: AC
   ```

2. **Numeric values are returned without a unit.** Physical quantities refer to the base units or to the units set with the Unit command.

   ```
   INPut:FILTer:LPASs:FREQuency?
   Response: 3.0E5 for 300 kHz
   ```

3. **Truth values (boolean parameters) are returned as 0 (Off) and 1 (On).**

   ```
   INPut:FILTer:STATe?
   Response: 1
   ```

4. **Text (character data) is returned in short form.**

   ```
   INPut:SHUNt?
   Response: EXT
   ```

5. **With several queries in the same command line, the responses are returned in the same order as the queries, separated by semicolons.**

   ```
   INPut:FILTer:STATe?;:INPut:FILTer:LPASs:FREQuency?
   Response: 1;1.0E+04
   ```

#### Parameter types

Most commands require a parameter to be given. Parameters must be separated from the header by a "white space". Permitted parameters are numeric values, boolean parameters, text, character strings and block data. The parameter type and permitted value range for a given command are stated in the command description.

##### Numeric values

Numeric values may be entered in any form: sign, decimal point and exponent. Values exceeding the instrument's resolution are rounded up or down. The mantissa may consist of up to 15 characters, and the exponent must lie in the range -307 to 307. The exponent is introduced by `E` or `e`. Entering the exponent alone is not permitted. For physical quantities with a unit, no unit is accepted — the base unit is used.

```
SENSe:VOLTage1:RANGe 1000.0    sets range 1000 V
```

##### Boolean parameters

Boolean parameters represent two states. The ON state (logically true) is represented by `ON` or a numeric value other than 0. The OFF state (logically false) is represented by `OFF` or the numeric value 0. On query, 0 or 1 is returned.

```
Setting command: SYNC:STATe ON
Query:           SYNC:STATe?
Response:        1
```

##### Text

Text parameters follow the syntactic rules for keywords. They can be given in short or long form. Like all other parameters they must be separated from the header by a "white space". On query, the short form of the text is returned.

```
Setting command: INPut1:SHUNt EXTernal
Query:           INPut1:SHUNt?
Response:        EXT
```

##### Strings

Strings must always be given in quotation marks (`'` or `"`).

```
ROUTe:SYSTem "3W"
ROUTe:SYSTem '3W'
```

##### Block data

Block data is a transfer format suited to transferring large amounts of data from the instrument to the controller. Block data has the following structure:

```
#40008xxxxxxxx
```

The data block is introduced by the ASCII character `#`. The next digit states how many of the following digits describe the length of the data block. In the example, the four following digits state that the length is 8 bytes (leading zeros are ignored). The data bytes then follow. During the transfer of the data bytes, all End and other control characters are ignored until all bytes have been transferred. Data elements consisting of more than one byte are transferred with the byte specified by the SCPI command `FORMat:BORDer` first. The internal structure of the data in the block depends on the actual instrument configuration.

#### Overview of syntax elements

| Element | Meaning |
|---------|---------|
| `:` | The colon separates the keywords in a command. In a command line, after the separating semicolon it marks the topmost command level. |
| `;` | The semicolon separates two commands in a command line. It does not change the path. |
| `,` | The comma separates several parameters in a command. |
| `?` | The question mark forms a query. |
| `*` | The asterisk marks a common command. |
| `"` | Quotation marks introduce and terminate a string. |
| `#` | The ASCII character `#` introduces block data. |
| white space | A "white space" (ASCII code 0 to 9, 11 to 32 decimal, blank) separates header and parameter. |

### Instrument model and command processing

Processing of the interface commands takes place in several components that work independently of each other and concurrently, and that communicate with each other via messages (figure 1-2 in the manual):

```
Interface ──> Input unit (with input buffer) ──> Command recognition ──> Data set ──> Instrument hardware
                                                        │                   │
                                                        v                   v
Interface <── Output unit (with output buffer) <── Status reporting system
```

#### Input unit

The input unit receives commands, character by character, from the interface and stores them in the input buffer. **The input buffer has a size of 2048 characters.** The input unit sends a message to the command recognition when the input buffer is full, or when it receives a terminator, `<PROGRAM MESSAGE TERMINATOR>` as defined in IEEE 488.2, or the interface message DCL (GPIB only).

If the input buffer is full, interface traffic is stopped and the data received so far is processed; traffic then continues. If the buffer is not full when a terminator is received, the input unit can receive the next command while command recognition and execution are in progress. Receiving DCL (GPIB only) clears the input buffer and immediately sends a message to the command recognition.

#### Command recognition

The command recognition analyses the data from the input unit in the order received. Only DCL commands (GPIB only) are processed with priority; GET commands (Group Execute Trigger, GPIB only) are processed only after previously received commands. Each recognized command is transferred immediately to the data set, but without being executed there right away.

Syntax errors in commands are detected here and passed to the status reporting system. The remainder of a command line after a syntax error is analysed and processed further as far as possible.

When the command recognition recognizes a terminator or a DCL command (GPIB only), it asks the data set to set the commands in the instrument hardware as well. It is then immediately ready to continue command processing. This means new commands can be processed while the hardware is being set ("overlapped execution"). **Note: For this instrument, all commands are currently executed non-overlapped (= sequentially).**

#### Data set and instrument hardware

The term "instrument hardware" denotes the part of the instrument that actually performs the instrument functions: signal generation, measurement and so on. The controller is not included.

The data set is a detailed representation of the instrument hardware in software. Setting commands from the interface cause the data set to change. The data set management enters the new values (for example frequency) into the data set, but only passes them on to the hardware when requested by the command recognition. Since this only happens at the end of a command line, the order of the setting commands within the command line is irrelevant.

The data is checked for compatibility with each other and with the instrument hardware only immediately before it is transferred to the hardware. If it turns out that execution is not possible, an "execution error" is signalled to the status reporting system. All changes to the data set are discarded and the instrument hardware is not reset. Because of the deferred check and hardware setting, it is permitted for invalid instrument states to be set briefly within a command line without an error message being issued — but at the end of the command line a valid instrument state must have been reached.

Before the data is passed on to the hardware, the settling bit in the `STATus:OPERation` register is set. The hardware makes the settings and clears the bit once the new state has settled. This mechanism can be used for synchronizing command processing.

#### Status reporting system

The status reporting system collects information about the instrument state and makes it available to the output unit on request. A detailed description of its structure and function is given in chapter 2 of the manual.

#### Output unit

The output unit collects the information requested by the controller and delivered by the data set management. It processes the information according to the SCPI rules and makes it available in the output buffer. **The output buffer has a size of 2048 characters.** If the requested information exceeds this size, it is made available in portions without the controller noticing.

If the instrument is addressed as a talker without the output buffer containing data or waiting for data from the data set management, the output unit returns the error message "Query UNTERMINATED" to the status reporting system. No data is sent on the interface, and the controller waits until its timeout is reached. This behaviour is specified by SCPI.

Interface queries cause the data set management to send the requested data to the output unit.

### Command sequence and command synchronization

As mentioned above, overlapped execution is possible for all commands. Likewise, the setting commands in a command line are not necessarily processed in the order received. To ensure commands are executed in a particular order, each command must be sent in its own command line with its own viWrite call (viPrintf, viQueryf).

To prevent overlapped execution of commands, one of the commands `*OPC`, `*OPC?` or `*WAI` must be used. Each of the three commands triggers a particular action only after the hardware has been set and has settled. The controller can be programmed to wait for the respective action (see table 1-1).

**Table 1-1. Synchronization with *OPC, *OPC? and *WAI**

| Command | Action after the hardware has settled | Controller programming |
|---------|---------------------------------------|------------------------|
| `*OPC` | Sets the operation-complete bit in the ESR | - Set bit 0 in the ESE<br>- Set bit 5 in the SRE<br>- Wait for service request (SRQ) |
| `*OPC?` | Writes a "1" into the output buffer | Address the instrument as a talker |
| `*WAI` | Continues the IEC/IEEE bus handshake. The handshake is not stopped. | Send the next command |

An example of command synchronization is found in chapter 6 of the manual.

> **Note:** The command synchronization commands work, but are currently not necessary, since the instrument executes all commands sequentially.

## Common commands and measurement functions

This chapter describes all commands implemented in the instrument. The commands are first listed in tables and then described in detail, organized by command subsystem. The notation follows the SCPI standard, and SCPI conformance information is included in each command description.

**All commands can be used for control via every interface** (including the TCP/socket interface).

### Common commands

The common commands are taken from the IEEE 488.2 (IEC 625-2) standard. A given command has the same effect on different devices. The header of these commands consists of an asterisk `*` followed by three letters. Many common commands refer to the status reporting system described in chapter 2.

#### Overview

| Command | Parameter | Function | Comment |
|---|---|---|---|
| `*CLS` | – | Clear Status | no query |
| `*ESE` | 0 to 255 | Event Status Enable | |
| `*ESR?` | – | Standard Event Status Query | query only |
| `*IDN?` | – | Identification Query | query only |
| `*OPC` | – | Operation Complete | |
| `*OPC?` | – | Operation Complete Query | |
| `*OPT?` | – | Option Identification Query | query only |
| `*RST` | – | Reset | no query |
| `*SRE` | 0 to 255 | Service Request Enable | |
| `*STB?` | – | Status Byte Query | query only |
| `*WAI` | – | Wait to continue | no query |
| `*SAV` | 10 to 24 | Save User Setup | no query |
| `*RCL` | 1 to 9 / 10 to 24 | Recall Standard Setup / Recall User Setup | no query |
| `*LRN?` | – | Learn Setup String | query only |
| `*TRG` | – | Trigger | no query |

#### Detailed description

##### `*CLS`

CLEAR STATUS sets the status byte (STB), the standard event register (ESR) and the EVENt part of the QUEStionable and OPERation registers to zero. The command does not change the mask and transition parts of the registers. It clears the output buffer.

##### `*ESE 0 to 255`

EVENT STATUS ENABLE sets the event status enable register to the value given. The query form `*ESE?` returns the contents of the event status enable register in decimal form.

##### `*ESR?`

STANDARD EVENT STATUS QUERY returns the contents of the event status register in decimal form (0 to 255) and then sets the register to zero.

##### `*IDN?`

IDENTIFICATION QUERY asks for the instrument identification. The response is, for example:

```
"Fluke,NORMA4000,KN34512BA,01.00"
```

- `KN34512BA` = the instrument's serial number
- `01.00` = firmware version number

##### `*OPC`

OPERATION COMPLETE sets bit 0 in the event status register when all preceding commands have been executed. This bit can be used to trigger a service request.

##### `*OPC?`

OPERATION COMPLETE QUERY writes the message `"1"` to the output buffer as soon as all preceding commands have been executed.

##### `*OPT?`

OPTION IDENTIFICATION QUERY asks for the options included in the instrument and returns a list of installed options. The options are separated from each other by commas. The command requests identification of the device's options. Example response from the device:

```
"Option1,Option2"
```

##### `*RST`

RESET sets the instrument to a defined default state. The default setting is stated in the description of each individual command.

##### `*SRE 0 to 255`

SERVICE REQUEST ENABLE sets the service request enable register to the value given. Bit 6 (the MSS mask bit) remains 0. This command determines under which conditions a service request is generated. The query form `*SRE?` reads the contents of the service request enable register in decimal form. Bit 6 is always 0.

##### `*STB?`

READ STATUS BYTE QUERY reads out the contents of the status byte in decimal form.

##### `*TRG`

TRIGGER starts the measurement immediately if the instrument is in single-shot mode (`INITiate:CONTinuous OFF`). This command corresponds to `INITiate:IMMediate` (see the "TRIGger subsystem" section). If memory recording is configured, the ARM and TRIGger layers are bypassed and the instrument starts storing data immediately. The synchronization condition must be met if synchronization is ON.

##### `*WAI`

WAIT-to-CONTINUE allows processing of subsequent commands only after all preceding commands have been executed and all signals have settled.

##### `*SAV 10 to 24`

SAVE SETUP stores the instrument setup in the specified user configuration memory.

##### `*RCL 1 to 24`

RECALL SETUP retrieves an instrument setup from the specified configuration memory (1 to 9 = standard setups, 10 to 24 = user setups).

##### `*LRN?`

LEARN SETUP STRING asks for the complete instrument setup. The setup is returned as a sequence of semicolon-separated commands. If this sequence is sent back to the instrument, the instrument configuration is fully restored.

### Measurement functions

`<function>` is a hierarchical measurement function that states which type of averaged electrical quantity the instrument is to be configured to measure. One value of `<function>` is computed over one averaging cycle in the instrument. Using the `[SENSe:]FUNCtion` subsystem, several functions can be measured/computed simultaneously. `<function>` has the following syntax:

```
<function> ::= "<function_name>"
```

Used with the `[SENSe:]FUNCtion` subsystem, `<function_name>` is STRING PROGRAM DATA, i.e. the function names are enclosed in double quotation marks. If several functions are given, each function name must be in its own quotation marks.

Example:

```
FUNC "VOLT1:DC"                          Measure True RMS on phase 1
FUNC "VOLT1:AC", "VOLT2:AC", "VOLT3:AC"  Measure RMS on phases 1, 2 and 3
```

When a `<function>` is returned in response to a query, it contains no spaces. The mnemonics in the query response use the short form with default nodes omitted. All letters in the response are upper case.

#### Phase suffixes

The first node of the function name takes an integer numeric suffix used to distinguish between the phases in a polyphase system. Valid suffixes:

| Suffix | Phase |
|---|---|
| 1 | L1 |
| 2 | L2 |
| 3 | L3 |
| 4 | L4 on the NORMA 5000 model |
| 5 | L5 on the NORMA 5000 model |
| 6 | L6 on the NORMA 5000 model |
| 12 | phase-to-phase voltage L1 – L2 |
| 13 | phase-to-phase voltage L1 – L3 (W2 system only) |
| 23 | phase-to-phase voltage L2 – L3 |
| 31 | phase-to-phase voltage L3 – L1 (W3 system only) |
| 45 | phase-to-phase voltage L4 – L5 (N5000 model) |
| 56 | phase-to-phase voltage L5 – L6 (N5000 model) |
| 64 | phase-to-phase voltage L6 – L4 (N5000 model) |
| (no suffix) | Average/total/aggregate value from the channels in the 1st three-phase system (phase 1 … phase 3), or the 1st two-phase system (phase 1 … phase 2) when the two-wattmeter configuration (2W/Aron) is active |
| 460 | Average/total/aggregate value from the channels in the 2nd three-phase system (phase 4 … phase 6) |
| 123 | Average phase-to-phase voltage in the 1st three-phase system (phase 1 … phase 3), or the 1st two-phase system (phase 1 … phase 2) when the two-wattmeter configuration (2W/Aron) is active |
| 456 | Average phase-to-phase voltage in the 2nd three-phase system (phase 4 … phase 6) |

#### MINimum/MAXimum and IPOSitive/INEGative/INTegral

The optional **MINimum/MAXimum** part of the function name states that the extreme value of the function is to be returned. After each averaging cycle the new mean value is compared against the MIN/MAX registers, so that the extreme values accumulate over many averaging cycles until they are cleared with a dedicated command.

The MINimum/MAXimum feature is not the same as PHIGH/PLOW: PHIGH/PLOW return the highest/lowest sampled value found within the current averaging interval.

The MINimum/MAXimum feature must be enabled in the CALCulate subsystem before it can be used as part of a `<function>`.

> Note: Per the manual, the MINimum/MAXimum options are currently unimplemented.

The optional **IPOSitive/INEGative/INTegral** part of the function name states that the summed value (integral) of the function is to be returned:

- `IPOSitive` – only the positive values of the function are summed
- `INEGative` – only the negative values of the function are summed
- `INTegral` – the sum of both

The IPOSitive/INEGative/INTegral feature must be enabled in the CALCulate subsystem before it can be used as part of a `<function>`. The integrated values can be cleared with the command `CALCulate:INTegral:CLEar[:IMMediate]`.

#### Base instrument measurement functions

> Note: If the W2 system is selected, `VOLTage31` is replaced by `VOLTage13`.

| Function (quantity) | Command | No suffix (1st system) or with 460 (2nd system) |
|---|---|---|
| True RMS Voltage | `VOLTage[1..6\|460][:DC][:MINimum\|MAXimum]` | Average Voltage trms |
| RMS without DC component | `VOLTage[1..6\|460]:AC[:MINimum\|MAXimum]` | Average Voltage rms |
| True RMS phase-to-phase voltage | `VOLTage[12\|23\|31\|45\|56\|64][:DC][:MINimum\|MAXimum]` | |
| Rectified Mean phase-to-phase voltage | `VOLTage[12\|23\|31\|45\|56\|64]:RMEAN[:MINimum\|MAXimum]` | |
| Rectified Mean phase-to-phase voltage, corrected | `VOLTage[12\|23\|31\|45\|56\|64]:RMCORR[:MINimum\|MAXimum]` | |
| Phase-to-phase voltage harmonic | `VOLTage[12\|23\|31\|45\|56\|64]:HAR[:MINimum\|MAXimum]` | |
| Phase-to-phase voltage, form factor | `VOLTage[12\|23\|31\|45\|56\|64]:FFACtor[:MINimum\|MAXimum]` | |
| Phase-to-phase voltage THD | `VOLTage[12\|23\|31\|45\|56\|64]:THD[:MINimum\|MAXimum]` | |
| Phase-to-phase voltage, harmonic content | `VOLTage[12\|23\|31\|45\|56\|64]:HCONTent[:MINimum\|MAXimum]` | |
| Phase-to-phase voltage, fundamental content | `VOLTage[12\|23\|31\|45\|56\|64]:FCONTent[:MINimum\|MAXimum]` | |
| Phase-to-phase absolute voltage phase | `VOLTage[12\|23\|31\|45\|56\|64]:PHASe[:MINimum\|:MAXimum]` | |
| Average true RMS phase-to-phase voltage | `VOLTage123\|456[:DC][:MINimum\|MAXimum]` | |
| Average Mean phase-to-phase voltage | `VOLTage123\|456:MEAN[:MINimum\|MAXimum]` | |
| Average Rectified Mean phase-to-phase voltage | `VOLTage123\|456:RMEAN[:MINimum\|MAXimum]` | |
| Average Rectified Mean phase-to-phase voltage, corrected | `VOLTage123\|456:RMCORR[:MINimum\|MAXimum]` | |
| Average phase-to-phase voltage harmonic | `VOLTage123\|456:HAR[:MINimum\|MAXimum]` | |
| Mean value of voltage | `VOLTage[1..6\|460]:MEAN[:MINimum\|MAXimum\|IPOSitive\|INEGative\|INTegral]` | Average Mean Voltage |
| Rectified Mean Voltage | `VOLTage[1..6\|460]:RMEAN[:MINimum\|MAXimum]` | Average Rectified Mean Voltage |
| Rectified Mean Voltage, corrected | `VOLTage[1..6\|460]:RMCORR[:MINimum\|MAXimum]` | Average Rectified Mean Voltage Corrected |
| Peak-to-peak voltage | `VOLTage[1..6]:PTP[:MINimum\|MAXimum]` | |
| Highest value within the averaging interval | `VOLTage[1..6]:PHIGH[:MINimum\|MAXimum]` | |
| Lowest value within the averaging interval | `VOLTage[1..6]:PLOW[:MINimum\|MAXimum]` | |
| Voltage harmonic | `VOLTage[1..6\|460]:HAR[:MINimum\|MAXimum]` (order selected with `CALCulate:HARMonic:ORDer`) | Average Voltage Harm |
| Voltage, crest factor | `VOLTage[1..6]:CFACtor[:MINimum\|MAXimum]` | |
| Voltage, absolute phase | `VOLTage[1..6]:PHASe[:MINimum\|MAXimum]` (relative to the synchronization signal) | |
| Voltage, form factor | `VOLTage[1..6]:FFACtor[:MINimum\|MAXimum]` | |
| Voltage, harmonic content | `VOLTage[1..6]:HCONTent[:MINimum\|MAXimum]` | |
| Voltage, fundamental content | `VOLTage[1..6]:FCONTent[:MINimum\|MAXimum]` | |
| Voltage THD | `VOLTage[1..6]:THD[:MINimum\|MAXimum]` | |
| True RMS Current | `CURRent[1..6\|460][:DC][:MINimum\|MAXimum]` | Average Current trms |
| RMS without DC component | `CURRent[1..6\|460]:AC[:MINimum\|MAXimum]` | Average Current rms |
| Mean value of current | `CURRent[1..6\|460]:MEAN[:MINimum\|MAXimum\|IPOSitive\|INEGative\|INTegral]` | Average Mean Current |
| Rectified Mean Current | `CURRent[1..6\|460]:RMEAN[:MINimum\|MAXimum]` | Average Rectified Mean Current |
| Rectified Mean Current, corrected | `CURRent[1..6\|460]:RMCORR[:MINimum\|MAXimum]` | Average Rectified Mean Current Corrected |
| Peak-to-peak current | `CURRent[1..6]:PTP[:MINimum\|MAXimum]` | |
| Highest value within the averaging interval | `CURRent[1..6]:PHIGH[:MINimum\|MAXimum]` | |
| Lowest value within the averaging interval | `CURRent[1..6]:PLOW[:MINimum\|MAXimum]` | |
| Current harmonic | `CURRent[1..6\|460]:HAR[:MINimum\|MAXimum]` | Average Current Harm |
| Current, crest factor | `CURRent[1..6]:CFACtor[:MINimum\|MAXimum]` | |
| Current, absolute phase | `CURRent[1..6]:PHASe[:MINimum\|MAXimum]` (relative to the synchronization signal) | |
| Current, form factor | `CURRent[1..6]:FFACtor[:MINimum\|MAXimum]` | |
| Current, harmonic content | `CURRent[1..6]:HCONTent[:MINimum\|MAXimum]` | |
| Current, fundamental content | `CURRent[1..6]:FCONTent[:MINimum\|MAXimum]` | |
| Current THD | `CURRent[1..6]:THD[:MINimum\|MAXimum]` | |
| Active power | `POWer[1..6\|460][:ACTive][:MINimum\|MAXimum\|IPOSitive\|INEGative\|INTegral]` | Total Active Power |
| Apparent power | `POWer[1..6\|460]:APParent[:MINimum\|MAXimum\|IPOSitive\|INEGative\|INTegral]` | Total Apparent Power |
| Reactive power | `POWer[1..6\|460]:REACtive[:MINimum\|MAXimum\|IPOSitive\|INEGative\|INTegral]` | Total Reactive Power |
| Power factor | `POWer[1..6\|460]:FACTor[:MINimum\|MAXimum]` | Total Power Factor |
| Corrected power | `POWer[1..6\|460]:CORRected[:MINimum\|MAXimum]` | Total Corrected Power |
| Electrical efficiency | `POWer[460]:EFFiciency[:MINimum\|MAXimum]` | Total Electrical Efficiency |
| Phase angle between U and I (arccos[PF]) | `PHASe[1..6\|460][:MINimum\|MAXimum]` | Total Phase Angle (arccos[PF]) |
| Apparent impedance | `IMPedance[1..6\|460][:APParent][:MINimum\|MAXimum]` | Total App. Impedance |
| Series resistance | `RESistance[1..6\|460]:SERial[:MINimum\|MAXimum]` | Total Serial Resistance |
| Parallel resistance | `RESistance[1..6\|460]:PARallel[:MINimum\|MAXimum]` | Total Parallel Resistance |
| Series reactance | `REACTance[1..6\|460]:SERial[:MINimum\|MAXimum]` | Total Serial Reactance |
| Parallel reactance | `REACTance[1..6\|460]:PARallel[:MINimum\|MAXimum]` | Total Parallel Reactance |
| Active power, harmonic | `POWer[1..6\|460][:ACTive]:HAR[:MINimum\|MAXimum\|IPOSitive\|INEGative\|INTegral]` | Total Active Power Harm. |
| Apparent power, harmonic | `POWer[1..6\|460]:APParent:HAR[:MINimum\|MAXimum\|IPOSitive\|INEGative\|INTegral]` | Total Apparent Power Harm. |
| Reactive power, harmonic | `POWer[1..6\|460]:REACtive:HAR[:MINimum\|MAXimum\|IPOSitive\|INEGative\|INTegral]` | Total Reactive Power Harm. |
| Power factor, harmonic | `POWer[1..6\|460]:FACTor:HAR[:MINimum\|MAXimum]` | Total Power Factor Harm. |
| Electrical efficiency, harmonic | `POWer[460]:EFFiciency:HAR[:MINimum\|MAXimum]` | Total Electrical Efficiency Harm. |
| Phase angle U to I, harmonic (arccos[PF]) | `PHASe[1..6\|460]:HAR[:MINimum\|MAXimum]` | Total Phase Angle Har. (arccos[PF]) |
| Apparent impedance, harmonic | `IMPedance[1..6\|460][:APParent]:HAR[:MINimum\|MAXimum]` | Total App. Impedance Harmonic |
| Series resistance, harmonic | `RESistance[1..6\|460]:SERial:HAR[:MINimum\|MAXimum]` | Total Serial Resistance Harmonic |
| Parallel resistance, harmonic | `RESistance[1..6\|460]:PARallel:HAR[:MINimum\|MAXimum]` | Total Par. Resistance Harmonic |
| Series reactance, harmonic | `REACTance[1..6\|460]:SERial:HAR[:MINimum\|MAXimum]` | Total Serial Reactance Harmonic |
| Parallel reactance, harmonic | `REACTance[1..6\|460]:PARallel:HAR[:MINimum\|MAXimum]` | Total Par. Reactance Harmonic |
| SYNC frequency | `FREQuency[:MINimum\|MAXimum]` | |
| Length of the averaging interval in seconds | `TIME[:INTerval][:MINimum\|MAXimum]` | |
| Time [s] since the timer was reset | `TIME:RELative` | |

#### Additional functions available with process interface PI1 installed

| Function (quantity) | Command | No suffix |
|---|---|---|
| Shaft torque | `TORQue[1..4][:MINimum\|MAXimum]` | `TORQue1` |
| Rotational speed | `SPEed[1..4][:MINimum\|MAXimum]` | `SPEed1` |
| Mechanical power | `POWer[1..4]:MECHanical[:MINimum\|MAXimum]` | `POWer1:MECHanical` |
| Slip | `SLIP[1..4][:MINimum\|MAXimum]` | `SLIP1` |
| Mechanical efficiency | `EFFiciency[1..4][:MINimum\|MAXimum]` | `EFFiciency1` |
| Raw input value | `GPINput[1..8][:MINimum\|MAXimum]` | `GPINput1` |

## Subsystems: ABORt through ROUTe

This chapter documents the ABORt, CALCulate, DISPlay, FORMat, HARDcopy (HCOPy), INITiate, INPut, OUTPut and ROUTe subsystems. The SCPI short form is shown in upper case in the command name (e.g. `CALCulate` = short form `CALC`).

### The ABORt subsystem

The ABORt subsystem contains the commands for aborting triggered actions. Once an action has been aborted, it can be triggered again immediately. All the commands are events and have no *RST value.

| Command | Parameter | Default value/unit | Note |
|---|---|---|---|
| `:ABORt` | – | – | No query |

#### `ABORt`

Resets the trigger/synchronization system. Once the measurement or data storage has started after the trigger/sync condition has been met, this command has no effect. The command takes the instrument from the "waiting for trigger/sync" state back to the state it was in before the `INITiate[:IMMediate]:SEQuence` command was sent.

- **Parameters:** none
- **Response:** no query
- **\*RST state:** –
- **Invalidates / invalidated by:** –

Example:

```
ABOR
```

### The CALCulate subsystem

The CALCulate subsystem contains commands for spectrum computation, user-defined computation of electrical efficiency and integration of the averaged values.

| Command | Parameter | Default value/unit | Note |
|---|---|---|---|
| `:CALCulate:TRANsform:FREQuency[:STATe]` | `ONCE` | – | No query |
| `:CALCulate:TRANsform:FREQuency:MODE` | `FFT \| DFT \| STD` | `FFT` | |
| `:CALCulate:TRANsform:FREQuency:FUNCtion` | `<function list>` | – | |
| `:CALCulate:TRANsform:FREQuency:STARt` | `0` | – | FFT and DFT only |
| `:CALCulate:TRANsform:FREQuency:STOP` | `<value>` | – | FFT and DFT only |
| `:CALCulate:TRANsform:FREQuency:CYCLes` | `4 \| 6 \| 8 \| 10 \| 12` | `10` | STD only |
| `:CALCulate:TRANsform:FREQuency:GROuping` | `COMPonent \| HARMonic \| HGRoup \| HSGRoup \| ISGRoup \| SGRoup` | – | STD only |
| `:CALCulate:DATA?` | `[<count>,[<offset>]]` | – | Query only |
| `:CALCulate:DATA:PREamble?` | – | – | Query only |
| `:CALCulate:DATA:THD?` | – | – | Query only |
| `:CALCulate:INTegral[:STATe]` | `ON \| OFF` | `OFF` | |
| `:CALCulate:INTegral:FUNCtion` | `<function list>` | – | |
| `:CALCulate:INTegral:CLEar[:IMMediate]` | – | – | |
| `:CALCulate:INTegral:CLEar:AUTO` | `ON \| OFF` | `ON` | |
| `:CALCulate:INTegral:STARt:SOURce` | `CMD \| TIME \| MAN` | `CMD` | |
| `:CALCulate:INTegral:STARt[:IMMediate]` | – | – | |
| `:CALCulate:INTegral:STARt:TIME` | `yyyy,mm,dd,hh,mm,ss` | – | |
| `:CALCulate:INTegral:STOP:SOURce` | `CMD \| TIME \| MAN \| TINTerval` | `CMD` | |
| `:CALCulate:INTegral:STOP[:IMMediate]` | – | – | |
| `:CALCulate:INTegral:STOP:TIME` | `yyyy,mm,dd,hh,mm,ss` | – | |
| `:CALCulate:INTegral:STOP:TINTerval` | `0 to ...` | – | |
| `:CALCulate:HARMonic:ORDer` | `0 to 40` | `1` | |
| `:CALCulate:POWer:EFFiciency:REFerence` | `<function 1>, <function 2>` | – | |
| `:CALCulate:POWer:CORRected` | `STAR \| DELTa` | – | |

#### `CALCulate:TRANsform:FREQuency[:STATe] ONCE`

Starts a single computation of the frequency transform (spectrum), i.e. the instrument computes the spectrum only on receipt of this command. Attempting to perform a spectrum computation while `SENSe:SWEep1[:STATe]` is `ON` (memory storage of samples) generates the error "-221, Settings conflict".

- **Parameters:** `ONCE`
- **\*RST state:** –
- **Invalidates / invalidated by:** –

Example:

```
CALC:TRAN:FREQ ONCE
```

#### `CALCulate:TRANsform:FREQuency:MODE FFT | DFT | STD`

Selects the computation method for harmonics.

| Parameter | Meaning |
|---|---|
| `FFT` | Computes an FFT amplitude spectrum. The number of lines is determined by the instrument and depends on the stop frequency, the anti-alias filter setting and the instrument model. |
| `DFT` | Computes a DFT amplitude spectrum, i.e. the frequency and amplitude of the fundamental and its integer multiples from the FFT spectrum. The number of lines (harmonics) depends on the selected frequency range and the fundamental frequency (max. 41 including the DC component). |
| `STD` | Computes harmonics according to the EN61000-4-7 Ed 2.1 standard. The instrument must be synchronized to the fundamental frequency. |

- **\*RST state:** `FFT`
- **Invalidates / invalidated by:** –

Examples:

```
CALC:TRAN:FREQ:MODE FFT
CALC:TRAN:FREQ:MODE?    Response: FFT
```

#### `CALCulate:TRANsform:FREQuency:FUNCtion <function>{,<function>}`

Selects the function(s) `<function>` the instrument is to use in the spectrum computation. `<function>` is given as a quoted string, for example: `"VOLTage1"`. A comma-separated list of `<sensor_function>` can be passed as parameters. When a new list is sent, the previously active list (if any) is invalidated.

The query response returns a comma-separated list of functions, where each function is `<STRING RESPONSE DATA>`, i.e. enclosed in quotation marks. The query returns the short-form mnemonics and omits any default nodes in `<function>`.

This function list is not stored with `*SAV` and is cleared by `*RST`.

- **\*RST state:** empty list = no values defined
- **Invalidates / invalidated by:** –

Examples:

```
CALC:TRAN:FREQ:FUNC "VOLT1","CURR1","POW1"
CALC:TRAN:FREQ:FUNC?    Response: "VOLT1","CURR1","POW1"
```

#### `CALCulate:TRANsform:FREQuency:STARt <frequency>`

Sets the start frequency for the computation of harmonics, in hertz. The command accepts only 0 and always returns 0. The command is implemented for compatibility reasons.

- **Parameters:** `<frequency>`
- **\*RST state:** 0.0 Hz
- **Invalidates / invalidated by:** –

Examples:

```
CALC:TRAN:FREQ:STAR 0.0
CALC:TRAN:FREQ:STAR?    Response: 0.0
```

#### `CALCulate:TRANsform:FREQuency:STOP <frequency>`

Sets the upper frequency for the computation of harmonics (the frequency of the outermost FFT or DFT spectrum line).

- **Parameters:** `<frequency>`
  - Minimum: 10 Hz
  - Maximum: sample rate / 2
  - The sample rate can be read with `[:SENSe]:SWEep:FREQuency?`.
  - The instrument coerces the value given to the nearest higher exact frequency.
- **\*RST state:** highest possible value (instrument dependent)
- **Invalidates / invalidated by:** –

Examples:

```
CALC:TRAN:FREQ:STOP 625.0
CALC:TRAN:FREQ:STOP?    Response: 625.0
```

#### `CALCulate:TRANsform:FREQuency:CYCLes 4 | 6 | 8 | 10 | 12`

Defines the length of the analysis interval. The setting applies in STD mode only.

- **Parameters:** `<cycles>` – selects the number of fundamental cycles for the analysis interval (restricted list). The default values are 10 for 50 Hz and 12 for 60 Hz nominal frequency (corresponding to an interval length of 200 ms at fnom).
- **\*RST state:** `10`
- **Invalidates / invalidated by:** –

#### `CALCulate:TRANsform:FREQuency:GROuping COMPonent | HARMonic | HGRoup | HSGRoup | ISGRoup | SGRoup`

Sets the grouping mode for post-processing of the harmonic analysis. The mode can be changed even after a triggered analysis (`CALC:TRAN:FREQ ONCE`) in order to re-read the data from the same interval with a different grouping mode. The setting applies in STD mode only.

| Parameter | Meaning |
|---|---|
| `COMPonent` | No grouping, only the spectral components from the base FFT are delivered (Y C,k) |
| `HARMonic` | The harmonic components are computed (Y H,h) |
| `HGRoup` | The harmonic groups are computed (Y g,h) |
| `HSGRoup` | The harmonic subgroups are computed (Y sg,h) |
| `ISGRoup` | The interharmonic subgroups are computed (Y isg,h) |
| `SGRoup` | Both harmonic and interharmonic subgroups are computed (Ysg,h, Yisg,h) |

- **\*RST state:** `COMPonent`
- **Invalidates / invalidated by:** –

#### `CALCulate:DATA? [<count>[,<offset>]]`

Returns spectrum data in the format defined by the FORMat commands. When the command is sent without arguments, all data is returned in accordance with the preamble information. The first spectrum line returned corresponds to the DC component of the signal.

If a harmonic could not be computed because the sampling theorem was violated for a frequency in DFT mode, NaN is returned for that harmonic.

If a harmonics measurement has not yet been performed, or if this query is sent while a harmonics measurement is in progress (bit 12 in `STATus:OPERation` is set), the error "-230, Data corrupt or stale" is generated and no data is returned.

- **Parameters (optional):**
  - `<count>` – states the number of lines/harmonics to be returned per function.
  - `<offset>` – if not given, spectrum data is returned starting from line/harmonic 0 (the DC component). Otherwise the first line returned has the index given by `<offset>`.
  - If `<offset>` + `<count>` exceeds the number of available lines, the error "-222, Data out of range" is generated. Use `CALCulate:DATA:PREamble?` to find the actual number of available harmonics/lines.
- **Response:**
  - When `FORMat:TRANspose` is `ON`, the points are grouped per function:
    `<line1>,<line2>,<line3>,... (func1) ... <line1>,<line2>,<line3>,... (func2)`
  - When `FORMat:TRANspose` is `OFF`, the points are grouped per line:
    `<func1>,<func2>,<func3>,... (line1) ... <func1>,<func2>,<func3>,... (line2)`
- **\*RST state:** no response to this command after reset.
- **Invalidates / invalidated by:** –

Example:

```
CALC:DATA?    Response: 221.56,0.056,15.456,0.075,5.24,0.034...
```

#### `CALCulate:DATA:PREamble?`

Reads the preamble for the spectrum data. The preamble information is valid only for spectrum data from the same single spectrum computation that was started with `CALCulate:TRANsform:FREQuency[:STATe] ONCE`.

If this query is sent while a harmonics measurement is in progress (bit 12 in `STATus:OPERation` is set), the error "-230, Data corrupt or stale" is generated and no preamble data is returned.

- **Response:** `<start_time>,<line_count>,<function_count>,<freq>[,<freq>,...]`
  - `<start_time>` – states the time interval between `TIMer:RESet:TIME?` and the first point of the sampled data used in the last spectrum computation. If a harmonics measurement has not yet been performed, the ASCII equivalent of NaN (+9.91E+37) is returned for this element.
  - `<line_count>` – states the number of spectrum lines computed per function.
  - `<function_count>` – states the number of functions in the spectrum function list.
  - `<freq>[,<freq>,...]` – a list of frequency steps (FFT), fundamental frequency (DFT) or synchronization frequency (STD) for each function in the spectrum list. The first frequency corresponds to the first function in the function list, the second frequency to the second function, and so on. For FFT (frequency step) and STD (synchronization frequency) the `<freq>` values are identical for all functions; for DFT each function may have an individual fundamental frequency. If no fundamental frequency can be found (DFT), or the synchronization frequency is invalid or out of range (STD), an ASCII equivalent of NaN (+9.91E37) is returned.
- **Invalidates / invalidated by:** –

Example:

```
CALC:DATA:PRE?    Response: 11.32, 40, 3, 50.0,50.0,50.0
```

#### `CALCulate:DATA:THD?`

Reads the THD values according to the grouping selected in STD mode. The values are valid only in STD mode (except with `COMPonent` grouping).

- **Response:** `<THD>[,<THD>,...]` – a list of THD values for each function in the spectrum list, given as a relative % of the fundamental. NaN values are returned in the following cases:
  - the mode setting is FFT, DFT, or STD with `COMPonent` grouping
  - the fundamental of a function is less than 5 % of the measuring range
  - synchronization is not locked, or is outside the analysis range for harmonics
- **Invalidates / invalidated by:** –

Example:

```
CALC:DATA:THD?    Response: +2.36780E+00,+5.12943E+00,+9.91000E+37
```

#### `CALCulate:INTegral[:STATe] ON | OFF`

Controls the state of the instrument's integration functionality. When enabled, the instrument can integrate over individual averaged measurement functions.

| Parameter | Meaning |
|---|---|
| `ON` | Integration enabled. |
| `OFF` | Integration disabled. |

- **\*RST state:** `OFF`
- **Invalidates / invalidated by:** –

Examples:

```
CALC:INT ON
CALC:INT?    Response: ON
```

#### `CALCulate:INTegral:FUNCtion <function>{,<function>}`

Sets the function list for integral computation. `<function>` is given as a quoted string, for example: `"POWer1"`. A comma-separated list of `<function>` can be passed as parameters. When a new list is sent, the previously active list (if any) is invalidated.

The query response returns a comma-separated list of functions, where each function is `<STRING RESPONSE DATA>`, i.e. enclosed in quotation marks. The query returns the short-form mnemonics and omits any default nodes in `<function>`.

Averaged POWer and VOLTage/CURRent:MEAN values can be integrated. The specifiers `INTegral | PINTegral | NINTegral` can then be used in the `SENSe:FUNCtion` / `SENSe:DATA?` list to read the integrated values. If the integration function list is changed or integration is switched `OFF`, the values corresponding to the removed functions return NaN in response to `SENSe:DATA?`.

`SENSe:FUNCtion` generates the error "-221, Settings conflict" if an attempt is made to set a `SENSe:FUNCtion` list containing an integrated function that is not part of the integration function list, or while INTegral computation is `OFF`. The maximum number of integrated functions is 6.

`*RCL` and `*RST` clear all integrated values.

- **Parameters:** `<function>` – states the integrated function. List of valid functions:

```
VOLTage1..6:MEAN
CURRent1..6:MEAN
POWer[1..6|460][:ACTive]
POWer[1..6|460]:APParent
POWer[1..6|460]:REACtive
POWer[1..6|460][:ACTive]:HAR
POWer[1..6|460]:APParent:HAR
POWer[1..6|460]:REACtive:HAR
```

- **\*RST state:** depends on the number of installed phases:

| Number of installed phases | Function list |
|---|---|
| 1 | `"POW1"` |
| 2 | `"POW1","POW2"` |
| 3 | `"POW1","POW2","POW3","POW"` |
| 4 | `"POW1","POW2","POW3","POW","POW4"` |
| 5 | `"POW1","POW2","POW3","POW","POW4","POW5"` |
| 6 | `"POW1","POW2","POW3","POW","POW460"` |

- **Invalidates / invalidated by:** –

Examples:

```
CALC:INT:FUNC "POW1:ACT"
CALC:INT:FUNC?    Response: "POW1"
```

#### `CALCulate:INTegral:CLEar[:IMMediate]`

Sets the values of all the integrated functions to zero. All values are cleared at the same time.

- **Parameters:** –
- **\*RST state:** –
- **Invalidates / invalidated by:** –

Example:

```
CALC:INT:CLE
```

#### `CALCulate:INTegral:CLEar:AUTO ON | OFF`

Controls automatic clearing of the integrated functions.

| Parameter | Meaning |
|---|---|
| `ON` | Integrated values are cleared when integration starts. All values are set to zero at integration start. |
| `OFF` | Automatic clearing of integrated values is disabled. |

- **\*RST state:** `ON`
- **Invalidates / invalidated by:** –

Examples:

```
CALC:INT:CLE:AUTO ON
CALC:INT:CLE:AUTO?    Response: ON
```

#### `CALCulate:INTegral:STARt:SOURce CMD | TIME | MAN`

Sets the start condition for integration.

| Parameter | Meaning |
|---|---|
| `CMD` | Integration starts on receipt of the command `CALCulate:INTegral:STARt[:IMMediate]`. |
| `TIME` | Integration starts at a time given with the command `CALCulate:INTegral:STARt:TIME`. |
| `MAN` | Integration starts when the user presses the F1 key on the front panel from the integration measurement screen on the instrument. |

- **\*RST state:** `CMD`
- **Invalidates / invalidated by:** –

Examples:

```
CALC:INT:STAR:SOUR CMD
CALC:INT:STAR:SOUR?    Response: CMD
```

#### `CALCulate:INTegral:STARt[:IMMediate]`

Starts integration immediately. The command also sets the integrated values to zero if `CALCulate:INTegral:CLEar:AUTO` is set to `ON`.

Attempting to start integration with this command when `CALCulate:INTegral:STARt:SOURce` is not set to `CMD` generates the error "-221, Settings conflict".

- **Parameters:** –
- **\*RST state:** –
- **Invalidates / invalidated by:** –

Example:

```
CALC:INT:STAR
```

#### `CALCulate:INTegral:STARt:TIME <yyyy,MM,dd,hh,mm,ss>`

Sets the start time for integration. Integration starts when the instrument's internal date/time equals the time given with this command.

| Parameter | Meaning |
|---|---|
| `yyyy` | Year |
| `MM` | Month |
| `dd` | Day |
| `hh` | Hours in 24-hour notation |
| `mm` | Minutes |
| `ss` | Seconds (integer value) |

- **\*RST state:** `2002,1,1,0,0,0`
- **Invalidates / invalidated by:** –

Examples:

```
CALC:INT:STAR:TIME 2002,01,12,12,30,00
CALC:INT:STAR:TIME?    Response: 2002,01,12,12,30,00
```

#### `CALCulate:INTegral:STOP:SOURce CMD | TIME | MAN | TINTerval`

Sets the stop condition for integration. Stopping integration does not clear the integrated values.

| Parameter | Meaning |
|---|---|
| `CMD` | Integration stops on receipt of the command `CALCulate:INTegral:STOP[:IMMediate]`. |
| `TIME` | Integration stops at a time given with the command `CALCulate:INTegral:STOP:TIME`. |
| `MAN` | Integration stops when the user presses the F2 key on the front panel from the integration measurement screen on the instrument. |
| `TINTerval` | Integration stops after the interval given with `CALCulate:INTegral:STOP:TINTerval` has elapsed, counted from integration start. |

- **\*RST state:** `CMD`
- **Invalidates / invalidated by:** –

Examples:

```
CALC:INT:STOP:SOUR CMD
CALC:INT:STOP:SOUR?    Response: CMD
```

#### `CALCulate:INTegral:STOP[:IMMediate]`

Stops integration immediately. Stopping integration does not clear the integrated values.

Attempting to use this command when `CALCulate:INTegral:STARt:SOURce` is not set to `CMD` generates the error "-221, Settings conflict". *(As stated in the manual.)*

- **Parameters:** –
- **\*RST state:** –
- **Invalidates / invalidated by:** –

Example:

```
CALC:INT:STOP
```

#### `CALCulate:INTegral:STOP:TIME <yyyy,MM,dd,hh,mm,ss>`

Sets the stop time for integration. Integration stops when the instrument's internal date/time equals the time given with this command.

| Parameter | Meaning |
|---|---|
| `yyyy` | Year |
| `MM` | Month |
| `dd` | Day |
| `hh` | Hours in 24-hour notation |
| `mm` | Minutes |
| `ss` | Seconds (integer value) |

- **\*RST state:** `2010,1,1,0,0,0`
- **Invalidates / invalidated by:** –

Examples:

```
CALC:INT:STOP:TIME 2002,01,12,12,30,00
CALC:INT:STOP:TIME?    Response: 2002,01,12,12,30,00
```

#### `CALCulate:INTegral:STOP:TINTerval <interval>`

Sets the integration interval in seconds. Integration stops after this interval has elapsed, counted from integration start.

- **Parameters:** `1.0e-3` to `9.99e+6`
- **\*RST state:** `6.00000E+01`
- **Invalidates / invalidated by:** –

Examples:

```
CALC:INT:STOP:TINT 1.0
CALC:INT:STOP:TINT?    Response: 1.0
```

#### `CALCulate:HARMonic:ORDer <order>`

Sets the harmonic order for the measurement function `VOLTage[1..6|460]:HAR[:MINimum|MAXimum]`.

- **Parameters:** `0` to `40` (currently only `1` is valid)
- **\*RST state:** `1`
- **Invalidates / invalidated by:** –

Examples:

```
CALC:HARM:ORD 1
CALC:HARM:ORD?    Response: 1
```

#### `CALCulate:POWer[460]:EFFiciency:REFerence <function1>,<function2>`

Sets two functions for user-defined computation of electrical efficiency.

- `CALCulate:POWer:EFFiciency:REFerence` sets the electrical efficiency variables for the 1st 2- or 3-phase system (`POWer:EFFiciency`).
- `CALCulate:POWer460:EFFiciency:REFerence` sets the electrical efficiency variables for the 2nd 3-phase system (`POWer460:EFFiciency`).

- **Parameters:** `<function1>` and `<function2>` may be any of the averaged active powers the instrument measures: `"POWer[1..6|460][:ACTive]"`
- **\*RST state:** depends on instrument type and number of installed phases.
- **Invalidates / invalidated by:** –

Examples:

```
CALC:POW:EFF:REF "POW460", "POW1"
CALC:POW:EFF:REF?    Response: "POW460", "POW1"
```

#### `CALCulate:POWer:CORRected STAR | DELTa`

Selects phase-to-neutral or phase-to-phase voltages for the computation of no-load loss measurements on transformers according to IEC60076-1 (the measurement function `POWer[1|2|3|4|5|6|460]:CORRected`).

In a 3-phase instrument, `STAR` is not possible together with W2 (Aron); the parameter is then set automatically to `DELTa`. If there are more than 3 phases, `STAR` can still be selected in W2 mode, but it then applies only to phase 4 and above. Pcorr from the first system is then not available.

The command is only accepted by firmware version V1.4 and later.

| Parameter | Meaning |
|---|---|
| `STAR` | Use phase-to-neutral voltages (star/wye connection). |
| `DELTa` | Use phase-to-phase voltages (delta connection). |

- **\*RST state:** `CALCulate:POWer:CORRected?`: `STAR`
- **Invalidates:** –
- **Invalidated by:** `ROUTe:SYST "2W"` (3-phase instruments only)

Examples:

```
CALC:POW:CORR DELT
CALC:POW:CORR?    Response: DELT
```

### The DISPlay subsystem

The DISPlay subsystem contains commands for controlling the display.

| Command | Parameter | Default value/unit | Note |
|---|---|---|---|
| `:DISPlay[:WINDow][:STATe]` | `ON \| OFF` | `OFF` | |
| `:DISPlay:USER:FUNCtion` | `<function list>` | – | |

#### `:DISPlay[:WINDow][:STATe] ON | OFF`

Controls whether the instrument's processor updates the display. Display brightness and power consumption are not affected by this command.

| Parameter | Meaning |
|---|---|
| `ON` | The display is updated by the processor. |
| `OFF` | The instrument's processor does not update the display, freeing more processing power for extensive computations. |

- **\*RST state:** `OFF`
- **Invalidates / invalidated by:** –

Examples:

```
DISP ON
DISP?    Response: ON
```

#### `:DISPlay:USER:FUNCtion <function>{,<function>}`

Sets the function list for the user-defined measurement screen.

- **Parameters:** `<function>{,<function>}`
- **\*RST state:** empty list = no values defined
- **Invalidates / invalidated by:** –

Examples:

```
DISP:USER:FUNC "VOLT1:PHIGH","VOLT1:PLOW","VOLT1:PTP"
DISP:USER:FUNC?    Response: "VOLT1:PHIGH","VOLT1:PLOW","VOLT1:PTP"
```

### The FORMat subsystem

The FORMat subsystem sets the data format for the transfer of numeric measurement data and measurement arrays. This data format is used for the response data of those commands specifically stated to be affected by the FORMat subsystem.

| Command | Parameter | Default value/unit | Note |
|---|---|---|---|
| `:FORMat[:DATA]` | `ASCii \| REAL`, `0 to 8 \| 32 \| 64` | `ASCii` | Default length for REAL is 64 |
| `:FORMat[:DATA]:STATus` | `ASCii \| INTeger, 8 \| 16 \| 32` | `ASCii` | Default length for INTeger is 8 |
| `:FORMat:BORDer` | `NORMal \| SWAPped` | `NORMal` | Length for INTeger only |
| `:FORMat:TRANspose` | `ON \| OFF` | `OFF` | |

#### `FORMat[:DATA] ASCii | INTeger | REAL, [0..8] | 16 | [32 | 64]`

Sets the data format for the transfer of measurement values from the instrument. The command applies to all types of measurement data: averaged measurements, memory recordings, spectrum/FFT data and so on. If `<length>` is not given, the instrument uses the last valid setting.

This command is coupled with `FORMat[:DATA]:STATus`. A change from ASCii (text) format to REAL | INTeger (binary) format or vice versa, via either `FORMat[:DATA]` or `FORMat[:DATA]:STATus`, changes both formats — both the measurement data and the status information format. These formats are always either both text or both binary. The last valid length is used for the indirectly changed format.

**Parameters – type:**

| Type | Meaning |
|---|---|
| `ASCii` | Data is transferred as a floating point value formatted as a string. Multiple ASCii data values are separated by commas. The length states the number of mantissa digits in scientific notation. If the measurement status is error (8), the measurement value is NaN = +9.91E+37. |
| `REAL` | Data is transferred as floating point numbers of the stated length in definite-length block binary format: `#abbbb....` where one character ('0'–'9') gives the number of b characters that state the number of data bytes, then the number of data bytes that follow immediately, and then the data bytes themselves. REAL data is by default in big-endian byte order. The byte order can be changed with the `FORMat:BORDer` command. |

**Parameters – length [bits]:**

| Length | Meaning |
|---|---|
| `0 to 8` | Applies to ASCii. The length states the number of mantissa digits in scientific notation. For non-zero lengths the values are formatted with the C format string `"%+.(length-1)e"`. A `<length>` value of zero means the unit itself chooses the number of significant digits returned. The maximum length for ASCii is 8. The default length is 6. |
| `16` | Applies to INTeger. States the number of bits representing the signed integer. |
| `32 \| 64` | Applies to REAL. States the length of the binary representation of the floating point number in bits (default is 64). |

If the measurement status is error (8), the measurement value is IEEE 754 NaN:

```
FORMat:BORDer SWAPped
REAL,32 = {0, 0, 0xC0, 0x7F}
REAL,64 = {0, 0, 0, 0, 0, 0, 0xF8, 0x7F}
FORMat:BORDer NORMal
REAL,32 = {0x7F, 0xC0, 0, 0}
REAL,64 = {0x7F, 0xF8, 0, 0, 0, 0, 0, 0}
```

- **Response:** `<type>,[<length>]`
- **\*RST state:** `ASCii,6`
- **Invalidates:** `FORMat[:DATA]:STATus`
- **Invalidated by:** `FORMat[:DATA]:STATus`

Examples:

```
FORM ASC,6
FORM REAL,32
FORM?    Response: REAL,64
```

#### `FORMat[:DATA]:STATus ASCii | INTeger, [8] | 16 | 32`

Sets the format for the status information when transferring measurement values from the instrument. Status information is an integer.

The command applies to averaged measurements and memory recordings of averaged data. If `<length>` is not given, the instrument uses the last valid setting.

This command is coupled with `FORMat[:DATA]`. A change from ASCii (text) format to REAL | INTeger (binary) format or vice versa, via either `FORMat[:DATA]` or `FORMat[:DATA]:STATus`, changes both formats — both the measurement data and the status information format. These formats are always either both text or both binary. The last valid length is used for the indirectly changed format.

| Parameter | Meaning |
|---|---|
| `ASCii` | The status information is transferred as an integer value formatted as a string. Multiple status information values are separated by commas. Length is not valid for the ASCii format of status information. |
| `INTeger,[8]\|16\|32` | The status information value is transferred as a binary integer of the stated length. INTeger status information values are in big-endian byte order. For the default length of 8 bits, the length can be omitted. |

- **Response:** `<type>,[<length>]`
- **\*RST state:** `ASCii`
- **Invalidates:** `FORMat[:DATA]`
- **Invalidated by:** `FORMat[:DATA]`

Examples:

```
FORM:STAT ASC
FORM:STAT INT,8
FORM?    Response: INT,8
```

#### `FORMat:BORDer NORMal | SWAPped`

States whether binary data transferred over the interface is in normal (Motorola) or swapped (Intel) byte order. The command applies to all types of binary measurement data: averaged measurements, memory recordings, spectrum/FFT data and so on.

| Parameter | Meaning |
|---|---|
| `NORMal` | Big-endian data format (Motorola). |
| `SWAPped` | Little-endian data format (Intel). |

- **Response:** `<format>`
- **\*RST state:** `NORMal`
- **Invalidates / invalidated by:** –

Examples:

```
FORM:BORD SWAP
FORM:BORD?    Response: SWAP
```

#### `FORMat:TRANspose ON | OFF`

The command applies to memory recording (TRACe) and to spectrum data output. The data can be regarded as a 2D array (matrix). The command selects whether the rows and columns of the matrix are swapped.

| Parameter | Meaning |
|---|---|
| `ON` | The values are grouped per measurement function: all values for function 1, all values for function 2, ... |
| `OFF` | The values are grouped per interval/spectrum line: all values from interval 1, all values from interval 2, ... |

- **\*RST state:** `OFF`
- **Invalidates / invalidated by:** –

Examples:

```
FORM:TRAN ON
FORM:TRAN?    Response: ON
```

### The HARDcopy subsystem (HCOPy)

The HARDcopy subsystem contains commands for retrieving the image of the instrument screen.

| Command | Parameter | Default value/unit | Note |
|---|---|---|---|
| `:HCOPy:SDUMp:DATA?` | – | – | Query only |

#### `HCOPy:SDUMp:DATA?`

Returns screen dump data (in the internal format accepted by the PC program for transferring instrument screen images).

- **Parameters:** –
- **Response:** block of RLE-encoded screen data
- **\*RST state:** –
- **Invalidates / invalidated by:** –

Example:

```
HCOP:SDUM:DATA?
Response: Block of RLE-encoded screen data
```

### The INITiate subsystem

The INITiate subsystem controls the operation of the instrument's averaging functionality. If memory recording is enabled, it also initiates the trigger/synchronization subsystem.

| Command | Parameter | Default value/unit | Note |
|---|---|---|---|
| `:INITiate:CONTinuous` | `ON \| OFF` | `ON` | |
| `:INITiate[:IMMediate]` | – | – | No query |
| `:INITiate[:IMMediate]:SEQuence1` | – | – | |
| `:INITiate[:IMMediate]:NAME` | `STARt` | – | |

#### `INITiate:CONTinuous ON | OFF`

Controls the continuous state of the averaging functionality. If set to `ON`, the instrument automatically starts a new averaging cycle when the previous one has finished. `INITiate:CONTinuous ON` should be used for gap-free measurements.

| Parameter | Meaning |
|---|---|
| `ON` | Free-run mode. The instrument automatically starts a new averaging cycle when the previous one has finished. |
| `OFF` | Single-shot mode. The instrument performs one averaging cycle on receipt of either `INIT[:IMMediate]` or `*TRG`. The instrument is then returned to the IDLE state. |

- **\*RST state:** `ON`
- **Invalidates / invalidated by:** –

Examples:

```
INIT:CONT ON
INIT:CONT?    Response: 1
```

#### `INITiate[:IMMediate]`

Leaves the IDLE state and starts a single averaging cycle. When this averaging cycle is complete, the instrument is returned to the IDLE state. If the unit is not in IDLE, or if `INITiate:CONTinuous` is set to `ON`, an IMM command has no effect and error -213 is generated.

- **Parameters:** –
- **\*RST state:** –
- **Invalidates / invalidated by:** –

Example:

```
INIT
```

#### `INITiate[:IMMediate]:SEQuence1` / `INITiate[:IMMediate]:NAME STARt`

Initiates the start trigger for memory acquisition. After initiation, the pretrigger (if > 0) is filled. The pretrigger is full when the "waiting for trigger" bit in the OPER:STAT register is set to 1. `STARt` is an alias for `SEQuence1`.

- **Parameters:** –
- **\*RST state:** –
- **Invalidates / invalidated by:** –

Example:

```
INITiate:NAME STARt
```

### The INPut subsystem

The INPut subsystem controls the characteristics of the input channels. Numeric suffixes on the INPut node correspond to the hardware input channel on the instrument. For 6-channel models the valid electrical channel suffixes are 1 to 6. For 12-channel models the valid electrical channel suffixes are 1 to 12. The electrical INPut subsystem does not distinguish between current and voltage channels. The input filter settings are common to all electrical channels, so the channel suffixes can be omitted, even though they are implemented for compatibility reasons. If the channel suffix is omitted, the command applies to input 1.

For the Process Interface option, the valid mechanical channel suffixes are 21 to 24 (torque input 1..4) and 25 to 28 (speed input 1..4). See section 2.1.10 SENSe2 Subsystem for a detailed description.

| Command | Parameter | Default value/unit | Note |
|---|---|---|---|
| `:INPut[1..12]:COUPling` | `AC \| DC` | `DC` | |
| `:INPut[1..12]:GAIN` | `0 to 1.0e12` | `1.0` | Current channels only |
| `:INPut[1..12]:FILTer[:STATe]` | `ON \| OFF` | `ON` | |
| `:INPut[1..12]:FILTer[:LPASs]:FREQuency?` | – | Device dependent | Query only |
| `:INPut[1..12]:SHUNt` | `INTernal \| EXTernal` | `INTernal` | Current channels only |
| `:INPut[21..28]:TYPE` | `VOLTage \| FREQuency` | – | Process Interface option |

#### `INPut[1..12]:COUPling AC | DC`

Sets the input coupling for the selected input channel.

| Parameter | Meaning |
|---|---|
| `AC` | The DC component is removed from the signal before further processing. This coupling compensates for any DC offsets on the signal. |
| `DC` | The signal is left untouched and all components are passed on for processing. This coupling should be used for true RMS computations. |

- **\*RST state:** `DC` for all channels
- **Invalidates / invalidated by:** –

Examples:

```
INP1:COUP AC
INP2:COUP?    Response: DC
```

#### `INPut[1|2|3|4|5|6|7|8|9|10|11|12]:GAIN <gain>`

Sets the shunt factor for the current inputs. The setting applies when the EXTernal input shunt is selected. The even (voltage) channel numbers are only available on instruments equipped with the PP59/PP69 power phase.

- **Parameters:** `1.0e-7` to `1.0e+7`
  - This dimensionless gain factor states the V/A conversion ratio of the external shunt connected to the specified channel. Negative values are not permitted.
- **Response:** `<gain>`
- **\*RST state:** `1.0` for all channels
- **Invalidates / invalidated by:** –

Examples:

```
INP1:GAIN 10.0
INP3:GAIN?    Response: 251.65
```

#### `INPut[1..12]:FILTer[:STATe] ON | OFF`

Switches the anti-alias filters on the inputs on or off. The filters on all channels are coupled, i.e. enabling/disabling the filter on one channel enables/disables the filters on all channels.

| Parameter | Meaning |
|---|---|
| `ON` | Anti-alias filter enabled. |
| `OFF` | Anti-alias filter disabled. |

- **\*RST state:** `ON` for all channels
- **Invalidates / invalidated by:** –

Examples:

```
INP:FILT ON
INP:FILT?    Response: 1
```

#### `INPut[1..12]:FILTer[:LPASs]:FREQuency?`

Reads the cutoff frequency of the anti-alias low-pass filter. All input channels are equipped with the same filters, so the value returned is always identical for all channels. The cutoff frequency of the anti-alias filter cannot be changed.

- **Response:** `<frequency>` in Hz
- **\*RST state:** device dependent for all channels
- **Invalidates / invalidated by:** –

Example:

```
INP:FILT:FREQ?    Response: 300.0e3
```

#### `INPut[1|2|3|4|5|6|7|8|9|10|11|12]:SHUNt INTernal | EXTernal`

Selects the shunt used on the current channel. The even (voltage) channel numbers are only available on instruments equipped with the PP59/PP69 power phase.

| Parameter | Meaning |
|---|---|
| `INTernal` | Internal shunts up to 10 A are used. |
| `EXTernal` | An external shunt is connected. The shunt factor must be set with the `INPut:GAIN` command. |

- **\*RST state:** `INTernal` for all channels
- **Invalidates / invalidated by:** –

Examples:

```
INP1:SHUN EXT
INP3:SHUNt?    Response: INT
```

#### `INPut[21..28]:TYPe VOLTage | FREQuency` (Process Interface option)

Sets the input type to match the sensor type.

| Parameter | Meaning |
|---|---|
| `VOLTage` | A DC signal in the range +/- 10 V is expected at the input. |
| `FREQuency` | An AC signal with a frequency in the range 1 Hz to 200 kHz is expected at the input. |

- **\*RST state:** `FREQuency`
- **Invalidates / invalidated by:** –

Examples:

```
INP21:TYP FREQ
INP21:TYP?    Response: FREQ
```

### The OUTPut subsystem

The OUTPut subsystem controls the characteristics of the SYNC output.

| Command | Parameter | Default value/unit | Note |
|---|---|---|---|
| `:OUTPut9[:STATe]` | `ON \| OFF` | `OFF` | SYNC output |

#### `OUTPut9[:STATe] ON | OFF`

Sets the state of the synchronization output. Can only be set to `ON` if `SYNC:SOURce` is not set to `EXTernal`.

| Parameter | Meaning |
|---|---|
| `ON` | The synchronization pulses are output on the sync in/out connector on the rear. |
| `OFF` | No synchronization pulses are output. |

- **\*RST state:** `OFF`
- **Invalidates:** –
- **Invalidated by:** `SYNC:SOURce EXTernal`

Examples:

```
OUTP9 ON
OUTP9?    Response: 0
```

### The ROUTe subsystem

The ROUTe subsystem selects the connection type the instrument uses to measure on a three-phase system.

| Command | Parameter | Default value/unit | Note |
|---|---|---|---|
| `:ROUTe:SYSTem` | `"3W" \| "2W"` | `"3W"` | |

#### `ROUTe:SYSTem "3W" | "2W"`

Selects the connection type the instrument uses to measure on a three-phase system.

**Note:** The `2W` parameter is only accepted by firmware version 1.4 and higher.

| Parameter | Meaning |
|---|---|
| `"3W"` | Three-wattmeter configuration. |
| `"2W"` | Two-wattmeter configuration. |

- **\*RST state:** `"3W"`
- **Invalidates / invalidated by:** –

Examples:

```
ROUT:SYST "3W"
ROUT:SYST?    Response: "3W"
```

## Subsystems: SENSe, SENSe2 and SOURce

### The SENSe subsystem

The SENSe subsystem controls the instrument's averaging function and the computation of the basic averaged values. Numeric suffixes on the `VOLTage|CURRent` nodes correspond to electrical phases. The channels of the INPut subsystem are combined into phases in the `SENSe:VOLTage|CURRent` subsystem as shown in the table below. If the channel suffix is omitted, the command applies to phase 1.

| Input channel (suffix)               | Electrical phase | Phase suffix in the SENSe subsystem |
|--------------------------------------|------------------|-------------------------------------|
| INPut1 (current), INPut2 (voltage)   | L1             | `SENSe:VOLTage|CURRent1`        |
| INPut3 (current), INPut4 (voltage)   | L2             | `SENSe:VOLTage|CURRent2`        |
| INPut5 (current), INPut6 (voltage)   | L3             | `SENSe:VOLTage|CURRent3`        |
| INPut7 (current), INPut8 (voltage)   | L4             | `SENSe:VOLTage|CURRent4`        |
| INPut9 (current), INPut10 (voltage)  | L5             | `SENSe:VOLTage|CURRent5`        |
| INPut11 (current), INPut12 (voltage) | L6             | `SENSe:VOLTage|CURRent6`        |

The SENSe node is the default node at the root level of the command tree. The default node in the SENSe subsystem is `POWer`. All phase-related SENSe settings are common to both AC and DC coupling, so the `AC | DC` node can be omitted (DC is assumed). The `:AC[|:DC]` node is implemented for compatibility reasons only.

#### Command overview (SENSe)

| Command | Parameter | Default value/unit | Note |
|---|---|---|---|
| `[:SENSe]` | | | |
| `  :CURRent[1..6]|VOLTage[1..6]` | | | |
| `    :AC|[:DC]` | | | |
| `      :RANGe` | | | |
| `        [:UPPer]` | 0.3 to 1000 V | - [V] | VOLTage |
| | 0.03 to 10 | - [A] | CURRent with INTernal shunt |
| | 0.03 to 20 V | | CURRent with EXTernal shunt |
| `        :AUTO` | ON \| OFF \| ONCE | ON | ONCE not yet implemented |
| `        :LIST?` | | | Query only |
| `      :SCALe` | 0.9 to 1.0e+7 | 1.0 [-] | |
| `  [:POWer[1..6]]|CURRent[1..6]|VOLTage[1..6]` | | | |
| `    :AC|[:DC]` | | | |
| `      :APERture` | | | |
| `        [:TIME]` | 15.0e-3 to 3.6e3 | 0.3 s | |
| `  :SWEep` | | | |
| `    :FREQuency?` | | device dependent | Query only |
| `  :FUNCtion` | | | |
| `    [:ON]` | list of sens func | "" | |
| `      :ALL` | | - | No query |
| `      :COUNt?` | | 0 | Query only |
| `    :OFF` | | | |
| `      :ALL` | | | |
| `    :CONCurrent` | ON \| OFF | ON | |
| `  :DATA?` | list of sens func | "" | Query only |
| `    :STATus?` | list of sens func | "" | Query only |
| `  :SWEep1|2` | | | |
| `    :TIME` | \<value\> \| MAX | | |
| `      :MAX?` | | | Query only |
| `    :POINTS?` | | | Query only |
| `    :OFFSet` | | | |
| `      :TIME` | 0 \| \<value\> \| MAX | | |
| `      :POINTS?` | | | Query only |
| `    [:STATe]` | ON \| OFF | OFF | |
| `    :COUNt` | 1 to 65535 | 1 | |
| `    :SFACtor` | 1 to 65535 | 1 | |
| `    :FUNCtion` | \<function list\> | | |

#### Scaling

**`[SENSe:]CURRent[1..6]|VOLTage[1..6]:AC[|:DC]:SCALe <value>`**

Sets the voltage/current scaling factor, which reflects the transformation ratio of any voltage/current transformers or voltage dividers used. The voltage or current on the specified channel is multiplied by this scaling factor before all further processing. All signal quantities computed on the basis of current and/or voltage are scaled by this factor.

- **Parameters:** `0.9 to 1.0e+7`. Negative values are not permitted.
- **State after `*RST`:** `1.0`
- **Invalidates:** –
- **Invalidated by:** –

```scpi
VOLT3:SCAL 10.0
CURR2:SCAL?          -> 10.0
```

#### Ranging

**`[SENSe:]CURRent[1..6]|VOLTage[1..6]:AC|[:DC]:RANGe[:UPPer] <value>`**

Sets the voltage/current range. The range given is the actual range of the instrument's input channel. Channel scaling factors and external shunts (`INPut:GAIN`) are not included. The command sets the RMS value of the range; the actual peak range is twice as high. A range value within the valid interval is rounded up to the nearest higher possible value.

- **Parameters:**
  - `0.3 to 1000.0 V` – Range for a voltage channel. Applies to the voltage channels (2, 4, 6, 8, 10, 12).
  - `0.03 to 10.0 A` – Range for a current channel when `INPut:SHUNt` is set to `INTernal`. Applies to channels (1, 3, 5, 7, 9, 11).
  - `0.03 to 10.0 V` – Range for a current channel when `INPut:SHUNt` is set to `EXTernal`. The range is set as the voltage at the voltage input of the current channel. The actual current range in amperes is found by multiplying this value by `INPut:GAIN` for the corresponding channel. Applies to channels (1, 3, 5, 7, 9, 11).
- **State after `*RST`:** After reset, autorange is ON, so no fixed range is set.
- **Invalidates:** `[SENSe:]CURRent[1..6]|VOLTage[1..6]:RANGe:AUTO ON`
- **Invalidated by:** `[SENSe:]CURRent[1..6]|VOLTage[1..6]:RANGe:AUTO ON`

```scpi
VOLT3:RANG 25.0
CURR2:RANG?          -> 3.0
```

#### Averaging and range lists

**`[SENSe:]CURRent[1..6]|VOLTage[1..6]:AC[|:DC]:RANGe[:UPPer]:LIST?`**

Queries a list of the valid voltage/current ranges available on the channel given by the phase suffix. The list returned contains the actual ranges of the instrument's input channels. Channel scaling factors and external shunts (`INPut:GAIN`) are not included. For current channels the list depends on the current setting of `INPut:SHUNt` (`EXTernal` or `INTernal`).

- **Response:** `<range_list>` – comma-separated list of range values.
- **Invalidates:** –
- **Invalidated by:** –

```scpi
CURR2:RANG:LIST?
-> 0.03, 0.1, 0.3, 1, 3, 10   (for the INPut:SHUNt INTernal setting)
```

**`[SENSe:]CURRent[1..6]|VOLTage[1..6]:AC[|:DC]:RANGe[:UPPer]:AUTO ON|OFF|ONCE`**

Controls autorange for voltage/current.

- **Parameters:**
  - `ON` – Autorange is permanently ON. Monitoring the `STATus:OPERation` register will detect that the range changes.
  - `OFF` – Autorange is permanently OFF.
  - `ONCE` – ONCE is not yet implemented.
- **State after `*RST`:** `ON`
- **Invalidates:** `[SENSe:]CURRent[1..6]|VOLTage[1..6]:RANGe[:UPPer]`
- **Invalidated by:** `[SENSe:]CURRent[1..6]|VOLTage[1..6]:RANGe[:UPPer]`

```scpi
VOLT3:RANG:AUTO ON
CURR2:RANG:AUTO?     -> 0
```

**`[SENSe:][:POWer[1..6]]|CURRent[1..6]|VOLTage[1..6]:AC[|:DC]:APERture[:TIME] <avgtime>`**

Sets the nominal averaging interval. The query returns the nominal averaging interval that has been set. In synchronous mode the actual averaging interval changes on the fly: the nominal averaging interval is extended to the next whole signal period.

To query the actual averaging period, the command `:SENSe:DATA? "TIME[:INTerval]"` must be used.

If the nominal averaging interval is changed with this command, the synchronization timeout (`SYNC:TIMeout`) is set to the nominal averaging interval or 0.3 seconds, whichever is greater.

> **Note:** The nodes `[:POWer[1..6]]|CURRent[1..6]|VOLTage[1..6]:AC[|:DC]` are implemented for SCPI compatibility only. The instrument works with just one averaging interval for all averaged measurements.

- **Parameters:** `15 ms ... 3600 s`. Resolution 1 ms. The unit is seconds.
- **State after `*RST`:** `0.3 s`
- **Invalidates:** `SYNC:TIMeout`
- **Invalidated by:** –

```scpi
APER 0.2
APER?                -> 1.5
```

#### Sampling frequency

**`[SENSe:]SWEep:FREQuency?`**

Queries the sampling frequency of the instrument's ADCs. The sampling frequency is fixed and cannot be changed.

- **Response:** `<sample_rate>` – the actual sampling frequency the instrument uses for data acquisition. The sampling frequency is common to all channels. The unit is Hz.
- **State after `*RST`:** Device dependent:
  - Norma 3000: 102.4 kHz
  - Norma 4000: 341.33 kHz or 1.024 MHz
  - Norma 5000: 341.33 kHz or 1.024 MHz
- **Invalidates:** –
- **Invalidated by:** –

```scpi
SWEep:FREQ?          -> 3.4133E+05
```

#### Measurement functions

**`[SENSe:]FUNCtion[:ON] <function>{,<function>}`**

The `FUNCtion[:ON]` command selects which `<function>`(s) the instrument is to measure (SENSe). `<function>` is given as a quoted string, for example: `FUNCtion "VOLTage:AC"`. If `CONCurrent` is `OFF`, a single `<function>` is passed as a parameter; that function is selected as the one to be measured. If more than one function is passed, the error `-108 (Parameter not allowed)` is generated. If `CONCurrent` is `ON`, a comma-separated list of `<sensor_function>` can be passed as parameters; those functions are switched on, while all other functions are switched off.

The query `FUNCtion[:ON]?` returns a comma-separated list of the functions that are ON, each as `<STRING RESPONSE DATA>`. If no functions are ON, an empty string is returned. The query returns the short forms and omits any default nodes in `<function>`.

This function list is not stored with `*SAV` and is cleared by `*RST`.

- **Parameters:** `<function>{,<function>}`
- **State after `*RST`:** Empty list = no values defined.
- **Invalidates:** `[SENSe:]FUNCtion[:ON]:COUNt?`
- **Invalidated by:** `[SENSe:]FUNCtion[:ON]:ALL`, `[SENSe:]FUNCtion:OFF:ALL`, `[SENSe:]FUNCtion:CONCurrent OFF`, `[SENSe:]DATA? <function>{,<function>}`, `[SENSe:]DATA:STATUS? <function>{,<function>}`

```scpi
FUNC "VOLT","CURR","POW"
FUNC?                -> "VOLT","CURR","POW"
```

**`[SENSe:]FUNCtion[:ON]:ALL`**

Switches ON all `<sensor_function>`s the instrument can measure simultaneously.

- **State after `*RST`:** –
- **Invalidates:** `[SENSe:]FUNCtion[:ON]:COUNt?`
- **Invalidated by:** `[SENSe:]FUNCtion[:ON]`

```scpi
FUNC:ALL
```

**`[SENSe:]FUNCtion[:ON]:COUNt?`**

The query returns the number of `<sensor_function>`s that are ON.

- **Response:** `<count>` – the number of averaged measurements currently configured.
- **State after `*RST`:** `0`
- **Invalidates:** –
- **Invalidated by:** `[SENSe:]FUNCtion[:ON]`, `[SENSe:]FUNCtion[:OFF]`, `[SENSe:]DATA? <function>{,<function>}`, `[SENSe:]DATA:STATUS? <function>{,<function>}`, `[SENSe:]FUNCtion:CONCurrent OFF`

```scpi
FUNC:COUN?           -> 3
```

**`[SENSe:]FUNCtion:OFF:ALL`**

Switches OFF all `<sensor_function>`s the instrument can measure simultaneously.

- **State after `*RST`:** –
- **Invalidates:** `[SENSe:]FUNCtion[:ON]:COUNt?`
- **Invalidated by:** `[SENSe:]FUNCtion[:ON]`

```scpi
FUNC:OFF:ALL
```

**`[SENSe:]FUNCtion:CONCurrent ON | OFF`**

The `CONCurrent` command states whether the SENSor block is to be configured to measure one function at a time, or more than one function at a time (concurrently).

- **Parameters:**
  - `ON` – The function(s) given as parameters to the `FUNCtion[:ON]` command are switched on, while the state of other functions is set to off.
  - `OFF` – The `FUNCtion[:ON]` command acts as a "one-of-n" switch that selects the given function as the only measured function.
- **State after `*RST`:** `ON`
- **Invalidates:** `[SENSe:]FUNCtion[:ON]:COUNt?`, `[SENSe:]FUNCtion[:ON]`
- **Invalidated by:** –

```scpi
FUNC:CONC ON
FUNC:CONC?           -> 1
```

#### Data query

**`[SENSe:]DATA? [<function,function...>]`**

Returns data in the format defined by the FORMat commands. Without arguments: the number of values returned equals the number of arguments to the `SENSe:FUNCtion[:ON]` command. If `SENSe:FUNCtion:CONCurrent` is `OFF`, only one function/measurement can be configured and returned. If it is `ON`, several functions can be configured and queried for measurement results.

- **Parameters:** `[<function>,<function>,...]`
- **Response:** `<measurement_value>[,<measurement_value>,…]`
- **State after `*RST`:** Empty list = no values defined.
- **Invalidates:** `[SENSe:]FUNCtion[:ON]:COUNt?`, `[SENSe:]FUNCtion[:ON]`
- **Invalidated by:** –

```scpi
DATA? "VOLT","CURR","POW"
-> 221.56,1.056,230.65
```

**`[SENSe:]DATA:STATus? [<function,function...>]`**

Returns the averaged measurement(s) followed by status information for the measurement. The status information states the validity of the measurement and is appended after the set of measurement values. The number of status values equals the number of measurement values returned. The format of the status information is controlled by the `FORMat:STATus` commands.

- **Parameters:** `[<function>,<function>,...]` – see `SENSe:FUNCtion` for a detailed description of the available functions.

**Status values:** The status values returned are integers and are appended at the end of the measurement results. The status value is a bitmask and may be a combination of one or more of the following values (bits) combined with a logical OR. For example, the value 3 represents both an underrange and an overrange condition.

| Value | Name | Meaning |
|---|---|---|
| 0 | Normal | Valid measurement, no questionable condition. |
| 1 | Underrange | The value returned is valid, but the signal amplitude is too low for the given range, so measurement precision is reduced. |
| 2 | Overrange | The instrument returns a measurement value, but the input signal amplitude is too high for the given range and is clipped to an amplitude within the current range. The value returned may be more or less outside the specification. |
| 8 | Undefined | The instrument could not compute a valid value. This may be caused, for example, by loss of synchronization (no valid frequency, harmonics, ...). The instrument returns Not A Number for the measurement. |
| 16 | Not available | The requested function is not, or no longer, available (e.g. option not installed, function switched off). The instrument returns Not A Number for the measurement. |
| 128 | Power Factor capacitive | For the Power Factor function, this indicates a capacitive phase difference between voltage and current (0 = inductive). |

- **Response:** `<measurement_value1>[,<measurement_value2>,…],<measurement_status1>,[<measurement_status2>,…]`
- **State after `*RST`:** Empty list = no values defined.
- **Invalidates:** –
- **Invalidated by:** –

```scpi
DATA:STATUS? "VOLT","CURR","POW"
-> 221.56,1.056,230.65,0,0,0
```

#### Memory recording

The commands that configure memory recording use mandatory suffixes 1 and 2 after the SWEep node:

- `[:SENSe]:SWEep1` – Configures memory recording of sampled values (REALtime).
- `[:SENSe]:SWEep2` – Configures memory recording of averaged values (AVERage).

The settings for memory recording of averaged and sampled values share the same configuration area, and all settings must be set again when switching from recording averaged to sampled values or vice versa. The command `[SENSe:]SWEep1|2[:STATe] OFF` resets the settings to their default values, except for triggers.

**`[SENSe:]SWEep1|2:TIME <value> | MAX`**

Sets the maximum length of the memory recording in seconds. The recording length includes the pretrigger. If synchronization is on, the maximum recording duration for SWEep2 depends directly on the exact number of averaging intervals that will be recorded, computed as the recording length given / nominal averaging interval.

- **Parameters:** `<value> | MAX`
- **State after `*RST`:** `MAX`
- **Invalidates:** –
- **Invalidated by:** –

```scpi
SWE1:TIME 1.0
SWE1:TIME?           -> 1.0
```

**`[SENSe:]SWEep1|2:TIME:MAX?`**

Returns the maximum recording time in seconds according to the current memory recording settings (total amount of available memory, set of variables to be recorded, sample factor and the instrument's sampling frequency).

- **Response:** `<time>`
- **State after `*RST`:** The maximum recording time according to the `*RST` settings for memory recording.
- **Invalidates:** –
- **Invalidated by:** –

```scpi
SWE1:TIME:MAX?
```

**`[SENSe:]SWEep1|2:POINTS?`**

Before the recording has been started or completed, this command returns the max. number of points per function that will be recorded. For synchronized SWEep2 recordings (AVERage) the value is computed as:

```
Configured recording time / nominal averaging interval / sample factor
( SWEep2:TIME? / APER? / SWEep2:SFACtor? )
```

The actual max. number of points recorded depends on the variations in the frequency of the measured signal. Once the recording is complete, the command returns the actual number of points recorded per function.

- **Response:** `<count>`
- **State after `*RST`:** The maximum available number of points. Depends on the amount of memory available in the instrument.
- **Invalidates:** –
- **Invalidated by:** –

```scpi
SWE1:POINTS?
```

**`[SENSe:]SWEep1|2:OFFSet:TIME <value> | MAX`**

Sets the pretrigger length in seconds.

- **Parameters:** `<value> | MAX`. The pretrigger length must be greater than or equal to zero and less than the recording length set with `[SENSe:]SWEep1|2:TIME`.
- **State after `*RST`:** `0.0`
- **Invalidates:** –
- **Invalidated by:** –

```scpi
SWE1:OFFS:TIME 1.0
```

**`[SENSe:]SWEep1|2:OFFSet:POINTS?`**

Before the recording has been started or completed, this command returns the max. number of points per function that will be recorded in the pretrigger. For synchronized SWEep2 recordings (AVERage) the value is computed as:

```
Configured pretrigger time / nominal averaging interval / sample factor
( SWEep2:OFFSet:TIME? / APER? / SWEep2:SFACtor? )
```

The actual max. number of points recorded in the pretrigger depends on the variations in the frequency of the measured signal. Once the recording is complete, the command returns the actual number of points per function recorded in the pretrigger.

- **Parameters:** –
- **Response:** `<count>`
- **State after `*RST`:** `0`
- **Invalidates:** –
- **Invalidated by:** –

```scpi
SWE1:OFFS:POINTS?    -> 0
```

**`[SENSe:]SWEep1|2[:STATe] ON | OFF`**

Enables/disables memory recording of sampled/averaged values. Only one of the sweeps can be enabled at a time. Attempting to enable both sweeps generates the error `-221 Settings conflict`, i.e. SWEep1 recording (REALtime) and SWEep2 recording (AVERage) cannot run at the same time. A transition from OFF to ON clears the memory as if `TRACe:DELete:ALL` had been executed. A transition from ON to OFF resets all settings related to memory recording, except for triggers.

- **Parameters:**
  - `ON` – Enables memory recording.
  - `OFF` – Disables memory recording.
- **State after `*RST`:** `OFF`
- **Invalidates:** –
- **Invalidated by:** –

```scpi
SWE1 ON
SWE1?                -> 1
```

**`[SENSe:]SWEep1|2:COUNt <count>`**

The number of blocks to be recorded.

- **Parameters:** `1 to 65535` (currently only 1 is valid).
- **State after `*RST`:** `1`
- **Invalidates:** –
- **Invalidated by:** –

```scpi
SWE1:COUN 1
SWE1:COUN?           -> 1
```

**`[SENSe:]SWEep1|2:SFACtor <value>`**

Sets the sample factor. It states that every nth value produced by the `[SENSe:]SWEep1|2` block is stored in memory. If the sample factor given would produce a time between two stored samples greater than `SENSe:SWEep1|2:TIME` or `SENSe:SWEep1|2:OFFSet:TIME`, the error `-221 Settings conflict` is generated.

- **Parameters:** `1 to 65535`. When the sample factor is set to 1, all samples are stored.
- **State after `*RST`:** `1`
- **Invalidates:** –
- **Invalidated by:** –

```scpi
SWE1:SFAC 1
```

**`[SENSe:]SWEep1|2:FUNCtion <function>{,<function>}`**

Sets the function list for memory recording. The maximum number of functions is 20. If the list exceeds the unit's capacity, the sample factor is increased automatically.

For the SWEep1 function list (REALtime) only sampled values can be given. For the SWEep2 function list (AVERage) all functions from the instrument's standard function list can be given.

The function list is stored in a saved configuration and is loaded again at PowerOn or with `*RCL`.

- **Parameters:** `<function>`
  - Valid functions for SWEep1 (REALtime):
    - `VOLTage1..6[:DC]`
    - `CURRent1..6[:DC]`
    - `POWer1..6[:ACTive]`
    - `TORQue[1..4]`
    - `SPEed[1..4]`
    - `POWer[1..4]:MECHanical`
  - Valid functions for SWEep2 (AVERage): all functions from the instrument's standard function list.
- **State after `*RST`:** `"VOLTage1"`
- **Invalidates:** –
- **Invalidated by:** –

```scpi
SWE1:FUNC "VOLT1","VOLT2","VOLT3"
```

### The SENSe2 subsystem (requires the Process Interface option)

> **Note:** The whole SENSe2 subsystem applies only to instruments with the **Process Interface** option installed.

The SENSe2 subsystem controls the settings for the inputs of the optional Process Interface. Numeric suffixes on the `TORQue|SPEed|POLepairs|TYPe|REFerence` nodes correspond to the index of the 4 supported motors/generators. The channels of the INPut subsystem are combined into a drive index in the `SENSe2:xxx` subsystem as shown in the table below.

| Input channel (suffix)                    | Drive index | Suffix in the SENSe2 subsystem |
|-------------------------------------------|-------------|--------------------------------|
| INPut21 (torque), INPut25 (speed)          | 1 | `SENSe2:TORQue|SPEed|POLepairs|TYPe|REFerence1[:POWer]` |
| INPut22 (torque), INPut26 (speed)          | 2 | `SENSe2:TORQue|SPEed|POLepairs|TYPe|REFerence2[:POWer]` |
| INPut23 (torque), INPut27 (speed)          | 3 | `SENSe2:TORQue|SPEed|POLepairs|TYPe|REFerence3[:POWer]` |
| INPut24 (torque), INPut28 (speed)          | 4 | `SENSe2:TORQue|SPEed|POLepairs|TYPe|REFerence4[:POWer]` |

| Command | Parameter | Default value/unit | Note |
|---|---|---|---|
| `:INPut[21..28]` | | | |
| `  :TYPe` | VOLTage \| FREQuency | FREQuency | Analog/digital sensor |

**`INPut[21..28]:TYPe VOLTage | FREQuency`**

Selects the type of signal measured on a Process Interface input.

- **Parameters:**
  - `VOLTage` – The input signal is a voltage.
  - `FREQuency` – The input signal is a frequency.
- **State after `*RST`:** `FREQuency` for all Process Interface inputs.
- **Invalidates:** –
- **Invalidated by:** –

```scpi
INP21:TYP VOLT
INP25:TYP?           -> FREQ
```

The SENSe2 node separates the mechanical system from the electrical one (SENSe1, for the electrical system, is the default node at the root level of the command tree). The `SENSe2:xxx:VOLTage` nodes apply when the corresponding input type is set to `INPutx:TYPe VOLTage`, and the `SENSe2:xxx:FREQuency` nodes apply when the corresponding input type is set to `INPutx:TYPe FREQuency`.

#### Command overview – torque (TORQue)

| Command | Parameter | Default value/unit | Note |
|---|---|---|---|
| `:SENSe2` | | | |
| `  :TORQue[1..4]` | | | |
| `    :VOLTage` | | | |
| `      :SCALe` | -1e6 to 1e6 | 1 [Nm/V] | Analog torque sensor |
| `      :OFFSet` | | | |
| `        [:VALue]` | -1e6 to 1e6 | 0 [V] | Input voltage for 0 [Nm] |
| `        :IMMediate` | | | Set offset from input value; no query |
| `    :FREQuency` | | | |
| `      :SCALe` | -1e6 to 1e6 | [Nm/Hz] | Digital torque sensor |
| `      :OFFSet` | | | |
| `        [:VALue]` | -1e6 to 1e6 | 10000 [Hz] | Input frequency for 0 [Nm] |
| `        :IMMediate` | | | Set offset from input value; no query |

#### Command overview – speed (SPEed) and drive

| Command | Parameter | Default value/unit | Note |
|---|---|---|---|
| `:SENSe2` | | | |
| `  :SPEed[1..4]` | | | |
| `    :VOLTage` | | | |
| `      :SCALe` | | | |
| `        [:DEFault]` | -1e6 to 1e6 | 1 [rpm/V] | Analog speed sensor |
| `      :OFFSet` | | | |
| `        [:VALue]` | -1e6 to 1e6 | 0 [V] | Input voltage for 0 [rpm] |
| `        :IMMediate` | | | Set offset from input value; no query |
| `    :FREQuency` | | | |
| `      :SCALe` | | | |
| `        [:DEFault]` | -1e6 to 1e6 | 60 [rpm/Hz] | Digital speed sensor |
| `        :PULSe` | 1 to 100000 | 1 [pul/rev] | Alternative setting |
| `      :OFFSet` | | | |
| `        [:VALue]` | -1e6 to 1e6 | 0 [Hz] | Input frequency for 0 [rpm] |
| `        :IMMediate` | | | Set offset from input value; no query |
| `  :TYPe[1..4]` | MOTor \| GENerator | MOTor | |
| `  :POLepairs[1..4]` | 1 to 999 | 1 | |
| `  :REFerence[1..4]` | | | |
| `    [:POWer]` | "POWer[1..6][:ACTive]" | "POWer" | For efficiency computation |

#### Scaling – torque and speed

**`SENSe2:TORQue[1..4]:VOLTage:SCALe <value>`**

Sets the torque scaling factor for a voltage-type input, reflecting the transformation ratio of the torque sensors used. The difference between the voltage at the input in question and the offset value given is multiplied by this scaling factor before all further processing.

- **Parameters:** `-1.0e6 to 1.0e6` [Nm/V]
- **State after `*RST`:** `1.0`
- **Invalidates:** –
- **Invalidated by:** –

```scpi
SENS2:TORQ3:VOLT:SCAL 10.0
SENS2:TORQ1:VOLT:SCAL?   -> 25.0
```

**`SENSe2:TORQue[1..4]:VOLTage:OFFSet[:VALue] <value>`**

Sets the input voltage corresponding to zero torque. This voltage is subtracted from the voltage measured at the input before the difference is multiplied by the scaling factor.

- **Parameters:** `-1.0e6 to 1.0e6` [V]
- **State after `*RST`:** `0.0`
- **Invalidates:** –
- **Invalidated by:** `SENSe2:TORQue[1..4]:VOLTage:OFFSet:IMMediate`

```scpi
SENS2:TORQ3:VOLT:OFFS 0.0
SENS2:TORQ2:VOLT:OFFS?   -> 0.0
```

**`SENSe2:TORQue[1..4]:VOLTage:OFFSet:IMMediate`**

Sets the offset value to the torque voltage currently being measured. The measurement must be valid (no overload).

- **Parameters:** –
- **State after `*RST`:** –
- **Invalidates:** `SENSe2:TORQue[1..4]:VOLTage:OFFSet`
- **Invalidated by:** –

```scpi
SENS2:TORQ3:VOLT:OFFS:IMM
```

**`SENSe2:TORQue[1..4]:FREQuency:SCALe <value>`**

Sets the torque scaling factor for a frequency-type input, reflecting the transformation ratio of the torque sensors used. The difference between the frequency at the input in question and the offset value given is multiplied by this scaling factor before all further processing.

- **Parameters:** `-1.0e6 to 1.0e6` [rpm/Hz] *(the unit as given in the manual; the command overview states [Nm/Hz] for the torque scale)*
- **State after `*RST`:** `1.0`
- **Invalidates:** –
- **Invalidated by:** –

```scpi
SENS2:TORQ2:FREQ:SCAL 0.001
SENS2:TORQ1:FREQ:SCAL?   -> 0.001
```

**`SENSe2:TORQue[1..4]:FREQuency:OFFSet[:VALue] <value>`**

Sets the input frequency corresponding to zero torque. This frequency is subtracted from the frequency measured at the input before the difference is multiplied by the scaling factor.

- **Parameters:** `-1.0e6 to 1.0e6` [Hz]
- **State after `*RST`:** `0.0`
- **Invalidates:** –
- **Invalidated by:** `SENSe2:TORQue[1..4]:FREQuency:OFFSet:IMMediate`

```scpi
SENS2:TORQ3:FREQ:OFFS 1000.0
SENS2:TORQ2:FREQ:OFFS?   -> 1000.0
```

**`SENSe2:TORQue[1..4]:FREQuency:OFFSet:IMMediate`**

Sets the torque frequency offset value from the value currently being measured. The measurement must be valid (no overload / undefined value).

- **Parameters:** –
- **State after `*RST`:** –
- **Invalidates:** `SENSe2:TORQue[1..4]:FREQuency:OFFSet`
- **Invalidated by:** –

```scpi
SENS2:TORQ3:FREQ:OFFS:IMM
```

**`SENSe2:SPEed[1..4]:VOLTage:SCALe[:DEFault] <value>`**

Sets the speed scaling factor for a voltage-type input, reflecting the transformation ratio of the speed sensors used. The difference between the voltage at the input in question and the offset value given is multiplied by this scaling factor before all further processing.

- **Parameters:** `-1.0e6 to 1.0e6` [Nm/V] *(the unit as given in the manual; the command overview states [rpm/V] for the speed scale)*
- **State after `*RST`:** `1.0`
- **Invalidates:** –
- **Invalidated by:** –

```scpi
SENS2:SPE3:VOLT:SCAL 10.0
SENS2:SPE1:VOLT:SCAL?    -> 25.0
```

**`SENSe2:SPEed[1..4]:VOLTage:OFFSet[:VALue] <value>`**

Sets the input voltage corresponding to zero speed. This voltage is subtracted from the voltage measured at the input before the difference is multiplied by the scaling factor.

- **Parameters:** `-1.0e6 to 1.0e6` [V]
- **State after `*RST`:** `0.0`
- **Invalidates:** –
- **Invalidated by:** `SENSe2:SPEed[1..4]:VOLTage:OFFSet:IMMediate`

```scpi
SENS2:SPE3:VOLT:OFFS 0.0
SENS2:SPE2:VOLT:OFFS?    -> 0.0
```

**`SENSe2:SPEed[1..4]:VOLTage:OFFSet:IMMediate`**

Sets the offset value to the speed voltage currently being measured. The measurement must be valid (no overload).

- **Parameters:** –
- **State after `*RST`:** –
- **Invalidates:** `SENSe2:SPEed[1..4]:VOLTage:OFFSet[:VALue]`
- **Invalidated by:** –

```scpi
SENS2:SPEed3:VOLT:OFFS:IMM
```

**`SENSe2:SPEed[1..4]:FREQuency:SCALe[:DEFault] <value>`**

Sets the speed scaling factor for a frequency-type input, reflecting the transformation ratio of the speed sensors used. The difference between the frequency at the input in question and the offset value given is multiplied by this scaling factor before all further processing.

- **Parameters:** `-1.0e6 to 1.0e6` [rpm/Hz]
- **State after `*RST`:** `1.0`
- **Invalidates:** `SENSe2:SPEed[1..4]:FREQuency:SCALe:PULS`
- **Invalidated by:** `SENSe2:SPEed[1..4]:FREQuency:SCALe:PULS`

```scpi
SENS2:SPE2:FREQ:SCAL 0.001
SENS2:SPE1:FREQ:SCAL?    -> 0.001
```

**`SENSe2:SPEed[1..4]:FREQuency:SCALe:PULSe <value>`**

Sets the speed scaling factor for a frequency-type input, reflecting the transformation ratio of the speed sensors used. This alternative method makes it possible to pass the specification of a digital speed sensor directly to the unit. The corresponding offset value should be set to zero.

- **Parameters:** `1 to 100000` [pulses/revolution]
- **State after `*RST`:** `1`
- **Invalidates:** `SENSe2:SPEed[1..4]:FREQuency:SCALe[:DEFault]`
- **Invalidated by:** `SENSe2:SPEed[1..4]:FREQuency:SCALe[:DEFault]`

```scpi
SENS2:SPE2:FREQ:SCAL:PULS 1024
SENS2:SPE1:FREQ:SCAL:PULS?   -> 256
```

**`SENSe2:SPEed[1..4]:FREQuency:OFFSet[:VALue] <value>`**

Sets the input frequency corresponding to zero speed. This frequency is subtracted from the frequency measured at the input before the difference is multiplied by the scaling factor.

- **Parameters:** `-1.0e6 to 1.0e6` [Hz]
- **State after `*RST`:** `0.0`
- **Invalidates:** –
- **Invalidated by:** `SENSe2:SPEed[1..4]:FREQuency:OFFSet:IMMediate`

```scpi
SENS2:SPE3:FREQ:OFFS 1000.0
SENS2:SPE2:FREQ:OFFS?    -> 1000.0
```

**`SENSe2:SPEed[1..4]:FREQuency:OFFSet:IMMediate`**

Sets the speed frequency offset value from the value currently being measured. The measurement must be valid (no overload / undefined value).

- **State after `*RST`:** –
- **Invalidates:** `SENSe2:SPEed[1..4]:FREQuency:OFFSet[:VALue]`
- **Invalidated by:** –

```scpi
SENS2:TORQ3:FREQ:OFFS:IMM
```

*(The example is reproduced as it stands in the manual.)*

#### Drive settings

**`SENSe2:TYPe[1..4] MOTor | GENerator`**

Sets the type of drive used. The setting affects the computation of slip and efficiency.

- **Parameters:**
  - `MOTor` – Drive type set to motor.
  - `GENerator` – Drive type set to generator.
- **State after `*RST`:** `MOT`
- **Invalidates:** –
- **Invalidated by:** –

```scpi
SENS2:TYP1 MOT
SENS2:TYP3?          -> GEN
```

**`SENSe2:POLepairs[1..4] <value>`**

Sets the number of pole pairs for the drive. The setting is used for the computation of slip.

- **Parameters:** `<value>`, valid range: `1 to 999`
- **State after `*RST`:** `1`
- **Invalidates:** –
- **Invalidated by:** –

```scpi
SENS2:POL3 2
SENS2:POL1?          -> 1
```

**`SENSe2:REFerence[1..4][:POWer] <function>`**

Sets which measured electrical power is used for the efficiency computation.

- **Parameters:** `<function>` may be any of the averaged active powers the instrument measures: `"POWer[1..6|460][:ACTive]"`
- **State after `*RST`:** `"POW"`
- **Invalidates:** –
- **Invalidated by:** –

```scpi
SENS2:REF3 "POW1"
SENS2:REF2?          -> "POW"
```

### The SOURce subsystem (requires the Process Interface option)

> **Note:** The whole SOURce subsystem applies only to instruments with the **Process Interface** option installed.

The SOURce subsystem controls the settings for the analog outputs of the optional Process Interface. Numeric suffixes on the VOLTage node correspond to the index of the 4 supported outputs.

#### Command overview (SOURce)

| Command | Parameter | Default value/unit | Note |
|---|---|---|---|
| `:SOURce` | | | |
| `  :VOLTage[1..4]` | | | |
| `    [:LEVel]` | | | |
| `      [:IMMediate]` | | | |
| `        [:AMPLitude]` | -10.3 to 10.3 | 0.0 V | FIXed mode only |
| `    :MODE` | FIXed \| VARiable | FIXed | |
| `    :FEED` | \<function\> | VOLTage1 | VARiable mode only |
| `    :GAIN` | -1.0e6 to 1.0e6 | 1.0 V/Ref unit | |
| `    :ZERO` | -1.0e6 to 1.0e6 | 0.0 Ref unit | |

#### Output configuration

**`SOURce:VOLTage[1..4]:MODE FIXed | VARiable`**

Selects the operating mode for the analog outputs.

- **Parameters:**
  - `FIXed` – The output voltage is set directly with the command `SOURce:VOLTage[1..4][:LEVel][:IMMediate][:AMPLitude]`.
  - `VARiable` – After each measurement, the output voltage is computed from the measurement function selected with FEED, using the GAIN and ZERO values given.
- **State after `*RST`:** `FIXed`
- **Invalidates:** –
- **Invalidated by:** –

```scpi
SOUR:VOLT3:MODE VAR
SOUR:VOLT2:MODE?     -> FIX
```

**`SOURce:VOLTage[1..4][:LEVel][:IMMediate][:AMPLitude] <value>`**

Selects the output voltage for FIXed mode.

- **Parameters:** `<value>`, valid range: `-10.3 to 10.3 V`
- **State after `*RST`:** `0.0`
- **Invalidates:** –
- **Invalidated by:** –

```scpi
SOUR:VOLT4 5.3
SOUR:VOLT1?          -> -2.5
```

**`SOURce:VOLTage[1..4]:FEED <function>`**

Sets the reference function for the output in VARiable mode.

- **Parameters:** `<function>` – any valid averaged measurement function in the instrument.
- **State after `*RST`:** `"VOLTage1"`
- **Invalidates:** –
- **Invalidated by:** –

```scpi
SOUR:VOLT2:FEED "POW2:APP"
SOUR:VOLT4:FEED?     -> "CURR3:MEAN"
```

#### Output scaling

**`SOURce:VOLTage[1..4]:GAIN <value>`**

Sets the scaling for the output. The difference between the current value of the reference function and the ZERO value is multiplied by this factor to compute the output voltage.

- **Parameters:** `<gain>`, valid range: `-1.0e6 to 1.0e6 V/Ref unit`
- **State after `*RST`:** `1.0`
- **Invalidates:** –
- **Invalidated by:** –

```scpi
SOUR:VOLT2:GAIN 5.0
SOUR:VOLT3:GAIN?     -> 1.0e-3
```

**`SOURce:VOLTage[1..4]:ZERO <value>`**

Sets the offset for the output. This value is subtracted from the current value of the reference function before the difference is multiplied by the GAIN setting to compute the output voltage.

- **Parameters:** `<value>`, valid range: `-1.0e6 to 1.0e6 Ref unit`
- **State after `*RST`:** `0.0`
- **Invalidates:** –
- **Invalidated by:** –

```scpi
SOUR:VOLT1:ZERO 225.0
SOUR:VOLT3:ZERO?     -> 50.0
```

## Subsystems: SYNC through STATus

### The SYNC subsystem

The SYNC subsystem controls the instrument's synchronization capability. When synchronization is enabled, the instrument adapts the averaging cycles to the frequency of the signal fed to the synchronization source. If memory recording of sampled data is in progress, the synchronization signal can be used as a special form of triggering.

#### Command overview (SYNC)

| Command | Parameter | Default value/unit | Note |
|---|---|---|---|
| `SYNC:STATe` | ON \| OFF | ON | |
| `SYNC:LEVel:UNIT` | ABSolute \| PCT | PCT | |
| `SYNC[:SOURce]\|VOLTage[1..6]\|CURRent[1..6]` | VOLTage[1..6] \| CURRent[1..6] \| EXTernal | VOLTage1 | SOURce = the current sync source. \<currently only the SOURce node is implemented\> |
| `SYNC...:LEVel` | -150 %...150 % of range | 0.0 | Not for EXTernal |
| `SYNC...:SLOPe` | POSitive \| NEGative | POSitive | |
| `SYNC...:FILTer[:LPASs][:STATe]` | ON \| OFF | OFF | Not for the EXTernal source |
| `SYNC...:FILTer[:LPASs]:FREQuency` | 1.0e2, 1.0e3, 1.0e4 | 1.0e4 Hz | Not for the EXTernal source |
| `SYNC:TIMeout` | 0.015 to 3600.0 | 0.3 s | |

#### `SYNC:STATe ON | OFF`

**Description:** States whether the averaging interval is to be controlled by the signal frequency on the selected input or not. If synchronization is switched on, the actual averaging period is held to the first integer multiple of the sync signal that is greater than the user-specified nominal averaging period. If synchronization is switched off, the actual averaging period equals the user-specified nominal averaging period rounded to an integer multiple of sample periods.

**Parameters:**

| Parameter | Meaning |
|---|---|
| ON | Synchronization is required. The instrument will always try to synchronize to the frequency of the sync source signal. |
| OFF | Synchronization is disabled. Use this option for measurements on DC signals. |

**Example:**

```
SYNC:STAT ON
SYNC:STAT?          Response: 1
```

- **\*RST state:** ON

#### `SYNC:LEVel:UNIT ABSolute | PCT`

**Description:** Sets the unit for the command `SYNC[:SOURce]|VOLTage[1..6]|CURRent[1..6]:LEVel`.

**Parameters:**

| Parameter | Meaning |
|---|---|
| ABSolute | The level is given in absolute units. |
| PCT | The level is given as a percentage of the nominal input range. |

**Example:**

```
SYNC:LEV:UNIT ABS
SYNC:LEVel:UNIT?    Response: PCT
```

- **\*RST state:** PCT
- **Invalidates:** `SYNC:LEVel:UNIT`

#### `SYNC[:SOURce] VOLTage[1..6] | CURRent[1..6] | EXTernal`

**Description:** Selects the signal source for synchronization and frequency measurement.

**Parameters:**

| Parameter | Meaning |
|---|---|
| VOLTage[1..6] | One of the voltage channels is the sync source. |
| CURRent[1..6] | One of the current channels is the sync source. |
| EXTernal | The external TTL sync input is the sync source. |

**Example:**

```
SYNC:SOUR VOLT1
SYNC:SOUR?          Response: VOLT1
```

- **\*RST state:** VOLTage1
- **Invalidates:** `SYNC[:SOUR]:AUTO`
- **Invalidated by:** `SYNC[:SOUR]:AUTO`

#### `SYNC[:SOURce]|VOLTage[1..6]|CURRent[1..6]:LEVel <level>`

**Description:** Sets the sync level at which the period of the selected input signal is measured by the instrument's synchronization circuits. `SYNC:SOURce:LEVel` sets the sync level of the active trigger source (not for EXTernal). \<currently only the SOURce node is implemented\>

**Parameters:**

| Parameter | Meaning |
|---|---|
| `<level>` | Valid range: -150 % to 150 % of the nominal input range on the specified channel, in IEEE 488.2 \<NON-DECIMAL NUMERIC PROGRAM DATA\> format. The unit is selected with the `SYNC:LEVel:UNIT` command. |

**Example:**

```
SYNC:VOLT1:LEV 10.0
SYNC:VOLT1:LEV?     Response: 0.0
```

- **\*RST state:** 0.0
- **Invalidates:** `SYNC[:SOURce]:VOLTage[1..6]|CURRent[1..6]:LEVel:AUTO`
- **Invalidated by:** `SYNC[:SOURce]:VOLTage[1..6]|CURRent[1..6]:LEVel:AUTO`, `[SENSe:]VOLTage[1..6]|CURRent[1..6]:AC[:|DC]:RANGe[:UPPer]`, `INPut[1|2|3|4|5|6|7|8|9|10|11|12]:SHUNt`, `INPut[1|2|3|4|5|6|7|8|9|10|11|12]:GAIN`, `SYNC:LEVel:UNIT`

#### `SYNC[:SOURce]|VOLTage[1..6]|CURRent[1..6]:SLOPe POSitive | NEGative`

**Description:** Sets the active edge for the synchronization signal. \<currently only the SOURce node is implemented\>

**Parameters:**

| Parameter | Meaning |
|---|---|
| POSitive | The instrument synchronizes on the positive edge of the synchronization signal. |
| NEGative | The instrument synchronizes on the negative edge of the synchronization signal. |

**Example:**

```
SYNC:SLOP POS
SYNC:SLOP?          Response: POS
```

- **\*RST state:** POSitive

#### `SYNC[:SOURce]|VOLTage[1..6]|CURRent[1..6]:FILTer:[LPASs[:STATe]] ON | OFF`

**Description:** Controls the filter for the synchronization signal. The filtering is applied to the signal on the input channel selected as the synchronization source. This command has no effect if the selected sync source is EXTernal. \<currently only the SOURce node is implemented\>

**Parameters:**

| Parameter | Meaning |
|---|---|
| ON | Filter is ON. |
| OFF | Filter is OFF. |

**Example:**

```
SYNC:FILT ON
SYNC:FILT?          Response: 0
```

- **\*RST state:** OFF

#### `SYNC[:SOURce]|VOLTage[1..6]|CURRent[1..6]:FILTer:[LPASs]:FREQuency 10.0e3 | 1.0e3 | 100.0`

**Description:** Sets the low-pass frequency for the synchronization signal filter. This command has no effect if the selected sync source is EXTernal. The frequency unit is Hz. \<currently only the SOURce node is implemented\>

**Parameters:**

| Parameter | Meaning |
|---|---|
| 10.0e3 | 10 kHz |
| 1.0e3 | 1 kHz |
| 100.0 | 100 Hz |

Any other value between 100 Hz and 10 kHz is coerced to the nearest higher exact value.

**Example:**

```
SYNC:FILT:FREQ 100.0
SYNC:FILT:FREQ?     Response: 1000.0
```

- **\*RST state:** 10000.0

#### `SYNC:TIMeout <timeout>`

**Description:** Sets the synchronization timeout in seconds. The instrument starts averaging after the timeout if no sync signal is available. The timeout is only active when synchronization is on. If the nominal averaging interval is changed with the command `[SENSe:]{CURRent[1..6]|VOLTage[1..6]|[POWer]}:{AC|[DC]}:APERture[:TIME]`, the synchronization timeout is set to the nominal averaging interval or 0.3 seconds, whichever is greater.

**Parameters:**

| Parameter | Meaning |
|---|---|
| `<timeout>` | 0.015 to 3600 s |

**Example:**

```
SYNC:TIMeout 5.0
SYNC:TIMeout?       Response: 5.0
```

- **\*RST state:** 0.3
- **Invalidated by:** `[SENSe:]{CURRent[1..6]|VOLTage[1..6]|[POWer]}:{AC|[DC]}:APERture[:TIME]`

### The TIMer subsystem

The TIMer subsystem contains commands for controlling the instrument's internal timer. This timer provides timestamp information for averaged measurements.

#### Command overview (TIMer)

| Command | Parameter | Default value/unit | Note |
|---|---|---|---|
| `TIMer:RESet` | | - | No query |
| `TIMer:RESet:AUTO` | ON \| OFF | ON | \<not yet implemented\> |
| `TIMer:RESet:TIME?` | | | Query only |

#### `TIMer:RESet`

**Description:** Resets the instrument's internal timer. The timer is used to measure memory recording time and the number of averaging cycles. On reset, both the timer's time and the averaging cycle counter are set to zero. The absolute time of the last timer reset can be read with the command `TIMer:RESet:TIME?`. If this command is sent while averaged values are being stored in memory, the time information becomes inconsistent, since the timer starts counting from zero in the middle of the data.

The timer is reset automatically at Power On — `TIMer:RESet:TIME?` then gives the power-on time.

**Example:**

```
TIM:RES
```

- **\*RST state:** No reset condition
- **Invalidates:** `TIM:RES:TIME?`

#### `TIMer:RESet:AUTO ON | OFF`

**Description:** Controls whether the instrument's internal timer is reset automatically on ARMing. To retain an absolute time base for sequenced memory measurements, `TIMer:RESet:AUTO` must be set to OFF, so that subsequent `INITiate[:IMMediate]:NAME:STARt` commands do not reset the timer. \<not yet implemented\>

**Parameters:**

| Parameter | Meaning |
|---|---|
| ON | `INITiate[:IMMediate]:NAME:STARt` resets the timer. |
| OFF | `INITiate[:IMMediate]:NAME:STARt` does not reset the timer. |

**Example:**

```
TIM:RES:AUTO ON
TIM:RES:AUTO?       Response: 1
```

- **\*RST state:** ON
- **Invalidates:** `TIM:RESet:TIME?`

#### `TIMer:RESet:TIME?`

**Description:** Queries the absolute time of the last timer reset.

**Response:**

```
<year>,<month>,<day>,<hours>,<minutes>,<seconds>
```

The year is in four-digit numeric format. Hours are in 24-hour notation.

**Example:**

```
TIM:RES:TIME?
```

- **\*RST state:** Has no reset value

### The TRACe subsystem

The TRACe subsystem contains commands for reading memory recordings.

#### Command overview (TRACe)

| Command | Parameter | Note |
|---|---|---|
| `TRACe[:DATA]:PREamble?` | block | Query only |
| `TRACe[:DATA]?` | \<block\>, \<number_of_points\>, \<offset\>, \<sparsing\> | Query only |
| `TRACe[:DATA]:STATus?` | \<block\>, \<number_of_points\>, \<offset\>, \<sparsing\> | Query only |
| `TRACe:FREE?` | | Query only |
| `TRACe:CATalog:LENgth?` | | Query only |
| `TRACe:DELete:ALL` | | |

#### `TRACe[:DATA]:PREamble? [<block>]`

**Description:** Reads the data header for the given block. If the `<block>` parameter is omitted, all headers are sent.

**Parameters:**

| Parameter | Meaning |
|---|---|
| `<block>` = 1 | This optional parameter states which block in memory the preamble is to be returned for. Currently only one block can be recorded, and the only valid value is 1. |

**Response:**

| Field | Type | Meaning |
|---|---|---|
| `<points_per_func>` | integer | States the amount of data recorded for each function. |
| `<num_of_func>` | integer | The number of functions configured for memory recording. |
| `<trigger_index>` | integer | Index of the data point corresponding to the trigger. |
| `<first_point_time_relative_to_trigger>` | float | The time difference between the first recorded data point and the trigger, in seconds. |
| `<record_duration>` | float | States the time difference between the first and last recorded data point, in seconds. |
| `<trigger_time_relative_to_timer_reset_time>` | float | States the length of the time interval between the timer reset and the trigger, in seconds. The trigger time this value is derived from corresponds to the TIME:RELative function value of the data point immediately preceding the data at index `<trigger_index>` (the TIME:RELative values are timestamps for when the corresponding averaging intervals completed). |
| `<average_time_between_two_points>` | float | States the sample interval in seconds. For SWEep1 (REALtime) recordings and non-synchronized SWEep2 (AVERage) recordings, this value is the exact sample interval. For synchronized SWEep2 (AVERage) recordings, the value is an average sample interval computed as `<record_duration> / <points_per_func>`. The actual interval between individual consecutive samples depends on the variations in the frequency of the measured signal. |

**Example:**

```
TRAC:DATA:PRE?
Response: 0,0,0,0.00000E+00,0.00000E+00,0.00000E+00,0.00000E+00
```

- **\*RST state:** `0,0,0,0.00000E+00,0.00000E+00,0.00000E+00,0.00000E+00`

#### `TRACe[:DATA]? [<block>[,<count>[,<offset>[,<sparsing>]]]]`

**Description:** Reads data from memory.

The default data format is readable ASCii values (`FORMat[:DATA] ASCii, 8`). For better performance you can use binary output with 32-bit wide floating point numbers (`FORMat[:DATA] REAL,32`) and normal (the instrument's native) byte order (`FORMat:BORDer NORMal`).

**Parameters:**

| Parameter | Meaning |
|---|---|
| `<block>` | States the block to be read. Set to 0, all blocks are read. Set to 1, the first acquired block is read, and so on. |
| `<count>` | States the number of points to be read for the given block, starting at the offset index. |
| `<offset>` | States the index of the acquired data point at which the transfer is to start. |
| `<sparsing>` | States that every nth data point is transferred. Set to 1, all points are transferred. Set to 2, every other point is transferred. |

If no parameters are given, all recorded data is transferred. Parameters can be omitted from right to left.

Default values: `<block>=1`, `<count>=all`, `<offset>=0`, `<sparsing>=1`

**Response:**

When `FORMat:TRANspose` is ON, the values are grouped by function:

```
<interval1>,<interval2>,<interval3>,...   <interval1>,<interval2>,<interval3>,...
—— func1 ——                               —— func2 ——
```

When `FORMat:TRANspose` is OFF, the values are grouped by interval:

```
<func1>,<func2>,<func3>,...   <func1>,<func2>,<func3>,...
—— interval1 ——               —— interval2 ——
```

**Example:**

```
TRAC:DATA?          Response: 1.2345E+01,2.3456E+01,....
```

- **\*RST state:** There is no response to this command after reset.

#### `TRACe[:DATA]:STATus? [<block>[,<count>[,<offset>[,<sparsing>]]]]`

**Description:** Reads data and status from memory.

The data is returned first, followed by the status information.

The measurement status information states the validity of the measurement. The status information is appended after the set of measured values. The number of status values equals the number of measurement values returned. The format of the status information is controlled by the `FORMat:STATus` commands.

For best performance, use binary output with 32-bit wide floating point numbers (`FORMat[:DATA] REAL,32`), normal (the instrument's native) byte order (`FORMat:BORDer NORMal`) and 8-bit wide integer status values (`FORMat[:DATA]:STATus INT,8`).

**Parameters:** See `TRACe[:DATA]?`.

**Status values:**

The status values returned are integers. The measurement status values are appended at the end of the measurement results. The status value is a bitmask (an integer) and may be a combination of one or more of the following values (bits) combined with a logical OR:

| Value | Name | Meaning |
|---|---|---|
| 0 | Normal | Valid measurement, no questionable condition. |
| 1 | Underrange | The value returned is valid, but the signal amplitude is too low for the given range, so measurement precision is reduced. |
| 2 | Overrange | The instrument returns a measurement value, but the input signal amplitude is too high for the given range. This causes the input signal to be clipped to an amplitude within the current range. Because the measurement value is computed from clipped sample data, the value returned may be more or less outside the specification. |
| 8 | Undefined | The instrument could not compute a valid value. This may be caused, for example, by loss of synchronization (no valid frequency, harmonics, ...). The instrument returns Not A Number for the measurement. |
| 16 | Not available | The requested function is not, or no longer, available (e.g. option not installed, function switched off). The instrument returns Not A Number for the measurement. |
| 128 | Power Factor capacitive | For the power factor function, this indicates a capacitive phase difference between voltage and current (0 = inductive). |

**Response:**

When `FORMat:TRANspose` is ON, the values are grouped by function:

```
<interv1>,<interv2>,...  <interv1>,<interv2>,...     <interv1>,<interv2>,...  <interv1>,<interv2>,...
— func1 —                — func2 —                   — func1 —                — func2 — ...
—————————— data ——————————                           —————————— status ——————————
```

When `FORMat:TRANspose` is OFF, the values are grouped by interval:

```
<func1>,<func2>,...  <func1>,<func2>,...     <func1>,<func2>,...  <func1>,<func2>,...
— interval1 —        — interval2 —           — interval1 —        — interval2 — ...
—————————— data ——————————                   —————————— status ——————————
```

**Example:**

```
TRAC:DATA:STAT?     Response: 1.2345E+01,2.3456E+01,....,0,0,...
```

- **\*RST state:** There is no response to this command after reset.

#### `TRACe:FREE?`

**Description:** Returns the number of free bytes in memory.

**Response:**

```
<bytes>
```

**Example:**

```
TRAC:FREE?          Response: 4194176
```

- **\*RST state:** Returns the maximum available memory if no data has been recorded. This value is instrument dependent.

#### `TRACe:CATalog:LENgth?`

**Description:** Returns the actual number of blocks acquired in memory (only 1 is returned).

**Response:**

```
<number_of_blocks>
```

**Example:**

```
TRAC:CAT:LEN?       Response: 1
```

- **\*RST state:** 1

#### `TRACe:DELete:ALL`

**Description:** Deletes all memory.
**Example:**

```
TRAC:DEL:ALL
```

- **\*RST state:** This is an event and has no reset state.
- **Invalidates:** `TRACe:FREE?`

### The TRIGger subsystem

The TRIGger subsystem contains commands for defining the condition on an averaged measurement that is to trigger an action. The TRIGger subsystem only has an effect if memory recording is enabled.

#### Command overview (TRIGger)

| Command | Parameter | Default value/unit |
|---|---|---|
| `TRIGger:STARt:SOURce` | BUS \| TIME \| IMMediate \| MANual \| SYNC \| \<function\> | IMMediate |
| `TRIGger:STARt:TIME` | yyyy,mm,dd,hh,mm,ss | |
| `TRIGger:STARt:LEVel` | value | 0.0 |
| `TRIGger:STARt:SLOPe` | POSitive \| NEGative | POSitive |
| `TRIGger:STOP:SOURce` | TIME \| IMMediate \| MANual \| \<function\> | IMMediate |
| `TRIGger:STOP:TIME` | yyyy,mm,dd,hh,mm,ss | |
| `TRIGger:STOP:LEVel` | \<value\> | 0.0 |
| `TRIGger:STOP:SLOPe` | POSitive \| NEGative | POSitive |

#### `TRIGger:STARt:SOURce BUS | TIME | IMMediate | MANual | SYNC | <function>`

**Description:** Sets the start trigger source.

**Parameters:**

| Parameter | Meaning |
|---|---|
| BUS | Triggers when a `*TRG` command is received. |
| TIME | Triggers at an exact point in time. |
| IMMediate | No waiting for an event. |
| MANual | The signal is generated by the user by pressing the front panel "MEM" key. |
| SYNC | The trigger occurs every time an edge of the synchronization signal is detected. This source is only valid for the REALtime sweep (to use the EXTernal signal connector as a trigger, the SYNC source must be set to EXTernal). |
| `<function>` | A condition on an averaged measurement function triggers. |

**Example:**

```
TRIG:STAR:SOUR IMM
TRIG:STAR:SOUR?     Response: IMM
```

- **\*RST state:** IMM

#### `TRIGger:STARt:TIME <yyyy,MM,dd,hh,mm,ss>`

**Description:** The memory recording starts when the instrument's internal time reaches the value given.

**Parameters:**

| Parameter | Meaning |
|---|---|
| yyyy | Year |
| MM | Month |
| dd | Day |
| hh | Hours in 24-hour notation |
| mm | Minutes |
| ss | Seconds (integer value) |

**Example:**

```
TRIG:STAR:TIME 2002,01,01,11,00,00
TRIG:STAR:TIME?     Response: 2002,01,01,11,00,00
```

- **\*RST state:** 1970,1,1,0,0,0

#### `TRIGger:STARt:LEVel <level>`

**Description:** When the start source for recording is an averaged measurement function, this setting states the measurement function level that is to trigger the recording.

**Parameters:**

| Parameter | Meaning |
|---|---|
| `<level>` | The range for this setting is not defined. |

**Example:**

```
TRIG:STARt:LEV 50.0
TRIG:STARt:LEV?     Response: 25.0
```

- **\*RST state:** 0.0

#### `TRIGger:STARt:SLOPe POSitive | NEGative`

**Description:** When the source for recording is an averaged measurement function, this setting states the edge. (The manual refers to the stop source here; the command applies to the start trigger.)

**Parameters:**

| Parameter | Meaning |
|---|---|
| POSitive | Trigger on the positive edge. |
| NEGative | Trigger on the negative edge. |

**Example:**

```
TRIG:STAR:SLOP POS
TRIG:STAR:SLOP?     Response: POS
```

- **\*RST state:** POS

#### `TRIGger:STOP:SOURce TIME | IMMediate | MANual | <function>`

**Description:** The stop trigger source. Acquisition stops either at the date/time given, when memory is full, when the recording time is reached, or when the stop condition selected below is met — whichever occurs first.

**Parameters:**

| Parameter | Meaning |
|---|---|
| TIME | Acquisition stops either when memory is full, when the recording time is reached, or at the date/time given — whichever occurs first. |
| IMMediate | No additional stop condition is set. Acquisition stops either when memory is full or when the recording time is reached — whichever occurs first. |
| MANual | Acquisition stops either when memory is full, when the recording time is reached, or when the front panel MEM key is pressed — whichever occurs first. To stop the memory recording immediately, switch off the memory subsystem. |
| `<function>` | Acquisition stops either when memory is full, when the recording time is reached, or when the condition on the selected function is met — whichever occurs first. |

**Example:**

```
TRIG:STOP:SOUR MAN
TRIG:STOP:SOUR?     Response: MAN
```

- **\*RST state:** MAN

#### `TRIGger:STOP:TIME <yyyy,MM,dd,hh,mm,ss>`

**Description:** The memory recording stops when the instrument's internal time reaches the value given.

**Parameters:**

| Parameter | Meaning |
|---|---|
| yyyy | Year |
| MM | Month |
| dd | Day |
| hh | Hours in 24-hour notation |
| mm | Minutes |
| ss | Seconds (integer value) |

**Example:**

```
TRIG:STOP:TIME 2002,01,01,11,00,00
TRIG:STOP:TIME?     Response: 2002,01,01,11,00,00
```

- **\*RST state:** 1970,1,1,0,0,0

#### `TRIGger:STOP:LEVel <level>`

**Description:** When the stop source for recording is an averaged measurement function, this setting states the measurement function level that is to stop the recording.

**Parameters:**

| Parameter | Meaning |
|---|---|
| `<level>` | The range for this setting is not defined. |

**Example:**

```
TRIG:STOP:LEV 50.0
TRIG:STOP:LEV?      Response: 25.0
```

- **\*RST state:** 0.0

#### `TRIGger:STOP:SLOPe POSitive | NEGative`

**Description:** When the stop source for recording is an averaged measurement function, this setting states the edge.

**Parameters:**

| Parameter | Meaning |
|---|---|
| POSitive | Stops recording on the positive edge. |
| NEGative | Stops recording on the negative edge. |

**Example:**

```
TRIG:STOP:SLOP POS
TRIG:STOP:SLOP?     Response: POS
```

- **\*RST state:** POS

### The SYSTem subsystem

This subsystem collects a number of commands for general functions not directly related to power analysis. Among other things, the communication settings (`SYSTem:COMMunicate...`) are found here.

#### Command overview (SYSTem)

| Command | Parameter | Default value/unit | Note |
|---|---|---|---|
| `SYSTem:COMMunicate:GPIB[:SELF]:ADDRess` | 1 to 30 | 5 | |
| `SYSTem:COMMunicate:SERial:BAUD` | 1200 \| 2400 \| 4800 \| 9600 \| 19200 \| 38400 \| 57600 \| 115200 | 115200 bd | |
| `SYSTem:COMMunicate:SERial:BITS` | 7 \| 8 | 8 bits | \<not yet implemented\> |
| `SYSTem:COMMunicate:SERial:SBITs` | 1 \| 2 | 1 bit | \<not yet implemented\> |
| `SYSTem:COMMunicate:SERial:CONTrol:RTS` | ON \| IBFull \| RFR | RFR | \<not yet implemented\> |
| `SYSTem:COMMunicate:SERial:PACE` | XON \| NONE | NONE | \<not yet implemented\> |
| `SYSTem:COMMunicate:SERial:PARity` | EVEN \| ODD \| ZERO \| ONE \| NONE \| IGNore | NONE | \<not yet implemented\> |
| `SYSTem:DATE` | Year,month,day | | |
| `SYSTem:TIME` | Hours,minutes,seconds | | |
| `SYSTem:ERRor[:NEXT]?` | | | Query only |
| `SYSTem:ERRor:ALL?` | | | Query only |
| `SYSTem:KLOCk` | ON \| OFF \| REMote | OFF | |
| `SYSTem:LANGuage` | "DEFault" \| "D5255S" \| "D5255T" \| "D5255M" | "DEFault" | |
| `SYSTem:VERSion?` | | | Query only |

#### `SYSTem:COMMunicate:GPIB[:SELF]:ADDRess <addr>`

**Description:** Sets the primary address of the optional GPIB interface.

**Parameters:**

| Parameter | Meaning |
|---|---|
| `<addr>` | 1 to 30 |

**Response:** `<addr>`

**Example:**

```
SYST:COMM:GPIB:ADDR 10
SYST:COMM:GPIB:ADDR?    Response: 5
```

- **\*RST state:** Not affected by `*RST`

#### `SYSTem:COMMunicate:SERial:BAUD <value>`

**Description:** Sets the baud rate for the RS232 interface.

**Parameters:**

| Parameter | Meaning |
|---|---|
| `<value>` | 1200 \| 2400 \| 4800 \| 9600 \| 19200 \| 38400 \| 57600 \| 115200 |

**Response:** `<value>`

**Example:**

```
SYST:COMM:SER:BAUD 9600
SYST:COMM:SER:BAUD?     Response: 115200
```

- **\*RST state:** Not affected by `*RST`

#### `SYSTem:DATE <year>,<month>,<day>`

**Description:** Sets the date in the instrument's internal clock.

**Parameters:**

| Parameter | Meaning |
|---|---|
| `<year>` | Must be \<numeric_value\>. The year is in four-digit numeric format. |
| `<month>` | Must be \<numeric_value\>. The range is 1 to 12 inclusive. The number 1 corresponds to the month January, 2 to February, and so on. |
| `<day>` | Must be \<numeric_value\>. The range is 1 to the number of days in the month from the previous parameter. |

**Example:**

```
SYST:DATE 2001,2,5
SYST:DATE?          Response: 2001,2,5
```

- **\*RST state:** Not affected by reset.

#### `SYSTem:TIME <hours>,<minutes>,<seconds>`

**Description:** Sets the time in the instrument's internal clock.

**Parameters:**

| Parameter | Meaning |
|---|---|
| `<hours>` | Must be \<numeric_value\>. The hours are in 24-hour notation. |
| `<minutes>` | Must be \<numeric_value\>. The range is 0 to 59 inclusive. |
| `<seconds>` | Must be \<numeric_value\>. The range is 0 to 59 inclusive. |

**Example:**

```
SYST:TIME 15,45,23
SYST:TIME?          Response: 15,45,23
```

- **\*RST state:** Not affected by reset.

#### `SYSTem:ERRor[:NEXT]?`

**Description:** Queries the error/event queue for the next item and removes it from the queue. The response returns the entire queue item, consisting of an integer and a string. If there are no errors in the queue, `0,"No error"` is returned.

**Response:**

```
<code>,<text description>
```

**Example:**

```
SYST:ERR?           Response: -100,"Command Error"
```

- **\*RST state:** Not affected by reset.

#### `SYSTem:ERRor:ALL?`

**Description:** Queries the error/event queue for all items and removes them from the queue. The response returns a semicolon-separated list of entire queue items consisting of integer/string pairs. If there are no errors in the queue, `0,"No error"` is returned.

**Response:**

```
<code>,<text description>[;<code>,<text description>[; ...]]
```

**Example:**

```
SYST:ERR:ALL?
Response: -102,"Syntax Error";-113,"Undefined Header"
```

- **\*RST state:** Not affected by reset.

#### `SYSTem:KLOCk ON | OFF | REMote`

**Description:** This command locks the local controls on the instrument. This includes the front panel, keyboard and other local interfaces.

**Parameters:**

| Parameter | Meaning |
|---|---|
| ON | All front panel controls are locked. |
| OFF | All front panel controls can be operated by the user. |
| REMote | All front panel controls except F6/Esc are locked when a remote control command is received. |

**Example:**

```
SYST:KLOC ON
SYST:KLOC?          Response: 1
```

- **\*RST state:** OFF

#### `SYSTem:LANGuage "DEFault" | "D5255S" | "D5255T" | "D5255M"`

**Description:** Switches to a different command language. The standard SCPI command set is understood at all times.

**Parameters:**

| Parameter | Meaning |
|---|---|
| "DEFault" | Standard SCPI command set. |
| "D5255S" | Legacy command set used by the Norma D5255 Standard. |
| "D5255T" | Legacy command set used by the Norma D5255 Transformer / Rectified Mean. |
| "D5255M" | Legacy command set used by the D5255 Motor. |

**Example:**

```
SYST:LANGuage "D5255S"
```

- **\*RST state:** "DEFault"

#### `SYSTem:VERSion?`

**Description:** This query returns an \<NR2\>-formatted numeric value corresponding to the SCPI version number the instrument conforms to. The response has the form YYYY.V, where the Ys represent the year version (e.g. 1990) and V represents an approved revision number for that year.

**Response:**

```
<version>
```

**Example:**

```
SYST:VERS?          Response: 1999.0
```

- **\*RST state:** Not affected by reset.

### The STATus subsystem

The STATus subsystem contains the commands for the status reporting system (see the "Status Reporting System" section). `*RST` does not affect the status registers.

#### Command overview (STATus)

| Command | Parameter | Note |
|---|---|---|
| `STATus:OPERation[:EVENt]?` | | Query only |
| `STATus:OPERation:CONDition?` | | Query only |
| `STATus:OPERation:ENABle` | 0 to 65535 | |
| `STATus:OPERation:PTRansition` | 0 to 65535 | |
| `STATus:OPERation:NTRansition` | 0 to 65535 | |
| `STATus:QUEStionable[:EVENt]?` | | Query only |
| `STATus:QUEStionable:CONDition?` | | Query only |
| `STATus:QUEStionable:ENABle` | 0 to 65535 | |
| `STATus:QUEStionable:PTRansition` | 0 to 65535 | |
| `STATus:QUEStionable:NTRansition` | 0 to 65535 | |
| `STATus:QUEStionable:VOLTage[:EVENt]?` | | Query only |
| `STATus:QUEStionable:VOLTage:CONDition?` | | Query only |
| `STATus:QUEStionable:VOLTage:ENABle` | 0 to 65535 | |
| `STATus:QUEStionable:VOLTage:PTRansition` | 0 to 65535 | |
| `STATus:QUEStionable:VOLTage:NTRansition` | 0 to 65535 | |
| `STATus:QUEStionable:CURRent[:EVENt]?` | | Query only |
| `STATus:QUEStionable:CURRent:CONDition?` | | Query only |
| `STATus:QUEStionable:CURRent:ENABle` | 0 to 65535 | |
| `STATus:QUEStionable:CURRent:PTRansition` | 0 to 65535 | |
| `STATus:QUEStionable:CURRent:NTRansition` | 0 to 65535 | |

#### `STATus:QUEStionable:VOLTage:CONDition?`

**Description:** Returns the contents of the condition register associated with the status structure defined in the command. Reading the condition register is non-destructive. The response is (NR1 NUMERIC RESPONSE DATA) (range: 0 through 32767).

**Response:** `<value>` is a 16-bit integer in decimal notation.

| Bits | Meaning |
|---|---|
| bits 0 to 5 | Voltage channels (1, 3, 5, 7, 9, 11) overload. |
| bits 8 to 13 | Voltage channels (1, 3, 5, 7, 9, 11) underload. |

**Example:**

```
STAT:QUES:VOLT:COND?    Response: 2 (voltage overload on phase 2)
```

- **\*RST state:** Has no effect.
#### `STATus:QUEStionable:VOLTage:PTRansition <value>`

**Description:** Sets the positive transition filter. Setting a bit in the positive transition filter causes a 0-to-1 transition in the corresponding bit of the associated condition register to write a 1 into the corresponding bit of the associated event register. The command accepts parameter values in either format in the range 0 through 65535 (decimal) without error. The query response format is \<NR1\>.

**Parameters:**

| Bits | Meaning |
|---|---|
| bits 0 to 5 | Voltage channels (1, 3, 5, 7, 9, 11) overload, positive transition. |
| bits 8 to 13 | Voltage channels (1, 3, 5, 7, 9, 11) underload, positive transition. |

**Example:**

```
STAT:QUES:VOLT:PTR 16191
STAT:QUES:VOLT:PTR?     Response: 16191
```

- **\*RST state:** 0

#### `STATus:QUEStionable:VOLTage:NTRansition <value>`

**Description:** Sets the negative transition filter. Setting a bit in the negative transition filter causes a 0-to-1 transition in the corresponding bit of the associated condition register to write a 1 into the corresponding bit of the associated event register. The command accepts parameter values in either format in the range 0 through 65535 (decimal) without error. The query response format is \<NR1\>.

**Parameters:**

| Bits | Meaning |
|---|---|
| bits 0 to 5 | Voltage channels (1, 3, 5, 7, 9, 11) overload, negative transition. |
| bits 8 to 13 | Voltage channels (1, 3, 5, 7, 9, 11) underload, negative transition. |

**Example:**

```
STAT:QUES:VOLT:NTR 16191
STAT:QUES:VOLT:NTR?     Response: 16191
```

- **\*RST state:** 0

#### `STATus:QUEStionable:VOLTage[:EVENt]?`

**Description:** This query returns the contents of the event register associated with the status structure defined in the command. The response is (NR1 NUMERIC RESPONSE DATA) (range: 0 through 32767). Note that reading the event register clears it.

**Response:** `<value>` is a 16-bit integer in decimal notation.

| Bits | Meaning |
|---|---|
| bits 0 to 5 | Voltage channels (1, 3, 5, 7, 9, 11) overload event. |
| bits 8 to 13 | Voltage channels (1, 3, 5, 7, 9, 11) underload event. |

**Example:**

```
STAT:QUES:VOLT?         Response: 2 (voltage overload on phase 2)
```

- **\*RST state:** Has no effect.

#### `STATus:QUEStionable:VOLTage:ENABle <value>`

**Description:** Sets the enable mask that allows true conditions in the event register to be reported in the summary bit. If a bit is 1 in the enable register and the associated event bit goes true, a positive transition occurs in the associated summary bit. The command accepts parameter values in either format in the range 0 through 65535 (decimal) without error. The query response format is \<NR1\>.

**Parameters:**

| Bits | Meaning |
|---|---|
| bits 0 to 5 | Voltage channels (1, 3, 5, 7, 9, 11), enable overload event. |
| bits 8 to 13 | Voltage channels (1, 3, 5, 7, 9, 11), enable underload event. |

**Example:**

```
STAT:QUES:VOLT:ENAB 16191
STAT:QUES:VOLT:ENAB?    Response: 16191
```

- **\*RST state:** 0

#### `STATus:QUEStionable:CURRent:CONDition?`

**Description:** Returns the contents of the condition register associated with the status structure defined in the command. Reading the condition register is non-destructive. The response is (NR1 NUMERIC RESPONSE DATA) (range: 0 through 32767).

**Response:** `<value>` is a 16-bit integer in decimal notation.

| Bits | Meaning |
|---|---|
| bits 0 to 5 | Current channels (0, 2, 4, 6, 8, 10) overload. |
| bits 8 to 13 | Current channels (0, 2, 4, 6, 8, 10) underload. |

**Example:**

```
STAT:QUES:CURR:COND?    Response: 2 (current overload on phase 2)
```

- **\*RST state:** Has no effect.

#### `STATus:QUEStionable:CURRent:PTRansition <value>`

**Description:** Sets the positive transition filter. Setting a bit in the positive transition filter causes a 0-to-1 transition in the corresponding bit of the associated condition register to write a 1 into the corresponding bit of the associated event register. The command accepts parameter values in either format in the range 0 through 65535 (decimal) without error. The query response format is \<NR1\>.

**Parameters:**

| Bits | Meaning |
|---|---|
| bits 0 to 5 | Current channels (0, 2, 4, 6, 8, 10) overload, positive transition. |
| bits 8 to 13 | Current channels (0, 2, 4, 6, 8, 10) underload, positive transition. |

**Example:**

```
STAT:QUES:CURR:PTR 16191
STAT:QUES:CURR:PTR?     Response: 16191
```

- **\*RST state:** 0

#### `STATus:QUEStionable:CURRent:NTRansition <value>`

**Description:** Sets the negative transition filter. Setting a bit in the negative transition filter causes a 0-to-1 transition in the corresponding bit of the associated condition register to write a 1 into the corresponding bit of the associated event register. The command accepts parameter values in either format in the range 0 through 65535 (decimal) without error. The query response format is \<NR1\>.

**Parameters:**

| Bits | Meaning |
|---|---|
| bits 0 to 5 | Current channels (0, 2, 4, 6, 8, 10) overload, negative transition. |
| bits 8 to 13 | Current channels (0, 2, 4, 6, 8, 10) underload, negative transition. |

**Example:**

```
STAT:QUES:CURR:NTR 16191
STAT:QUES:CURR:NTR?     Response: 16191
```

- **\*RST state:** 0

#### `STATus:QUEStionable:CURRent[:EVENt]?`

**Description:** This query returns the contents of the event register associated with the status structure defined in the command. The response is (NR1 NUMERIC RESPONSE DATA) (range: 0 through 32767). Note that reading the event register clears it.

**Response:** `<value>` is a 16-bit integer in decimal notation.

| Bits | Meaning |
|---|---|
| bits 0 to 5 | Current channels (0, 2, 4, 6, 8, 10) event. |
| bits 8 to 13 | Current channels (0, 2, 4, 6, 8, 10) event. |

**Example:**

```
STAT:QUES:CURR?         Response: 2 (current overload on phase 2)
```

- **\*RST state:** Has no effect.

#### `STATus:QUEStionable:CURRent:ENABle <value>`

**Description:** Sets the enable mask that allows true conditions in the event register to be reported in the summary bit. If a bit is 1 in the enable register and the associated event bit goes true, a positive transition occurs in the associated summary bit. The command accepts parameter values in either format in the range 0 through 65535 (decimal) without error. The query response format is \<NR1\>.

**Parameters:**

| Bits | Meaning |
|---|---|
| bits 0 to 5 | Current channels (0, 2, 4, 6, 8, 10), enable overload event. |
| bits 8 to 13 | Current channels (0, 2, 4, 6, 8, 10), enable underload event. |

**Example:**

```
STAT:QUES:CURR:ENAB 16191
STAT:QUES:CURR:ENAB?    Response: 16191
```

- **\*RST state:** 0

#### `STATus:QUEStionable:CONDition?`

**Description:** Returns the contents of the condition register associated with the status structure defined in the command. Reading the condition register is non-destructive. The response is (NR1 NUMERIC RESPONSE DATA) (range: 0 through 32767).

**Response:** `<value>` is a 16-bit integer in decimal notation.

| Bit | Meaning |
|---|---|
| bit 0 | Voltage summary questionable. |
| bit 1 | Current summary questionable. |
| bit 5 | Frequency questionable. |

**Example:**

```
STAT:QUES:COND?
Response: 1 (voltage over/underload on some phase)
```

- **\*RST state:** Has no effect.

#### `STATus:QUEStionable:PTRansition <value>`

**Description:** Sets the positive transition filter. Setting a bit in the positive transition filter causes a 0-to-1 transition in the corresponding bit of the associated condition register to write a 1 into the corresponding bit of the associated event register. The command accepts parameter values in either format in the range 0 through 65535 (decimal) without error. The query response format is \<NR1\>.

**Parameters:**

| Bit | Meaning |
|---|---|
| bit 0 | Voltage summary questionable. |
| bit 1 | Current summary questionable. |
| bit 5 | Frequency questionable. |

**Example:**

```
STAT:QUES:PTR 35
STAT:QUES:PTR?      Response: 35
```

- **\*RST state:** 0

#### `STATus:QUEStionable:NTRansition <value>`

**Description:** Sets the negative transition filter. Setting a bit in the negative transition filter causes a 0-to-1 transition in the corresponding bit of the associated condition register to write a 1 into the corresponding bit of the associated event register. The command accepts parameter values in either format in the range 0 through 65535 (decimal) without error. The query response format is \<NR1\>.

**Parameters:**

| Bit | Meaning |
|---|---|
| bit 0 | Voltage summary questionable. |
| bit 1 | Current summary questionable. |
| bit 5 | Frequency questionable. |

**Example:**

```
STAT:QUES:NTR 35
STAT:QUES:NTR?      Response: 35
```

- **\*RST state:** 0

#### `STATus:QUEStionable[:EVENt]?`

**Description:** This query returns the contents of the event register associated with the status structure defined in the command. The response is (NR1 NUMERIC RESPONSE DATA) (range: 0 through 32767). Note that reading the event register clears it.

**Response:** `<value>` is a 16-bit integer in decimal notation.

| Bit | Meaning |
|---|---|
| bit 0 | Voltage summary questionable. |
| bit 1 | Current summary questionable. |
| bit 5 | Frequency questionable. |

**Example:**

```
STAT:QUES?          Response: 1 (voltage overload/underload on some phase)
```

- **\*RST state:** Has no effect.

#### `STATus:QUEStionable:ENABle <value>`

**Description:** Sets the enable mask that allows true conditions in the event register to be reported in the summary bit. If a bit is 1 in the enable register and the associated event bit goes true, a positive transition occurs in the associated summary bit. The command accepts parameter values in either format in the range 0 through 65535 (decimal) without error. The query response format is \<NR1\>.

**Parameters:**

| Bit | Meaning |
|---|---|
| bit 0 | Voltage summary questionable. |
| bit 1 | Current summary questionable. |
| bit 5 | Frequency questionable. |

**Example:**

```
STAT:QUES:ENAB 35
STAT:QUES:ENAB?     Response: 35
```

- **\*RST state:** 0

#### `STATus:OPERation:CONDition?`

**Description:** Returns the contents of the condition register associated with the status structure defined in the command. Reading the condition register is non-destructive. The response is (NR1 NUMERIC RESPONSE DATA) (range: 0 through 32767).

**Response:**

| Bit | Meaning |
|---|---|
| bit 2 | Ranging (changing measurement range). |
| bit 3 | Sweeping (memory recording in progress). |
| bit 5 | Waiting for trigger. |
| bit 8 | Synchronized (if the sync source changes, a glitch occurs). |
| bit 10 | Averaging (averaging in progress; at the end of each averaging cycle a glitch occurs). |
| bit 12 | Spectrum CALCulation in progress. |

**Example:**

```
STAT:OPER:COND?     Response: 1280 (synchronized and averaging)
```

- **\*RST state:** Has no effect.

#### `STATus:OPERation:PTRansition <value>`

**Description:** Sets the positive transition filter. Setting a bit in the positive transition filter causes a 0-to-1 transition in the corresponding bit of the associated condition register to write a 1 into the corresponding bit of the associated event register.

**Parameters:**

| Bit | Meaning |
|---|---|
| bit 2 | Ranging (changing measurement range). |
| bit 3 | Sweeping (memory recording in progress). |
| bit 5 | Waiting for trigger. |
| bit 8 | Synchronized (if the sync source changes, a glitch occurs). |
| bit 10 | Averaging (averaging in progress; at the end of each averaging cycle a glitch occurs). |
| bit 12 | Spectrum CALCulation in progress. |

**Example:**

```
STAT:OPER:PTR 5948
STAT:OPER:PTR?      Response: 5948
```

- **\*RST state:** 0

#### `STATus:OPERation:NTRansition <value>`

**Description:** Sets the negative transition filter. Setting a bit in the negative transition filter causes a 1-to-0 transition in the corresponding bit of the associated condition register to write a 1 into the corresponding bit of the associated event register.

**Parameters:**

| Bit | Meaning |
|---|---|
| bit 2 | Ranging (changing measurement range). |
| bit 3 | Sweeping (memory recording in progress). |
| bit 5 | Waiting for trigger. |
| bit 8 | Synchronized (if the sync source changes, a glitch occurs). |
| bit 10 | Averaging (averaging in progress; at the end of each averaging cycle a glitch occurs). |
| bit 12 | Spectrum CALCulation in progress. |

**Example:**

```
STAT:OPER:NTR 5948
STAT:OPER:NTR?      Response: 5948
```

- **\*RST state:** 0

#### `STATus:OPERation[:EVENt]?`

**Description:** This query returns the contents of the event register associated with the status structure defined in the command. The response is (NR1 NUMERIC RESPONSE DATA) (range: 0 through 32767). Note that reading the event register clears it.

**Response:** `<value>` is a 16-bit integer in decimal notation.

| Bit | Meaning |
|---|---|
| bit 2 | Ranging (changing measurement range). |
| bit 3 | Sweeping (memory recording in progress). |
| bit 5 | Waiting for trigger. |
| bit 8 | Synchronized (if the sync source changes, a glitch occurs). |
| bit 10 | Averaging (averaging in progress; at the end of each averaging cycle a glitch occurs). |
| bit 12 | Spectrum CALCulation in progress. |

**Example:**

```
STAT:OPER?          Response: 2 (changing measurement range)
```

- **\*RST state:** Has no effect.

#### `STATus:OPERation:ENABle <value>`

**Description:** Sets the enable mask that allows true conditions in the event register to be reported in the summary bit. If a bit is 1 in the enable register and the associated event bit goes true, a positive transition occurs in the associated summary bit. The command accepts parameter values in either format in the range 0 through 65535 (decimal) without error. The query response format is \<NR1\>.

**Parameters:**

| Bit | Meaning |
|---|---|
| bit 2 | Ranging (changing measurement range). |
| bit 3 | Sweeping (memory recording in progress). |
| bit 5 | Waiting for trigger. |
| bit 8 | Synchronized (if the sync source changes, a glitch occurs). |
| bit 10 | Averaging (averaging in progress; at the end of each averaging cycle a glitch occurs). |
| bit 12 | Spectrum CALCulation in progress. |

**Example:**

```
STAT:OPER:ENAB 5948
STAT:OPER:ENAB?     Response: 5948
```

- **\*RST state:** 0

## Quick reference: all commands

This is a complete overview of all the commands in the remote control API, grouped by subsystem ("List of Commands Grouped by Subsystems" from the manual). The tables show command syntax, valid parameters and any notes (firmware requirements, options and so on).

Conventions:

- Upper-case letters in the command name indicate the SCPI short form (e.g. `CALCulate` can be written `CALC`).
- Parts in square brackets `[...]` are optional.
- `|` separates alternative values or keywords.
- The note **n. i.** means *Not Implemented*.

### Common commands (IEEE 488.2)

| Command | Parameter | Note |
|---|---|---|
| `*CLS` | | |
| `*ESE` | 0 to 255 | |
| `*ESR?` | | |
| `*IDN?` | | |
| `*OPC` | | |
| `*OPC?` | | |
| `*OPT?` | | |
| `*RST` | | |
| `*SRE` | 0 to 255 | |
| `*STB?` | | |
| `*WAI` | | |
| `*SAV` | 10 to 24 | |
| `*RCL` | 1, 2, 10 to 24 | |
| `*LRN?` | | |
| `*TRG` | | |

### ABORt

| Command | Parameter | Note |
|---|---|---|
| `ABORt` | | |

### CALCulate

| Command | Parameter | Note |
|---|---|---|
| `CALCulate:TRANsform:FREQuency[:STATe]` | ONCE | |
| `CALCulate:TRANsform:FREQuency:MODE` | FFT \| DFT \| STD | STD: FW V1.5 and up |
| `CALCulate:TRANsform:FREQuency:FUNCtion` | `<function list>` | |
| `CALCulate:TRANsform:FREQuency:STARt` | `<frequency>` | for FFT and DFT only |
| `CALCulate:TRANsform:FREQuency:STOP` | `<frequency>` | for FFT and DFT only |
| `CALCulate:TRANsform:FREQuency:CYCLes` | 4 \| 6 \| 8 \| 10 \| 12 | for STD only (FW V1.5 and up) |
| `CALCulate:TRANsform:FREQuency:GROuping` | COMPonent \| HARMonic \| HGRoup \| HSGRoup \| ISGRoup \| SGRoup | for STD only (FW V1.5 and up) |
| `CALC:DATA?` | `[<count>[,<offset>]]` | |
| `CALC:DATA:PREamble?` | | |
| `CALC:DATA:THD?` | | for STD only (FW V1.5 and up) |
| `CALCulate:INTegral[:STATe]` | ON \| OFF | |
| `CALCulate:INTegral:CLEar[:IMMediate]` | | |
| `CALCulate:INTegral:CLEar:AUTO` | ON \| OFF | |
| `CALCulate:INTegral:STARt:SOURce` | CMD \| TIME \| MAN | |
| `CALCulate:INTegral:STARt[:IMMediate]` | | |
| `CALCulate:INTegral:STARt:TIME` | yyyy,MM,dd,hh,mm,ss | |
| `CALCulate:INTegral:STOP:SOURce` | CMD \| TIME \| MAN \| TINTerval | |
| `CALCulate:INTegral:STOP[:IMMediate]` | | |
| `CALCulate:INTegral:STOP:TIME` | yyyy,MM,dd,hh,mm,ss | |
| `CALCulate:INTegral:STOP:TINTerval` | 1.0e-3 to 9.99e+6 | |
| `CALCulate:HARMonic:ORDer` | `<order>` | |
| `CALCulate:POWer:CORRected` | STAR \| DELTa | FW V1.4 and up |
| `CALCulate:POWer:EFFiciency:REFerence` | `<function1>, <function2>` | |

### DISPlay

| Command | Parameter | Note |
|---|---|---|
| `DISPlay[:WINDow][:STATe]` | ON \| OFF | |
| `DISPlay:USER:FUNCtion` | `<function list>` | |

### FORMat

| Command | Parameter | Note |
|---|---|---|
| `FORMat[:DATA]` | ASCii \| REAL, [0..8] \| [32 \| 64] | |
| `FORMat[:DATA]:STATus` | ASCii \| INTeger, [8] \| 16 \| 32 | |
| `FORMat:BORDer` | NORMal \| SWAPped | |
| `FORMat:TRANspose` | ON \| OFF | |

### HCOPy

| Command | Parameter | Note |
|---|---|---|
| `HCOPy:SDUMp:DATA?` | | |

### INITiate

| Command | Parameter | Note |
|---|---|---|
| `INITiate:CONTinuous` | ON \| OFF | |
| `INITiate[:IMMediate]` | | |
| `INITiate[:IMMediate]:SEQuence1` / `INITiate[:IMMediate]:NAME STARt` | | |
| `INITiate[:IMMediate]:SEQuence2` / `INITiate[:IMMediate]:NAME STOP` | | |

### INPut

| Command | Parameter | Note |
|---|---|---|
| `INPut[1..12]:COUPling` | AC \| DC | |
| `INPut[1\|2\|3\|4\|5\|6\|7\|8\|9\|10\|11\|12]:GAIN` | 1.0e-7 to 1.0e+7 | |
| `INPut[1..12]:FILTer[:STATe]` | ON \| OFF | |
| `INPut[1..12]:FILTer[:LPASs]:FREQuency?` | | |
| `INPut[1\|2\|3\|4\|5\|6\|7\|8\|9\|10\|11\|12]:SHUNt` | INTernal \| EXTernal | |
| `INPut[21..28]:TYPe` | VOLTage \| FREQuency | Option PI1 |

### OUTPut

| Command | Parameter | Note |
|---|---|---|
| `OUTPut9[:STATe]` | ON \| OFF | |

### ROUTe

| Command | Parameter | Note |
|---|---|---|
| `ROUTe:SYSTem` | "3W" \| "2W" | |

### SENSe

| Command | Parameter | Note |
|---|---|---|
| `[SENSe:]CURRent[1..6]\|VOLTage[1..6]:AC\|[:DC]:RANGe[:UPPer]` | 0.3 to 1000.0 V, 0.03 to 10.0 A, 0.03 to 10 V | |
| `[SENSe:]CURRent[1..6]\|VOLTage[1..6]:AC[\|:DC]:SCALe` | 0.9 to 1.0e+7 | |
| `[SENSe:]CURRent[1..6]\|VOLTage[1..6]:AC[\|:DC]:RANGe[:UPPer]:LIST?` | | |
| `[SENSe:]CURRent[1..6]\|VOLTage[1..6]:AC[\|:DC]:RANGe[:UPPer]:AUTO` | ON \| OFF | |
| `[SENSe:]CURRent[1..6]\|VOLTage[1..6][\|:POWer]:AC[\|:DC]:APERture` | 0.015 to 3600.0 s | |
| `[SENSe:]SWEep:FREQuency?` | | |
| `[SENSe:]FUNCtion[:ON]` | `<function>{,<function>}` | |
| `[SENSe:]FUNCtion[:ON]:ALL` | | |
| `[SENSe:]FUNCtion:OFF:ALL` | | |
| `[SENSe:]FUNCtion:CONCurrent` | ON \| OFF | |
| `[SENSe:]FUNCtion[:ON]:COUNt?` | | |
| `[SENSe:]DATA?` | `[<function>{,<function...>}]` | |
| `[SENSe:]DATA:STATus?` | `[<function>{,<function...>}]` | |
| `[SENSe:]SWEep1\|2:TIME` | `<value>` \| MAX | |
| `[SENSe:]SWEep1\|2:OFFSet:TIME` | `<value>` \| MAX | |
| `[SENSe:]SWEep1\|2:POINTS?` | | |
| `[SENSe:]SWEep1\|2:OFFSet:POINTS?` | | |
| `[SENSe:]SWEep1\|2[:STATe]` | ON \| OFF | |
| `[SENSe:]SWEep1\|2:COUNt` | `<count>` | |
| `[SENSe:]SWEep1\|2:SFACtor` | 1 to 65535 | |
| `[SENSe:]SWEep1\|2:FUNCtion` | `<function list>` | |

### SENSe2 (motor/process interface, Option PI1)

| Command | Parameter | Note |
|---|---|---|
| `SENSe2:TORQue[1..4]:VOLTage:SCALe` | -1e6 to 1e6 | Option PI1 |
| `SENSe2:TORQue[1..4]:VOLTage:OFFSet[:VALue]` | -1e6 to 1e6 | Option PI1 |
| `SENSe2:TORQue[1..4]:VOLTage:OFFSet:IMMediate` | | Option PI1 |
| `SENSe2:TORQue[1..4]:FREQuency:SCALe` | -1e6 to 1e6 | Option PI1 |
| `SENSe2:TORQue[1..4]:FREQuency:OFFSet[:VALue]` | -1e6 to 1e6 | Option PI1 |
| `SENSe2:TORQue[1..4]:FREQuency:OFFSet:IMMediate` | | Option PI1 |
| `SENSe2:SPEed[1..4]:VOLTage:SCALe[:DEFault]` | -1e6 to 1e6 | Option PI1 |
| `SENSe2:SPEed[1..4]:VOLTage:OFFSet[:VALue]` | -1e6 to 1e6 | Option PI1 |
| `SENSe2:SPEed[1..4]:VOLTage:OFFSet:IMMediate` | | Option PI1 |
| `SENSe2:SPEed[1..4]:FREQuency:SCALe[:DEFault]` | -1e6 to 1e6 | Option PI1 |
| `SENSe2:SPEed[1..4]:FREQuency:SCALe:PULSe` | 1 to 100000 | Option PI1 |
| `SENSe2:SPEed[1..4]:FREQuency:OFFSet[:VALue]` | -1e6 to 1e6 | Option PI1 |
| `SENSe2:SPEed[1..4]:FREQuency:OFFSet:IMMediate` | | Option PI1 |
| `SENSe2:TYPe[1..4]` | MOTor \| GENerator | Option PI1 |
| `SENSe2:POLepairs[1..4]` | 1 to 999 | Option PI1 |
| `SENSe2:REFerence[1..4][:POWer]` | "POWer[1..6][:ACTive]" | Option PI1 |

### SOURce (analog outputs, Option PI1)

| Command | Parameter | Note |
|---|---|---|
| `SOURce:VOLTage[1..4]:MODE` | FIXed \| VARiable | Option PI1 |
| `SOURce:VOLTage[1..4][:LEVel][:IMMediate][:AMPLitude]` | -10.3 to 10.3 | Option PI1 |
| `SOURce:VOLTage[1..4]:FEED` | `<function>` | Option PI1 |
| `SOURce:VOLTage[1..4]:GAIN` | -1e6 to 1e6 | Option PI1 |
| `SOURce:VOLTage[1..4]:ZERO` | -1e6 to 1e6 | Option PI1 |

### SYNC

| Command | Parameter | Note |
|---|---|---|
| `SYNC:STATe` | ON \| OFF | |
| `SYNC:SOURce` | VOLTage[1..6] \| CURRent[1..6] \| EXTernal | |
| `SYNC[:SOURce][\|CURRent[1..6]\|VOLTage[1..6]]:LEVel` | (-150% to 150% of nominal input range) | |
| `SYNC:LEVel:UNIT` | ABSolute \| PCT | |
| `SYNC[:SOURce]\|CURRent[1..6]\|VOLTage[1..6]:SLOPe` | POSitive \| NEGative | SOUR only |
| `SYNC[:SOURce]\|CURRent[1..6]\|VOLTage[1..6]:FILTer:[LPASs[:STATe]]` | ON \| OFF | SOUR only |
| `SYNC[:SOURce]\|CURRent[1..6]\|VOLTage[1..6]:FILTer:[LPASs]:FREQuency` | 100Hz, 1kHz, 10kHz | SOUR only |
| `SYNC:TIMeout` | 0.015 to 3600 s | |

### TIMer

| Command | Parameter | Note |
|---|---|---|
| `TIMer:RESet` | | |
| `TIMer:RESet:AUTO` | ON \| OFF | n. i. |
| `TIMer:RESet:TIME?` | | |

### TRACe

| Command | Parameter | Note |
|---|---|---|
| `TRACe[:DATA]:PREamble?` | `<block>` | |
| `TRACe[:DATA]?` | `[<block> [,<count> [,<offset> [,<sparsing>[,opt_level]]]]]` | |
| `TRACe[:DATA]:STATus?` | `[<block> [,<count> [,<offset> [,<sparsing>]]]]` | |
| `TRACe:FREE?` | | |
| `TRACe:CATalog:LENgth?` | | |
| `TRACe:DELete:ALL` | | |

### TRIGger

| Command | Parameter | Note |
|---|---|---|
| `TRIGger:STARt:SOURce` | BUS \| TIME \| IMMediate \| MANual \| SYNC \| `<function>` | |
| `TRIGger:STARt:TIME` | yyyy,MM,dd,hh,mm,ss | |
| `TRIGger:STARt:LEVel` | `<level>` | |
| `TRIGger:STARt:SLOPe` | POSitive \| NEGative | |
| `TRIGger:STOP:SOURce` | TIME \| IMMediate \| MANual \| `<function>` | |
| `TRIGger:STOP:TIME` | yyyy,MM,dd,hh,mm,ss | |
| `TRIGger:STOP:LEVel` | `<level>` | |
| `TRIGger:STOP:SLOPe` | POSitive \| NEGative | |

### SYSTem

| Command | Parameter | Note |
|---|---|---|
| `SYSTem:COMMunicate:GPIB[:SELF]:ADDRess` | 1 to 30 | |
| `SYSTem:COMMunicate:SERial:BAUD` | 1200 to 115200 | |
| `SYSTem:DATE` | `<year>,<month>,<day>` | |
| `SYSTem:TIME` | `<hours>,<minutes>,<seconds>` | |
| `SYSTem:ERRor[:NEXT]?` | | |
| `SYSTem:ERRor:ALL?` | | |
| `SYSTem:KLOCk` | ON \| OFF \| REMote | |
| `SYSTem:LANGuage` | "DEFault" \| "D5255S" \| "D5255T" \| "D5255M" | |
| `SYSTem:VERSion?` | | |

### STATus

| Command | Parameter | Note |
|---|---|---|
| `STATus:QUEStionable:VOLTage:CONDition?` | | |
| `STATus:QUEStionable:VOLTage:PTRansition` | 0 to 65535 | |
| `STATus:QUEStionable:VOLTage:NTRansition` | 0 to 65535 | |
| `STATus:QUEStionable:VOLTage[:EVENt]?` | | |
| `STATus:QUEStionable:VOLTage:ENABle` | 0 to 65535 | |
| `STATus:QUEStionable:CURRent:CONDition?` | | |
| `STATus:QUEStionable:CURRent:PTRansition` | 0 to 65535 | |
| `STATus:QUEStionable:CURRent:NTRansition` | 0 to 65535 | |
| `STATus:QUEStionable:CURRent[:EVENt]?` | | |
| `STATus:QUEStionable:CURRent:ENABle` | 0 to 65535 | |
| `STATus:QUEStionable:CONDition?` | | |
| `STATus:QUEStionable:PTRansition` | 0 to 65535 | |
| `STATus:QUEStionable:NTRansition` | 0 to 65535 | |
| `STATus:QUEStionable[:EVENt]?` | | |
| `STATus:QUEStionable:ENABle` | 0 to 65535 | |
| `STATus:OPERation:CONDition?` | | |
| `STATus:OPERation:PTRansition` | 0 to 65535 | |
| `STATus:OPERation:NTRansition` | 0 to 65535 | |
| `STATus:OPERation[:EVENt]?` | | |
| `STATus:OPERation:ENABle` | 0 to 65535 | |

*n. i. – Not Implemented.*

## The status reporting system

### Introduction

The status reporting system stores all information about the instrument's current operating state, for example about errors that have occurred. The information is stored in status registers and in an error queue. Both the status registers and the error queue can be queried over the IEC/IEEE bus.

The information is structured hierarchically:

- The highest level is the status byte (**STB**, Status Byte) defined in IEEE 488.2, with its associated mask register **SRE** (Service Request Enable).
- The STB receives information from the standardized **ESR** (Event Status Register), also defined in IEEE 488.2, with its associated mask register **ESE** (Event Status Enable).
- The STB additionally receives information from the **STATus:OPERation** and **STATus:QUEStionable** registers, which are defined by SCPI and contain detailed information about the instrument.

The output buffer contains the messages the instrument returns to the controller. The output buffer is not part of the status reporting system, but it determines the value of the MAV bit in the STB register.

### Structure of a SCPI status register

Each SCPI register consists of five parts, each 16 bits wide, with different functions. The individual bits are independent of each other. Each hardware status is assigned a bit number that applies to all five parts. For example, bit 3 in the STATus:OPERation register is assigned the hardware status "Wait for trigger" in all five parts. Bit 15 (the most significant bit) is set to zero in all five parts, so that the controller can treat the register contents as a positive integer.

#### The CONDition part

The CONDition part is written to directly by the hardware or by the sum bit from the next lower register. Its contents reflect the instrument's current status. This register part can only be read, not written to or cleared. Reading does not affect the contents.

#### The PTRansition part

The PTRansition part (Positive Transition) acts as an edge detector. If a bit in the CONDition part changes from 0 to 1, the state of the corresponding PTR bit determines whether the EVENt bit is set to 1:

- PTR bit = 1: the EVENt bit is set.
- PTR bit = 0: the EVENt bit is not set.

This part can be both written to and read. Reading does not affect the contents.

#### The NTRansition part

The NTRansition part (Negative Transition) likewise acts as an edge detector. If a bit in the CONDition part changes from 1 to 0, the state of the corresponding NTR bit determines whether the EVENt bit is set to 1:

- NTR bit = 1: the EVENt bit is set.
- NTR bit = 0: the EVENt bit is not set.

This part can be both written to and read. Reading does not affect the contents.

With these two edge register parts, the user can define which status transition in the CONDition part (none, 0 to 1, 1 to 0 or both) is to be stored in the EVENt part.

#### The EVENt part

The EVENt part states whether an event has occurred since it was last read; it is the "memory" of the CONDition part. It shows only the events passed by the edge filters. The EVENt part is updated continuously by the instrument. This part can only be read. Reading clears its contents. In everyday use the EVENt part is often referred to as synonymous with the whole register.

#### The ENABle part

The ENABle part determines whether the corresponding EVENt bit contributes to the sum bit (see below). Each bit in the EVENt part is ANDed with the corresponding ENABle bit (symbol &). The results of all the logical operations in this part are passed on to the sum bit via an OR function (symbol +):

- ENABle bit = 0: the corresponding EVENt bit does not contribute to the sum bit.
- ENABle bit = 1: if the corresponding EVENt bit is 1, the sum bit is also set to 1.

This part can be both written to and read. Reading does not affect the contents.

#### The sum bit

The sum bit is formed, as mentioned above, from the EVENt part and the ENABle part of each register. The result is entered as a bit in the CONDition part of the next higher register.

The instrument generates a sum bit for each register automatically. This ensures that an event, for example a PLL that has not locked, can trigger a service request through all hierarchical levels.

> **Note:** The service request enable register (SRE) defined in IEEE 488.2 can be regarded as the ENABle part of the STB when the STB is structured in accordance with SCPI. Likewise, the ESE can be regarded as the ENABle part of the ESR.

### Overview of the status registers

The hierarchy of the status reporting structure (the minimum structure required by SCPI) is as follows:

- **Status Byte (STB)** — the top level. Receives sum bits from:
  - **Standard Event Status Register (ESR)** — bit 0 Operation Complete, bit 1 Not used, bit 2 Query Error, bit 3 Device Dependent Error, bit 4 Execution Error, bit 5 Command Error, bit 6 User Request, bit 7 Power On.
  - The **OPERation Status** register — bits 0–1 Reserved, bit 2 RANGing, bit 3 SWEeping, bit 4 MEASuring, bit 5 Waiting for TRIGger Summary, bits 6–7 Reserved, bit 8 SYNChronized, bit 9 Sync available (reserved), bit 10 Averaging, bit 11 Reserved, bit 12 CALCulation, bits 13–14 Reserved, bit 15 NOT USED*.
  - The **QUEStionable Status** register — which in turn receives sum bits from the sub-registers:
    - **QUEStionable:CURRent** — bits 0–5 INPut1/3/5/7/9/11 overload, bits 6–7 Reserved, bits 8–13 INPut1/3/5/7/9/11 underload, bit 14 Reserved, bit 15 NOT USED*.
    - **QUEStionable:VOLTage** — bits 0–5 INPut2/4/6/8/10/12 overload, bits 6–7 Reserved, bits 8–13 INPut2/4/6/8/10/12 underload, bit 14 Reserved, bit 15 NOT USED*.
  - The **Error/Event Queue** — controls bit 2 in the STB.
  - The **Output Buffer** — controls the MAV bit in the STB.

Bit 6 in the STB is RQS/MSS (triggers SRQ on the bus).

\* Use of bit 15 is not permitted, since some controllers may have problems reading an unsigned 16-bit integer. The value of this bit must always be 0.

### Description of the status registers

#### Status Byte (STB) and Service Request Enable Register (SRE)

The STB is defined in IEEE 488.2. It gives a rough overview of the instrument status by collecting the information from the lower registers. It can be compared to the CONDition part of a SCPI register and constitutes the highest level in the SCPI hierarchy. A special feature is that bit 6 acts as the sum bit for the other bits in the status byte.

The status byte is read with the command `*STB?` or via a serial poll.

The STB has an associated SRE. The SRE corresponds functionally to the ENABle part of the SCPI registers. Each bit in the STB is assigned a bit in the SRE. Bit 6 in the SRE is ignored. If a bit is set in the SRE and the corresponding bit in the STB changes from 0 to 1, a service request (SRQ) is generated on the IEC/IEEE bus, which triggers an interrupt in the controller (if the controller is configured for it) and can be processed further there.

The SRE is set with the command `*SRE` and read with the command `*SRE?`.

**Table 2-1. Status Register Bits**

| Bit no. | Description |
|---|---|
| 2 | **Error Queue Not Empty** — This bit is set when an entry is made in the error queue. If the bit is enabled via the SRE, every entry in the error queue generates a service request. An error can then be recognized and investigated further by querying the error queue. The query gives an informative error message. This approach is recommended, since it reduces the problems of IEC/IEEE bus control. |
| 3 | **QUEStionable Status sum bit** — This bit is set if an EVENt bit is set in the QUEStionable status register and the corresponding ENABle bit is set to 1. A set bit indicates a questionable instrument state that can be investigated further by querying the QUEStionable status register. |
| 4 | **MAV bit (Message AVailable)** — This bit is set if there is a message in the output buffer that can be read. The bit can be used for automatic reading of data from the instrument to the controller (see the program examples chapter, chapter 6 in the manual). |
| 5 | **ESB bit** — Sum bit for the event status register. It is set if one of the bits in the event status register is set and enabled in the event status enable register. A set bit indicates a serious error that can be investigated further by querying the event status register. |
| 6 | **MSS bit (Master Status Summary bit)** — This bit is set if the instrument triggers a service request. That happens when one of the other bits in the register is set together with its mask bit in the service request enable register (SRE). |
| 7 | **OPERation Status Register sum bit** — This bit is set if an EVENt bit is set in the OPERation status register and the corresponding ENABle bit is set to 1. A set bit indicates that the instrument is performing an action. The type of action can be determined by querying the OPERation status register. |

#### Event Status Register (ESR) and Event Status Enable Register (ESE)

The ESR is defined in IEEE 488.2. It can be compared to the EVENt part of a SCPI register. The event status register is read with the command `*ESR?`. The ESE is the corresponding ENABle part. It is set with the command `*ESE` and read with the command `*ESE?`.

**Table 2-2. Event Status Register Bits**

| Bit no. | Description |
|---|---|
| 0 | **Operation Complete** — This bit is set on receipt of the command `*OPC` when all preceding commands have been executed. |
| 1 | This bit is not used. |
| 2 | **Query Error** — This bit is set if the controller wants to read data from the instrument without having sent a query, or if it does not fetch requested data and instead sends new instructions to the instrument. The cause is often an erroneous query that consequently cannot be executed. |
| 3 | **Device-Dependent Error** — This bit is set if a device-dependent error occurs. An error message with a number between -300 and -399, or a positive error number describing the error in more detail, is entered in the error queue (see chapter 5 in the manual). |
| 4 | **Execution Error** — This bit is set if a received command is syntactically correct but cannot be executed for other reasons. An error message with a number between -200 and -300, describing the error in more detail, is entered in the error queue (see chapter 5 in the manual). |
| 5 | **Command Error** — This bit is set if a command is received that is undefined or syntactically incorrect. An error message with a number between -100 and -200, describing the error in more detail, is entered in the error queue (see chapter 5 in the manual). |
| 6 | **User Request** — This bit is set when the [LOCAL] key is pressed and the instrument is put into manual control. (The manual also states that this bit is not used.) |
| 7 | **Power On (AC supply voltage On)** — This bit is set when the instrument is switched on. |

#### The STATus:OPERation register

In its CONDition part this register contains information about which actions the instrument is currently performing, and in its EVENt part information about which actions the instrument has performed since the last read. The register is read with the commands:

```
STATus:OPERation:CONDition?
STATus:OPERation[:EVENt]?
```

**Table 2-3. STATus:OPERation Register Bits**

| Bit no. | Description |
|---|---|
| 0 to 1 | These bits are not used. |
| 2 | **RANGing** — This bit is set while the instrument is changing the measurement range on the input channels in autorange mode. |
| 3 | **SWEeping** — When memory recording is in progress, this bit is set to 1. While the pretrigger is being filled or while the instrument is waiting for a trigger, the bit is not set. |
| 4 | Not used. |
| 5 | **Waiting for TRIGger Summary** — When memory recording is waiting for a trigger after being started with `INITiate[:IMMediate]:SEQuence1` or `INITiate:CONTinuous:SEQuence1 ON`, this bit is set. It is cleared when the trigger arrives. |
| 6 to 7 | These bits are not used. |
| 8 | **SYNChronized** — This bit is set when the instrument is synchronized to a valid SYNC source. The bit is set to 0 when `SYNC:STATe` is set to OFF. |
| 9 | **SYNChronization Available (reserved)** — This bit is set if a valid synchronization signal is present on at least one input channel. (The manual also states that this bit is not used.) |
| 10 | **AVERaging** — This bit is set when the instrument is processing its averaging cycle. In free-run mode the bit is set to 0 for a short period at the end of each averaging cycle. |
| 11 | This bit is not used. |
| 12 | **CALCulation** — This bit is set to 1 if the computation is in progress. When the computation is complete, the bit is set to 0. |
| 13 to 14 | These bits are not used. |
| 15 | This bit is always 0. |

#### The STATus:QUEStionable register

This register contains information about indeterminate states that can arise if the unit is used outside its specifications. It can be queried with the commands:

```
STATus:QUEStionable:CONDition?
STATus:QUEStionable[:EVENt]?
```

**Table 2-4. STATus:QUEStionable Register Bits**

| Bit no. | Description |
|---|---|
| 0 | QUEStionable:VOLTage Register Summary. |
| 1 | QUEStionable:CURRent Register Summary. |
| 2 to 4 | These bits are not used. |
| 5 | **FREQuency** — The bit is set if the frequency measurement is invalid because of poor signal quality. |
| 6 to 14 | These bits are not used. |
| 15 | This bit is always 0. |

#### The STATus:QUEStionable:CURRent register

This register contains information about overload/underload conditions that can arise if the measurement range on a current input channel is exceeded or the input signal is too low. It can be queried with the commands:

```
STATus:QUEStionable:CURRent:CONDition?
STATus:QUEStionable:CURRent[:EVENt]?
```

**Table 2-5. STATus:QUEStionable:CURRent Register Bits**

| Bit no. | Description |
|---|---|
| 0 | INPut1 OVERrange |
| 1 | INPut3 OVERrange |
| 2 | INPut5 OVERrange |
| 3 | INPut7 OVERrange |
| 4 | INPut9 OVERrange |
| 5 | INPut11 OVERrange |
| 6 to 7 | These bits are not used. |
| 8 | INPut1 UNDERrange |
| 9 | INPut3 UNDERrange |
| 10 | INPut5 UNDERrange |
| 11 | INPut7 UNDERrange |
| 12 | INPut9 UNDERrange |
| 13 | INPut11 UNDERrange |
| 14 | This bit is not used. |
| 15 | This bit is always 0. |

#### The STATus:QUEStionable:VOLTage register

This register contains information about overload/underload conditions that can arise if the measurement range on a voltage input channel is exceeded or the input signal is too low. It can be queried with the commands:

```
STATus:QUEStionable:VOLTage:CONDition?
STATus:QUEStionable:VOLTage[:EVENt]?
```

**Table 2-6. STATus:QUEStionable:VOLTage Register Bits**

| Bit no. | Description |
|---|---|
| 0 | INPut2 OVERrange |
| 1 | INPut4 OVERrange |
| 2 | INPut6 OVERrange |
| 3 | INPut8 OVERrange |
| 4 | INPut10 OVERrange |
| 5 | INPut12 OVERrange |
| 6 to 7 | These bits are not used. |
| 8 | INPut2 UNDERrange |
| 9 | INPut4 UNDERrange |
| 10 | INPut6 UNDERrange |
| 11 | INPut8 UNDERrange |
| 12 | INPut10 UNDERrange |
| 13 | INPut12 UNDERrange |
| 14 | This bit is not used. |
| 15 | This bit is always 0. |

### Practical use of the status reporting system

To use the status reporting system effectively, the information it holds must be transferred to the controller and processed further there. There are several methods, described below. Detailed program examples are found in chapter 6 of the manual.

#### Service request — using the hierarchical structure (GPIB only)

Under certain circumstances the instrument can send a service request (SRQ) to the controller. Normally this service request triggers an interrupt in the controller, which the control program can respond to with appropriate actions. An SRQ is always triggered when one or more of bits 2, 3, 4, 5 or 7 in the status byte are set and enabled in the SRE. Each of these bits summarizes the information from an underlying register, the error queue or the output buffer. By setting the ENABle parts of the status registers appropriately, you can arrange for arbitrary bits in an arbitrary status register to trigger an SRQ. To exploit the possibilities of the service request fully, all bits should be set to 1 in the enable registers SRE and ESE.

**Example: using the `*OPC` command to generate an SRQ.** While the program waits for the SRQ, it can perform other tasks:

- Set bit 0 in the ESE (Operation Complete)
- Set bit 5 in the SRE (ESB)

Once the settings are complete, the instrument generates an SRQ.

The SRQ is the only way the instrument can become active on its own initiative. Every controller program should configure the instrument so that a service request is triggered on a malfunction, and the program should respond to it appropriately. A detailed example of a service request routine is found in chapter 6 of the manual.

**Example: indicating the end of an averaging cycle with an SRQ via bit 10 of the STATus:OPERation register.** While the program waits for the SRQ, it can perform other tasks:

- Set bit 7 in the SRE (sum bit for the STATus:OPERation register)
- Set bit 10 in the STATus:OPERation:ENABle register (Averaging)
- Set bit 10 in STATus:OPERation:NTRansition to ensure that the transition of averaging bit 10 from 1 to 0 (Averaging) is also stored in the EVENt register. Calling the `*CLS` command sets all bits in NTRansition and PTRansition to 1, so that every bit change is registered. Enabling the enable bit, in this case bit 10, will normally be sufficient.

When the averaging cycle is complete, the instrument generates an SRQ.

#### Serial poll (GPIB only)

With a serial poll, the status byte of an instrument is queried, just as with the `*STB?` command. The query is, however, realized via interface messages and is faster. The serial poll method was already defined in IEEE 488.1 and used to be the only standardized way of polling the status byte across different instruments. The method also works with instruments that do not follow SCPI or IEEE 488.2.

The VISA function for performing a serial poll is `viReadSTB`. Serial poll is mainly used to get a quick overview of the state of several instruments connected to the IEC bus (GPIB).

#### Querying with commands

Every part of every status register can be read with queries. The individual commands are given in the detailed description of the status registers above. What is returned is always a number representing the bit pattern in the register queried. Evaluation of this number is done by the controller program.

Queries are normally used after an SRQ to get more detailed information about the cause of the SRQ.

#### Querying the error queue

Every error condition in the instrument results in an entry in the error queue. The entries in the error queue are detailed error messages in plain text that can be displayed in the ERROR menu via manual operation, or queried over the IEC bus with the command:

```
SYSTem:ERRor?
```

Each call of `SYSTem:ERRor?` fetches one entry from the error queue. When no error messages are stored any longer, the instrument replies with `0, "No error"`.

The error queue should be queried after every SRQ in the controller program, since the entries describe the cause of the error more precisely than the status registers. Especially during the test phase of a controller program, the error queue should be queried regularly, since erroneous commands from the controller to the instrument are also registered there.

### Resetting the status reporting system

Table 2-7 shows the various commands and events that cause the status reporting system to be reset. None of the commands, apart from `*RST` and `SYSTem:PRESet`, affect the instrument's functional settings. In particular, DCL does not change the instrument settings.

**Table 2-7. Resetting Instrument Functions**

| Effect | Switching on the supply voltage | DCL, SDC (Device Clear, Selected Device Clear) | `*RST` | `*CLS` |
|---|---|---|---|---|
| Clear STB, ESR | yes | — | — | yes |
| Clear SRE, ESE | yes | — | — | — |
| Clear the EVENt parts of the registers | yes | — | — | yes |
| Clear the ENABle parts of all OPERation and QUEStionable registers | yes | — | — | — |
| Fill the PTRansition parts with 1, clear the NTRansition parts | yes | — | — | — |
| Empty the error queue | yes | — | — | yes |
| Empty the output buffer | yes | yes | 1) | 1) |
| Reset command processing and the input buffer | yes | yes | — | — |

1) Any command that comes first in a command line, i.e. immediately after a `<PROGRAM MESSAGE TERMINATOR>`, empties the output buffer.

## Error messages

### Introduction

Error messages are entered in the error/event queue of the status reporting system when the instrument is in remote control mode, and can be read out with the command `SYSTem:ERRor?`. The instrument's response format to the command is:

```
<error code>, "<error description>;<remote control command concerned>"
```

Stating the remote control command, preceded by a semicolon, is optional.

**Example**

The command `TEST:COMMAND` gives the following reply to the query `SYSTem:ERRor?`:

```
-113,"Undefined header;TEST:COMMAND"
```

The lists below describe the error texts shown on the instrument. A distinction is made between error messages defined by SCPI, which are marked with negative error codes, and device-specific error messages, which use positive error codes.

In the tables below, the error text entered in the error/event queue — which can be read out with the query `SYSTem:ERRor?` — is given together with a short explanation of the cause of the error. The left-hand column contains the corresponding error code.

Events that generate command errors must not generate execution errors, device-specific errors or query errors; see the other error definitions in this chapter.

### Command Error

An `<error/event number>` in the range **[-199, -100]** indicates that the instrument's parser has detected an IEEE 488.2 syntax error. Any error in this class causes the command error bit (bit 5) in the event status register (IEEE 488.2, section 11.5.1) to be set. One of the following events has occurred:

- The parser has detected an IEEE 488.2 syntax error, i.e. a message from the controller to the device violates the IEEE 488.2 standard. Possible violations include a data element that violates the device's listening formats, or whose type is not accepted by the device.
- An unknown header was received. Unknown headers include erroneous device-specific headers and erroneous or unimplemented IEEE 488.2 common commands.

| Error code | Error text | Explanation |
|---|---|---|
| -100 | **Command error** | Generic syntax error for devices that cannot detect more specific errors. The code only indicates that a Command Error as defined in IEEE 488.2, 11.5.1.1.4 has occurred. |
| -101 | **Invalid character** | A syntactic element contains a character that is invalid for that type; for example, a header containing an ampersand, `SETUP&`. |
| -102 | **Syntax error** | An unknown command or data type was encountered; for example, a string was received when the device does not accept strings. |
| -103 | **Invalid separator** | The parser expected a separator and encountered an illegal character; for example, the semicolon was omitted after a program message unit, `*SRE 1:INP1:COUP AC`. |
| -104 | **Data type error** | The parser recognized a data element of a type other than allowed; for example, numeric data or string data was expected but block data was received. |
| -108 | **Parameter not allowed** | More parameters than expected were received for the header; for example, the common command `*SRE` accepts only one parameter, so `*SRE 2,1` is not allowed. |
| -109 | **Missing parameter** | Fewer parameters than required were received for the header; for example, the common command `*SRE` requires one parameter, so `*SRE` on its own is not allowed. |
| -110 | **Command header error** | An error was detected in the header. |
| -112 | **Program mnemonic too long** | The header contains more than twelve characters (see IEEE 488.2, 7.6.1.4.1). |
| -113 | **Undefined header** | The header is syntactically correct, but is undefined for this device; for example, `*XYZ` is not defined for any device. |
| -114 | **Header suffix out of range** | The value of a numeric suffix attached to a program mnemonic (see the section on syntax and style) makes the header invalid. |
| -120 | **Numeric data error** | Generated when parsing a data element that appears to be numeric, including the non-decimal numeric types. For example, `INP:GAIN 1.0X2` will generate this error. |
| -130 | **Suffix error** | This error, and errors -131 through -139, are generated when parsing a suffix. |
| -131 | **Invalid suffix** | The suffix does not follow the syntax described in IEEE 488.2, 7.7.3.2, or the suffix is not appropriate for this device. |
| -134 | **Suffix too long** | The suffix contained more than 12 characters (see IEEE 488.2, 7.7.3.4). |
| -138 | **Suffix not allowed** | A suffix was encountered after a numeric element that does not allow suffixes. |
| -140 | **Character data error** | Generated when parsing a character data element. For example, `INP:COUP XYZ` will generate this error. |
| -141 | **Invalid character data** | Either the character data element contains an invalid character, or the element received is not valid for the header. |
| -144 | **Character data too long** | The character data element contains more than twelve characters (see IEEE 488.2, 7.7.1.4). |
| -148 | **Character data not allowed** | A legal character data element was encountered where the device prohibits it. |
| -150 | **String data error** | Generated when parsing a string data element. For example, `FUNC "XYZ"` will generate this error. |
| -151 | **Invalid string data** | A string data element was expected, but was invalid for some reason (see IEEE 488.2, 7.7.5.2); for example, an END message was received before the closing quotation mark. |

### Execution Error

An `<error/event number>` in the range **[-299, -200]** indicates that an error has been detected by the instrument's execution control block. Any error in this class must cause the execution error bit (bit 4) in the event status register (IEEE 488.2, section 11.5.1) to be set. One of the following events has occurred:

- A `<PROGRAM DATA>` element following a header was judged by the device to lie outside the legal input range, or is otherwise incompatible with the device's capabilities.
- A valid program message could not be executed correctly because of a condition in the device.

Execution errors are reported by the instrument after rounding and expression evaluation have taken place. Rounding of a numeric data element, for example, will not be reported as an execution error.

| Error code | Error text | Explanation |
|---|---|---|
| -200 | **Execution error** | Generic error for devices that cannot detect more specific errors. The code only indicates that an Execution Error as defined in IEEE 488.2, 11.5.1.1.5 has occurred. |
| -203 | **Command protected** | Indicates that a legal, password-protected program command or query could not be executed because the command was disabled. |
| -212 | **Arm ignored** | Indicates that an arming signal was received and recognized by the device, but was ignored. The instrument generates this error when ARM is received without memory recording being configured. |
| -213 | **Init ignored** | Indicates that a request for a measurement was ignored because another measurement was already in progress. |
| -221 | **Settings conflict** | Indicates that a legal program data element was parsed but could not be executed because of the device's current state (see IEEE 488.2, 6.4.5.3 and 11.5.1.1.5). |
| -222 | **Data out of range** | Indicates that a legal program data element was parsed but could not be executed because the interpreted value lay outside the legal range as defined by the device (see IEEE 488.2, 11.5.1.1.5). |
| -223 | **Too much data** | Indicates that a legal program data element of block, expression or string type was received with more data than the device could handle because of memory or similar device-specific requirements. |
| -224 | **Illegal parameter value** | Used where an exact value from a list of possible values was expected. |
| -225 | **Out of memory** | The device does not have enough memory to perform the requested operation. |
| -230 | **Data corrupt or stale** | Possibly invalid data; a new reading has been started but not completed since the last access. |
| -240 | **Hardware error** | Indicates that a legal program command or query could not be executed because of a hardware problem in the device. |

### Device-Specific Error

An `<error/event number>` in the range **[-399, -300]** or **[1, 32767]** indicates that the instrument has detected an error, possibly caused by an abnormal hardware or firmware condition. These codes are also used for errors in self-test responses. Any error in this class causes the device-specific error bit (bit 3) in the event status register (IEEE 488.2, section 11.5.1) to be set.

| Error code | Error text | Explanation |
|---|---|---|
| -300 | **Device-specific error** | Generic device-dependent error for devices that cannot detect more specific errors. The code only indicates that a Device-Dependent Error as defined in IEEE 488.2, 11.5.1.1.6 has occurred. |
| -310 | **System error** | Indicates that an error the device designates as a "system error" has occurred. The code is device dependent. |
| -311 | **Memory error** | Indicates a physical error in the device's memory, for example a parity error. |
| -313 | **Calibration memory lost** | Indicates that non-volatile calibration data used by the `*CAL?` command has been lost. |
| -314 | **Save/recall memory lost** | Indicates that the non-volatile data stored with the `*SAV?` command has been lost. |
| -315 | **Configuration memory lost** | Indicates that non-volatile configuration data stored by the device has been lost. |
| -320 | **Storage fault** | Indicates that the firmware detected a fault when using data storage. The error is not an indication of physical damage or failure of any mass storage element. |
| -325 | **Sample factor adjusted** | Indicates that applying the current configuration caused the sample factor to be adjusted. |
| -326 | **Recording time too long** | Indicates that the data that would be collected during the stated recording time would not fit in the available memory. |
| -330 | **Self-test failed** | (No further explanation in the source.) |
| -340 | **Calibration failed** | (No further explanation in the source.) |
| -350 | **Queue overflow** | A specific code entered into the queue in place of the code that caused the error. The code indicates that there is no room in the queue and that an error occurred but was not recorded. |
| -360 | **Communication error** | (No further explanation in the source.) |

### Query Error

An `<error/event number>` in the range **[-499, -400]** indicates that the instrument's output queue control has detected a problem with the message exchange protocol described in IEEE 488.2, chapter 6. Any error in this class causes the query error bit (bit 2) in the event status register (IEEE 488.2, section 11.5.1) to be set. These errors correspond to the message exchange protocol errors described in IEEE 488.2, section 6.5. One of the following is the case:

- An attempt is made to read data from the output queue when no output is either present or in progress.
- Data in the output queue has been lost.
- Events that generate query errors must not generate command errors, execution errors or device-specific errors; see the other error definitions in this chapter.

| Error code | Error text | Explanation |
|---|---|---|
| -400 | **Query error** | Generic query error for devices that cannot detect more specific errors. The code only indicates that a Query Error as defined in IEEE 488.2, 11.5.1.1.7 and 6.3 has occurred. |
| -410 | **Query INTERRUPTED** | Indicates that a condition causing an INTERRUPTED Query error has occurred (see IEEE 488.2, 6.3.2.3); for example, a query followed by DAB or GET before a response had been fully sent. |
| -420 | **Query UNTERMINATED** | Indicates that a condition causing an UNTERMINATED Query error has occurred (see IEEE 488.2, 6.3.2.2); for example, the device was addressed to talk and an incomplete program message was received. |
| -430 | **Query DEADLOCKED** | Indicates that a condition causing a DEADLOCKED Query error has occurred (see IEEE 488.2, 6.3.1.7); for example, both the input buffer and the output buffer are full and the device cannot continue. |
| -440 | **Query UNTERMINATED after indefinite response** | Indicates that a query was received in the same program message after a query requesting an indefinite response was executed (see IEEE 488.2, 6.5.7.5). |

## Programming examples

This chapter reproduces the examples from chapter 6 “Programming Examples” in the Remote Control Users Guide. The examples show how the instrument is programmed and can be used as a starting point for more complex programming tasks.

### Introduction

In these examples the interface (RS-232 / GPIB / Ethernet) can be selected by setting the `INTFC` constant to the corresponding `INTFC_…` constant. Communication parameters (e.g. serial port, baud rate, IP address and so on) are set with the `RSRC_NAME` and `RSRC_ATTR_…` constants.

The programming examples are written in ANSI C with a VISA library implemented according to version 2.2 of the VISA specification (www.vxipnp.org), for example National Instruments VISA 2.5 or later. It is possible to communicate with the instrument over the RS-232 or Ethernet interface using only basic operating system APIs (e.g. Win32 or UNIX), that is, without a VISA library. There is one such example for the Ethernet interface (using the Win32 API) later in this chapter — see [“U, I, P Measurement over Ethernet Interface without VISA Library”](#u-i-p-measurement-over-ethernet-interface-without-visa-library--raw-tcp-socket), which is the most relevant one if you are going to build your own TCP client.

Note the common pattern in all the examples:

- The VISA resource name for Ethernet is `"TCPIP::192.168.2.251::23::SOCKET"` — that is, a raw TCP socket to the instrument's IP address on **port 23**.
- All commands are sent as ASCII text terminated with a line feed (`\n`); responses are read back as lines of text.

### Initialize Interface — initializing the interface

The interface must be initialized before any communication with the instrument takes place.

This program opens a VISA session to the instrument. The program uses the RS-232, GPIB or Ethernet interface depending on the setting of the `INTFC` constant, and sets the I/O timeout to 10 seconds. For your own TCP client it is worth noting the LAN resource name (`TCPIP::<ip>::23::SOCKET`) and the timeout value (10 s), which is a sensible starting point for a socket-based client too.

```c
/*
 * INITIALIZE INTERFACE
 *
 * This program will open VISA session to the instrument.
 * The program uses RS-232, GPIB or ethernet interface depending
 * on the setting of 'INTFC' constant and sets the I/O timeout
 * to 10 seconds.
 */

#include <visa.h>
#include <stdio.h>

#define INTFC_RS232 1   /* RS-232 interface */
#define INTFC_GPIB  2   /* GPIB / IEEE 488.2 interface */
#define INTFC_LAN   3   /* ethernet / IEEE 802.3 interface */
#define INTFC_USB   4   /* USB interface (Virtual COM Port) */

#if 1
#define INTFC   INTFC_RS232
#elif 1
#define INTFC   INTFC_GPIB
#elif 1
#define INTFC   INTFC_LAN
#else
#define INTFC   INTFC_USB
#endif

#if INTFC == INTFC_RS232
#   define RSRC_NAME            "ASRL1"                 /* COM1 */
#   define RSRC_ATTR_BAUD       115200                  /* baud rate */
#   define RSRC_ATTR_FLOW_CNTRL VI_ASRL_FLOW_RTS_CTS    /* flow control */
#elif INTFC == INTFC_GPIB
#   define RSRC_NAME            "GPIB::5"
#elif INTFC == INTFC_LAN
#   define RSRC_NAME            "TCPIP::192.168.2.251::23::SOCKET"
#elif INTFC == INTFC_USB
#   define RSRC_NAME            "ASRL2"                /* COM2 */
#else
#error 'INTFC': unsupported value
#endif

ViSession  rm,      // Default resource manager session
           vi;      // VISA session

int main(int argc,char *argv[])
{
#define CHECK_STATUS(cond,func,status)      {                           \
        if ( cond )                                                     \
        {                                                               \
            fprintf(stderr,"%s failed, status 0x%lX\n",func,status);    \
            return 1;                                                   \
        }                                                               \
    }

    ViStatus status;

    (void) argc;
    (void) argv;

    /* Open default resource manager: */
    status = viOpenDefaultRM (&rm);
    CHECK_STATUS(status < VI_SUCCESS,"viOpenDefaultRM()",status);

    /* Open VISA session to the instrument: */
    status = viOpen (rm,RSRC_NAME,VI_NULL,0,&vi);
    CHECK_STATUS(status != VI_SUCCESS,"viOpen()",status);

    /* Set timeout to 10 seconds: */
    status = viSetAttribute (vi, VI_ATTR_TMO_VALUE, 10000);
    CHECK_STATUS(status != VI_SUCCESS,"viSetAttribute()",status);

    /* Close VISA session to the instrument: */
    status = viClose (vi);
    CHECK_STATUS(status != VI_SUCCESS,"viClose(vi)",status);

    /* Close session to the default resource manager: */
    status = viClose(rm);
    CHECK_STATUS(status != VI_SUCCESS,"viClose(rm)",status);

    puts("OK");

    return 0;

#undef CHECK_STATUS
}
```

### Initialize Instrument — identification and reset

Before communicating further, the instrument's identity should be verified and the instrument should be put into a known (default) state.

This program opens a VISA session to the instrument, reads the ID string and resets the instrument. The SCPI sequence — send `*IDN?`, read the response, send `*RST` — is directly reusable in your own TCP client as a startup “handshake”: it confirms you are talking to the right instrument and gives you a known initial state.

```c
/*
 * IDENTIFY AND RESET THE INSTRUMENT
 *
 * This program will open VISA session to the instrument, read ID string
 * and reset the instrument.
 */

#include <visa.h>
#include <stdio.h>

#define INTFC_RS232 1   /* RS-232 interface */
#define INTFC_GPIB  2   /* GPIB / IEEE 488.2 interface */
#define INTFC_LAN   3   /* ethernet / IEEE 802.3 interface */
#define INTFC_USB   4   /* USB interface (Virtual COM Port) */

#if 1
#define INTFC   INTFC_RS232
#elif 1
#define INTFC   INTFC_GPIB
#elif 1
#define INTFC   INTFC_LAN
#else
#define INTFC   INTFC_USB
#endif

#if INTFC == INTFC_RS232
#   define RSRC_NAME            "ASRL1"                 /* COM1 */
#   define RSRC_ATTR_BAUD       115200                  /* baud rate */
#   define RSRC_ATTR_FLOW_CNTRL VI_ASRL_FLOW_RTS_CTS    /* flow control */
#elif INTFC == INTFC_GPIB
#   define RSRC_NAME            "GPIB::5"
#elif INTFC == INTFC_LAN
#   define RSRC_NAME            "TCPIP::192.168.2.251::23::SOCKET"
#elif INTFC == INTFC_USB
#   define RSRC_NAME            "ASRL2"                /* COM2 */
#else
#error 'INTFC': unsupported value
#endif

ViSession   rm,         // Default resource manager session
            vi;         // VISA session
ViChar      buffer[512];    // buffer to hold instrument response
ViUInt32    retCnt;         // number of bytes read from the instrument

int main(int argc,char *argv[])
{
#define CHECK_STATUS(cond,func,status)      {                           \
        if ( cond )                                                     \
        {                                                               \
            fprintf(stderr,"%s failed, status 0x%lX\n",func,status);    \
            return 1;                                                   \
        }                                                               \
    }

    ViStatus status;

    (void) argc;
    (void) argv;

    /* Open default resource manager: */
    status = viOpenDefaultRM (&rm);
    CHECK_STATUS(status < VI_SUCCESS,"viOpenDefaultRM()",status);

    /* Open VISA session to the instrument: */
    status = viOpen (rm,RSRC_NAME,VI_NULL,0,&vi);
    CHECK_STATUS(status != VI_SUCCESS,"viOpen()",status);

#if INTFC == INTFC_RS232
    /* set RS-232 I/O attributes (transmission parameters): */
    viSetAttribute (vi, VI_ATTR_ASRL_BAUD, RSRC_ATTR_BAUD);
    viSetAttribute (vi, VI_ATTR_ASRL_FLOW_CNTRL, RSRC_ATTR_FLOW_CNTRL);
#endif

    /* Query the instrument's ID string: */
    viPrintf (vi, "*IDN?\n");
    /* Read the ID string into buffer: */
    viRead (vi, buffer, 256, &retCnt);
    /* Print contens of the buffer on screen: */
    printf (buffer);
    /* Bring the instrument into default state: */
    viPrintf (vi, "*RST\n");

    /* Close VISA session to the instrument: */
    status = viClose (vi);
    CHECK_STATUS(status != VI_SUCCESS,"viClose(vi)",status);

    /* Close session to the default resource manager: */
    status = viClose(rm);
    CHECK_STATUS(status != VI_SUCCESS,"viClose(rm)",status);

    return 0;

#undef CHECK_STATUS
}
```

### Perform Simple Power Measurement — a simple power measurement

Valid signals should be connected to the instrument's input channels, otherwise the measurement value may be invalid.

The program opens a VISA session and performs a simple power measurement, waiting synchronously (blocking in `viRead` while waiting for the measurement). The time it takes to measure the power depends on the voltage and current signals connected to the instrument. The default averaging interval (equal to the measurement time) is 300 ms. If `viRead` times out before the measured power is returned, the VISA timeout must be increased; use the `viSetAttribute` function to change the timeout value (the default is 10 s).

The reusable SCPI sequence for your own client is: `*RST` → wait for autorange → `*TRG` (trigger a measurement) → wait → `DATA? "POW:ACT"` (fetch active power) → read the response line.

```c
/*
 * MEASURE POWER (wait synchronously)
 *
 * This program will open VISA session to the instrument and perform a simple
 * power measurement. It dwells in viRead function while waiting
 * for measurement.
 *
 * The time it will take to measure the power depends on the voltage and
 * current signals attached to the instrument. Default averaging interval
 * (equals to measurement time) is 300 ms. In the case viRead times out before
 * the measured power is returned, VISA timeout must be increased. Use
 * function viSetAttribute to change the timeout value (default is 10 sec).
 */

#include <visa.h>
#include <stdio.h>

#define INTFC_RS232 1   /* RS-232 interface */
#define INTFC_GPIB  2   /* GPIB / IEEE 488.2 interface */
#define INTFC_LAN   3   /* ethernet / IEEE 802.3 interface */
#define INTFC_USB   4   /* USB interface (Virtual COM Port) */

#if 1
#define INTFC   INTFC_RS232
#elif 1
#define INTFC   INTFC_GPIB
#elif 1
#define INTFC   INTFC_LAN
#else
#define INTFC   INTFC_USB
#endif

#if INTFC == INTFC_RS232
#   define RSRC_NAME            "ASRL1"                 /* COM1 */
#   define RSRC_ATTR_BAUD       115200                  /* baud rate */
#   define RSRC_ATTR_FLOW_CNTRL VI_ASRL_FLOW_RTS_CTS    /* flow control */
#elif INTFC == INTFC_GPIB
#   define RSRC_NAME            "GPIB::5"
#elif INTFC == INTFC_LAN
#   define RSRC_NAME            "TCPIP::192.168.2.251::23::SOCKET"
#elif INTFC == INTFC_USB
#   define RSRC_NAME            "ASRL2"                /* COM2 */
#else
#error 'INTFC': unsupported value
#endif

ViSession   rm,             // Default resource manager session
            vi;             // VISA session
ViChar      buffer[512];    // buffer to hold instrument response
ViUInt32    retCnt;         // number of bytes read from the instrument

#if WIN32
#include <windows.h>
static void Delay(double seconds)
{
    Sleep((DWORD)(seconds * 1000));
}
#endif

int main(int argc,char *argv[])
{
#define CHECK_STATUS(cond,func,status)      {                           \
        if ( cond )                                                     \
        {                                                               \
            fprintf(stderr,"%s failed, status 0x%lX\n",func,status);    \
            return 1;                                                   \
        }                                                               \
    }

    ViStatus status;

    (void) argc;
    (void) argv;

    /* Open default resource manager: */
    status = viOpenDefaultRM (&rm);
    CHECK_STATUS(status < VI_SUCCESS,"viOpenDefaultRM()",status);

    /* Open VISA session to the instrument with GPIB address 5: */
    status = viOpen (rm,RSRC_NAME,VI_NULL,0,&vi);
    CHECK_STATUS(status != VI_SUCCESS,"viOpen()",status);

    /* Set timeout to 10 seconds: */
    status = viSetAttribute (vi, VI_ATTR_TMO_VALUE, 10000);
    CHECK_STATUS(status != VI_SUCCESS,"viSetAttribute()",status);

#if INTFC == INTFC_RS232
    /* set RS-232 I/O attributes (transmission parameters): */
    viSetAttribute (vi, VI_ATTR_ASRL_BAUD, RSRC_ATTR_BAUD);
    viSetAttribute (vi, VI_ATTR_ASRL_FLOW_CNTRL, RSRC_ATTR_FLOW_CNTRL);
#endif

#if 0
    /* enable termination character for read operations: */
    viSetAttribute (vi, VI_ATTR_TERMCHAR, '\n');
    viSetAttribute (vi, VI_ATTR_TERMCHAR_EN, VI_TRUE);
#if INTFC == INTFC_RS232
    viSetAttribute (vi, VI_ATTR_ASRL_END_IN, VI_ASRL_END_TERMCHAR);
#endif
#endif

    viPrintf (vi, "*RST\n");    /* Bring the instrument into a default state */
    Delay (3.0);        /* Wait 3 seconds for autorange to complete */
    viPrintf (vi, "*TRG\n");                /* Trigger a measurement */
    Delay (2.0);                            /* Wait 2 seconds */
    viPrintf (vi, "DATA? \"POW:ACT\"\n");   /* Query the power */
    memset(buffer,0,sizeof(buffer));        /* Clear buffer */
    viRead (vi, buffer, 256, &retCnt);      /* Read value */
    printf(buffer); /* Print the value on the screen */

    /* Close VISA session to the instrument: */
    status = viClose (vi);
    CHECK_STATUS(status != VI_SUCCESS,"viClose(vi)",status);

    /* Close session to the default resource manager: */
    status = viClose(rm);
    CHECK_STATUS(status != VI_SUCCESS,"viClose(rm)",status);

    return 0;

#undef CHECK_STATUS
}
```

### U, I, P Measurement — voltage, current and power measurement

This example configures the instrument to measure power, voltage and current on a three-phase system 3 x 400V/50Hz and reads the measurements.

The time it takes to complete the measurement is 1 s (the averaging time, set with `APER 1.0`). The `DATA?` query does not wait for the measurement to complete — it returns the values available at that moment. The delay of 2 seconds gives the instrument enough time to complete the measurement before the data is read.

The configuration sequence here (`ROUT:SYST`, `SYNC:SOUR`, `VOLT1:RANG`, `CURR1:RANG:AUTO`, `APER`, `FUNC`, `INIT:CONT ON`, then `DATA?`) is exactly the same as the one used in the VISA-free Ethernet example further down — this is the SCPI recipe you reuse in your own TCP client.

```c
/*
 * MEASURE U, I, P (wait synchronously)
 *
 * This example will configure the instrument to measure power, voltage
 * and current on three-phase system 3 x 400V/50Hz and reads the measurements.
 *
 * The time it will take to finish the measurement is 1 s (averaging time).
 * The query DATA? does not wait for the measurement to be completed, so it
 * would return whatever values are available at a moment. The delay of 2
 * seconds gives the instrument enough time to finish the measurement before
 * reading data.
 */

#include <visa.h>
#include <stdio.h>

#define INTFC_RS232 1   /* RS-232 interface */
#define INTFC_GPIB  2   /* GPIB / IEEE 488.2 interface */
#define INTFC_LAN   3   /* ethernet / IEEE 802.3 interface */
#define INTFC_USB   4   /* USB interface (Virtual COM Port) */

#if 1
#define INTFC   INTFC_RS232
#elif 1
#define INTFC   INTFC_GPIB
#elif 1
#define INTFC   INTFC_LAN
#else
#define INTFC   INTFC_USB
#endif

#if INTFC == INTFC_RS232
#   define RSRC_NAME            "ASRL1"                 /* COM1 */
#   define RSRC_ATTR_BAUD       115200                  /* baud rate */
#   define RSRC_ATTR_FLOW_CNTRL VI_ASRL_FLOW_RTS_CTS    /* flow control */
#elif INTFC == INTFC_GPIB
#   define RSRC_NAME            "GPIB::5"
#elif INTFC == INTFC_LAN
#   define RSRC_NAME            "TCPIP::192.168.2.251::23::SOCKET"
#elif INTFC == INTFC_USB
#   define RSRC_NAME            "ASRL2"                /* COM2 */
#else
#error 'INTFC': unsupported value
#endif

ViSession   rm,             // Default resource manager session
            vi;             // VISA session
ViChar      buffer[512];    // buffer to hold instrument response
ViUInt32    retCnt;         // number of bytes read from the instrument

#if WIN32
#include <windows.h>
static void Delay(double seconds)
{
    Sleep((DWORD)(seconds * 1000));
}
#endif

int main(int argc,char *argv[])
{
#define CHECK_STATUS(cond,func,status)      {                           \
        if ( cond )                                                     \
        {                                                               \
            fprintf(stderr,"%s failed, status 0x%lX\n",func,status);    \
            return 1;                                                   \
        }                                                               \
    }

    ViStatus status;

    (void) argc;
    (void) argv;

    /* Open default resource manager: */
    status = viOpenDefaultRM (&rm);
    CHECK_STATUS(status < VI_SUCCESS,"viOpenDefaultRM()",status);

    /* Open VISA session to the instrument with GPIB address 5: */
    status = viOpen (rm,RSRC_NAME,VI_NULL,0,&vi);
    CHECK_STATUS(status != VI_SUCCESS,"viOpen()",status);

    /* Set timeout to 10 seconds: */
    status = viSetAttribute (vi, VI_ATTR_TMO_VALUE, 10000);
    CHECK_STATUS(status != VI_SUCCESS,"viSetAttribute()",status);

#if INTFC == INTFC_RS232
    /* set RS-232 I/O attributes (transmission parameters): */
    viSetAttribute (vi, VI_ATTR_ASRL_BAUD, RSRC_ATTR_BAUD);
    viSetAttribute (vi, VI_ATTR_ASRL_FLOW_CNTRL, RSRC_ATTR_FLOW_CNTRL);
#endif

    /* Bring the instrument into a default state: */
    viPrintf (vi, "*RST\n");
    /* Select three wattmeter configuration: */
    viPrintf (vi, "ROUT:SYST \"3W\"\n");
    /* SYNC source = voltage phase 1: */
    viPrintf (vi, "SYNC:SOUR VOLT1\n");
    /* Set voltage range on voltage channel 1 to 300 V: */
    viPrintf (vi, "VOLT1:RANG 300.0\n");
    /* Set current channel 1 to autorange: */
    viPrintf (vi, "CURR1:RANG:AUTO ON\n");
    /* Set averaging time to 1 second: */
    viPrintf (vi, "APER 1.0\n");
    /* Select U, I, P measurement: */
    viPrintf (vi, "FUNC \"VOLT1\",\"CURR1\",\"POW1:ACT\"\n");
    /* Run continuous measurements: */
    viPrintf (vi, "INIT:CONT ON\n");

    Delay (2.0);        /* Wait 2 seconds */

    viPrintf (vi, "DATA?\n");           /* Query the measurement */
    memset(buffer,0,sizeof(buffer));    /* Clear buffer */
    viRead (vi, buffer, 256, &retCnt);  /* Read values */
    printf (buffer);                    /* Print the value on the screen */

    /* Close VISA session to the instrument: */
    status = viClose (vi);
    CHECK_STATUS(status != VI_SUCCESS,"viClose(vi)",status);

    /* Close session to the default resource manager: */
    status = viClose(rm);
    CHECK_STATUS(status != VI_SUCCESS,"viClose(rm)",status);

    return 0;

#undef CHECK_STATUS
}
```

### Continuous Power Measurement

Valid signals should be connected to the instrument's input channels, otherwise the measurement value may be invalid.

The program performs a continuous power measurement. Bit 10 (value `0x400`) in the operation status register is used to detect the end of the averaging interval — this ensures that the newest measurement is fetched and displayed immediately. The default averaging interval is 300 ms; if `viRead` times out before the measured power is returned, the VISA timeout must be increased with `viSetAttribute` (the default is 10 s).

The polling pattern transfers directly to a TCP client: send `*CLS` (clear the previous event), poll `STAT:OPER?` until bit 10 is set, and then fetch fresh values with `DATA? "POW"`. This is the recommended way to synchronize readings with the instrument's measurement rate, instead of using fixed wait times.

```c
/*
 * CONTINUOUS POWER MEASUREMENT
 *
 * This program will open VISA session to the instrument and perform continuous
 * power measurement. Bit 10 of the operating status register is used
 * to determine end of averaging interval. This way it is assured that
 * the newest measurement is immediately fetched and displayed.
 *
 * The time it will take to measure the power depends on the voltage and
 * current signals attached to the instrument. Default averaging interval
 * (equals to measurement time) is 300 ms. In the case viRead times out before
 * the measured power is returned, VISA timeout must be increased.
 * Use function viSetAttribute to change the timeout value (default is 10 sec).
 */

#include <visa.h>
#include <stdio.h>

#define INTFC_RS232 1   /* RS-232 interface */
#define INTFC_GPIB  2   /* GPIB / IEEE 488.2 interface */
#define INTFC_LAN   3   /* ethernet / IEEE 802.3 interface */
#define INTFC_USB   4   /* USB interface (Virtual COM Port) */

#if 1
#define INTFC   INTFC_RS232
#elif 1
#define INTFC   INTFC_GPIB
#elif 1
#define INTFC   INTFC_LAN
#else
#define INTFC   INTFC_USB
#endif

#if INTFC == INTFC_RS232
#   define RSRC_NAME            "ASRL1"                 /* COM1 */
#   define RSRC_ATTR_BAUD       115200                  /* baud rate */
#   define RSRC_ATTR_FLOW_CNTRL VI_ASRL_FLOW_RTS_CTS    /* flow control */
#elif INTFC == INTFC_GPIB
#   define RSRC_NAME            "GPIB::5"
#elif INTFC == INTFC_LAN
#   define RSRC_NAME            "TCPIP::192.168.2.251::23::SOCKET"
#elif INTFC == INTFC_USB
#   define RSRC_NAME            "ASRL2"                /* COM2 */
#else
#error 'INTFC': unsupported value
#endif

ViSession   rm,                 // Default resource manager session
            vi;                 // VISA session
ViChar      buffer[512];        // buffer to hold instrument response
ViUInt32    retCnt;             // number of bytes read from the instrument
ViUInt16    opStat;

#if WIN32
#include <windows.h>
static void Delay(double seconds)
{
    Sleep((DWORD)(seconds * 1000));
}
#endif

int main(int argc,char *argv[])
{
#define CHECK_STATUS(cond,func,status)      {                           \
        if ( cond )                                                     \
        {                                                               \
            fprintf(stderr,"%s failed, status 0x%lX\n",func,status);    \
            return 1;                                                   \
        }                                                               \
    }

    ViStatus status;

    (void) argc;
    (void) argv;

    /* Open default resource manager: */
    status = viOpenDefaultRM (&rm);
    CHECK_STATUS(status < VI_SUCCESS,"viOpenDefaultRM()",status);

    /* Open VISA session to the instrument with GPIB address 5: */
    status = viOpen (rm,RSRC_NAME,VI_NULL,0,&vi);
    CHECK_STATUS(status != VI_SUCCESS,"viOpen()",status);

    /* Set timeout to 10 seconds: */
    status = viSetAttribute (vi, VI_ATTR_TMO_VALUE, 10000);
    CHECK_STATUS(status != VI_SUCCESS,"viSetAttribute()",status);

#if INTFC == INTFC_RS232
    /* set RS-232 I/O attributes (transmission parameters): */
    viSetAttribute (vi, VI_ATTR_ASRL_BAUD, RSRC_ATTR_BAUD);
    viSetAttribute (vi, VI_ATTR_ASRL_FLOW_CNTRL, RSRC_ATTR_FLOW_CNTRL);
#endif

    viPrintf (vi, "*RST\n");    /* Bring the instrument into a default state */
    Delay (3.0);                /* Wait 3 seconds for autorange to complete */
    viPrintf (vi, "*TRG\n");    /* Trigger a measurement */
    Delay (2.0);                /* Wait 2 seconds */

    while (1)
    {
        opStat = 0;
        viPrintf (vi, "*CLS\n");    /* Clear previously detected event */
        do                          /* Run this loop until bit 10 is set */
        {
            /* Query the OPER:STAT register value: */
            viPrintf (vi, "STAT:OPER?\n");
            memset(buffer,0,sizeof(buffer));    /* Clear buffer */
            viRead (vi, buffer, 256, &retCnt);  /* Read value */
            opStat = atoi (buffer);
#if 0
            printf("opStat: 0x%X\n",(unsigned int)opStat);
#endif
        } while ((opStat & 0x400) == 0);

        viPrintf (vi, "DATA? \"POW\"\n");   /* Query the power measurement */
        viRead (vi, buffer, 256, &retCnt);  /* Read value */

        printf (buffer);        /* Print the value on the screen */
    }

    /* Close VISA session to the instrument: */
    status = viClose (vi);
    CHECK_STATUS(status != VI_SUCCESS,"viClose(vi)",status);

    /* Close session to the default resource manager: */
    status = viClose(rm);
    CHECK_STATUS(status != VI_SUCCESS,"viClose(rm)",status);

    return 0;

#undef CHECK_STATUS
}
```

### U, I, P Measurement over Ethernet Interface without VISA Library — raw TCP socket

**This is the key example for your own application talking directly to the instrument over TCP/IP, without a VISA library.** The example configures the instrument to measure power, voltage and current on a three-phase system 3 x 400V/50Hz and reads the measurements. It uses the Win32 Winsock API, but the pattern is identical in any language/environment with TCP sockets (Python, C#, Java, Node.js, UNIX sockets and so on).

The time it takes to complete the measurement is 1 s (the averaging time). The `DATA?` query does not wait for the measurement to complete, so it returns the values available at that moment; the delay of 2 seconds gives the instrument time to complete the measurement before the data is read.

What is directly reusable for your own TCP client:

- **Connecting:** open a TCP stream socket (`AF_INET`, `SOCK_STREAM`) and connect to the instrument's IP address (`HOST`, here `192.168.2.251`) on **port 23** (`PORT`).
- **Sending (`socket_puts`):** each SCPI command is sent as plain ASCII text with `\n` (a line feed) appended at the end.
- **Receiving (`socket_gets`):** make one `recv()` call (up to 1024 bytes) and cut the string at the first `\n`; any `\r` immediately before it is removed, and the line is null-terminated. That is the whole “protocol” — line-based text over TCP. Note that the code does not read in a loop: a robust client should read repeatedly until `\n` has been received, since TCP does not guarantee that the whole response line arrives in a single `recv()` call (cf. `sio_gets` in the RS-232 example, which reads character by character until `\n`).
- **The SCPI sequence** in `main()` (`*RST`, `ROUT:SYST "3W"`, `SYNC:SOUR VOLT1`, `VOLT1:RANG 300.0`, `CURR1:RANG:AUTO ON`, `APER 1.0`, `FUNC …`, `INIT:CONT ON`, `DATA?`) can be copied unchanged.
- The commented-out `*IDN?` block (`#if 0 … #endif`) shows how to verify the connection at startup.

```c
/*
 * MEASURE U, I, P (wait synchronously)
 *
 * This example will configure the instrument to measure power, voltage
 * and current on three-phase system 3 x 400V/50Hz and reads the measurements.
 *
 * The time it will take to finish the measurement is 1 s (averaging time).
 * The query DATA? does not wait for the measurement to be completed, so it
 * would return whatever values are available at a moment. The delay of 2
 * seconds gives the instrument enough time to finish the measurement before
 * reading data.
 */

#if WIN32
#define WIN 1
#endif

#if WIN
#if !defined(_MFC_VER)      /* !MFC (!<afx.h>) */
#include <winsock2.h>
#include <windows.h>
#endif  /* !_MFC_VER */
#endif
#include <stdio.h>

#define HOST    "192.168.2.251"
#define PORT    23

#if WIN
#define SOCKET_HANDLE_FMT_PFX   ""
#define SOCKET_HANDLE_FMT       "u"     /* "%u" - 'typedef u_int SOCKET' */
typedef SOCKET socket_handle_t;
#endif

typedef struct {
    socket_handle_t h;
} *socket_t;

/******************************************************************************/
static void Delay(double seconds)
{
    Sleep((DWORD)(seconds * 1000));
}

/******************************************************************************/
void wsa_error(char *func,int error)
{
    fprintf(stderr,"%s() failed, error %d\n",
        func,error == -1 ? WSAGetLastError() : error);
}

/******************************************************************************/
int socket_setup(void)
{
#if WIN

    WSADATA wsaData;
    int wsaerrno;

    /*
     * Initialize Windows Socket DLL
     */
    if ( (wsaerrno = WSAStartup(
        MAKEWORD(1,1),  /* at least version 1.1 */
        &wsaData)) != 0 )
    {
        wsa_error("WSAStartup",wsaerrno);
        return -1;
    }

#endif  /* WIN */

    return 0;   /* OK */
}

/******************************************************************************/
int socket_cleanup(void)
{
#if WIN
    if ( WSACleanup() == SOCKET_ERROR )
        wsa_error("WSACleanup",-1);
#endif

    return 0;   /* OK */
}

/******************************************************************************/
socket_t socket_create(void)
{
    socket_handle_t sh;
    socket_t s;

    sh = socket(AF_INET,SOCK_STREAM,0);
#if WIN
    if ( sh == INVALID_SOCKET )
    {
        wsa_error("socket",-1);
        return NULL;
    }
#endif

    s = calloc(1,sizeof(*s));
    if ( !s )
        return NULL;

    s->h = sh;

    return s;   /* OK */
}

/******************************************************************************/
int socket_connect(socket_t s,struct sockaddr *addr,int addrlen)
{
    int ret = 0;    /* OK */

#if WIN
    if ( connect(s->h,addr,addrlen) == SOCKET_ERROR )
    {
        wsa_error("connect",-1);
        return -1;
    }
#endif

    return 0;   /* OK */
}

/******************************************************************************/
int socket_recv(socket_t s,void *buf,int len,int flags)
{
    register int l;

#if WIN
    l = recv(s->h,buf,len,flags);
    if ( l == SOCKET_ERROR )
    {
        wsa_error("recv",-1);
        return -1;
    }
#endif

    return l;
}

/******************************************************************************/
int socket_send(socket_t s,void *buf,int len,int flags)
{
    register int slen;  /* sent length */

#if WIN
    slen = send(s->h,buf,len,flags);
    if ( slen == SOCKET_ERROR )
    {
        wsa_error("send",-1);
        return -1;
    }
#endif

    return slen;
}

/******************************************************************************/
int socket_puts(socket_t s,char *str)
{
    char buf[1024];

    strcpy(buf,str);
    strcat(buf,"\n");
    if ( socket_send(s,buf,strlen(buf),0) < 0 )
        return -1;

    return 0;
}

/******************************************************************************/
int socket_gets(socket_t s,char *str)
{
    char buf[1024];
    char *p;

    if ( socket_recv(s,buf,sizeof(buf),0) < 0 )
        return -1;

    if ( (p = memchr(buf,'\n',sizeof(buf))) != NULL )
    {
        if ( p > buf && p[-1] == '\r' )
            p--;
        *p = '\0';
    }
    else
        buf[sizeof(buf)-1] = '\0';

    strcpy(str,buf);

    return strlen(str);
}

/******************************************************************************/
int main(int argc,char *argv[])
{
    socket_t s;
    struct sockaddr_in saddr;
    struct sockaddr_in *addr_in = (struct sockaddr_in *)&saddr;
    char buffer[1024];

    /* socket (TCP/IP) API initialization: */
    if ( socket_setup() < 0 )
        return 1;

    /*
     * Connect to the instrument:
     */
    /* set destination IP address and TCP port: */
    memset(addr_in,0,sizeof(struct sockaddr_in));
    addr_in->sin_family = AF_INET;
    addr_in->sin_port = htons(PORT);
    addr_in->sin_addr.s_addr = inet_addr(HOST);
    /* create socket: */
    s = socket_create();
    if ( !s )
        return 1;
#if 0
    fprintf(stderr,"socket_connect() ...\n");
#endif
    if ( socket_connect(s,(struct sockaddr *)&saddr,sizeof(saddr)) < 0 )
        return 1;
#if 0
    fprintf(stderr,"socket_connect(): done\n");
#endif

#if 0
    if ( socket_puts(s,"*IDN?") < 0 )
        return 1;
    if ( socket_gets(s,buffer) < 0 )
        return 1;
    puts(buffer);
#endif

    /* Bring the instrument into a default state: */
    socket_puts(s,"*RST");
    /* Select three wattmeter configuration: */
    socket_puts(s,"ROUT:SYST \"3W\"");
    /* SYNC source = voltage phase 1: */
    socket_puts(s,"SYNC:SOUR VOLT1");
    /* Set voltage range on voltage channel 1 to 300 V: */
    socket_puts(s,"VOLT1:RANG 300.0");
    /* Set current channel 1 to autorange: */
    socket_puts(s,"CURR1:RANG:AUTO ON");
    /* Set averaging time to 1 second: */
    socket_puts(s,"APER 1.0");
    /* Select U, I, P measurement: */
    socket_puts(s,"FUNC \"VOLT1\",\"CURR1\",\"POW1:ACT\"");
    /* Run continuous measurements: */
    socket_puts(s,"INIT:CONT ON");

    Delay(2.0);        /* Wait 2 seconds */

    socket_puts(s,"DATA?");             /* Query the measurement */
    memset(buffer,0,sizeof(buffer));    /* Clear buffer */
    socket_gets(s,buffer);              /* Read values */
    puts(buffer);                       /* Print the value on the screen */

    socket_cleanup();

    return 0;
}
```

#### The protocol in summary, for your own TCP client

Based on the example above, everything you need to talk to the instrument from your own code is:

1. Open a TCP connection to the instrument's IP address, port 23.
2. Send SCPI commands as ASCII lines terminated with `\n`.
3. For queries (commands ending in `?`): read the response as a single line of text terminated with `\n` (possibly `\r\n` — strip the `\r`).
4. Configure the measurement once (`*RST`, `ROUT:SYST`, `SYNC:SOUR`, ranges, `APER`, `FUNC`, `INIT:CONT ON`), then fetch values with `DATA?` as often as you like — optionally synchronized against `STAT:OPER?` bit 10, as the continuous example shows.

### U, I, P Measurement over RS-232 Interface without VISA Library

This example does the same as the Ethernet example above — configures the instrument to measure power, voltage and current on a three-phase system 3 x 400V/50Hz and reads the measurements — but over RS-232 with the Win32 API (`CreateFile`/`ReadFile`/`WriteFile`) instead of sockets. It is mainly relevant as a reference if you need serial communication; for a TCP application the Ethernet example above is the one to use. Note that the SCPI sequence in `main()` is identical — only the transport layer has been swapped out.

The time it takes to complete the measurement is 1 s (the averaging time). The `DATA?` query does not wait for the measurement to complete; the delay of 2 seconds gives the instrument time to complete the measurement before the data is read.

```c
/*
 * MEASURE U, I, P (wait synchronously)
 *
 * This example will configure the instrument to measure power, voltage
 * and current on three-phase system 3 x 400V/50Hz and reads the measurements.
 *
 * The time it will take to finish the measurement is 1 s (averaging time).
 * The query DATA? does not wait for the measurement to be completed, so it
 * would return whatever values are available at a moment. The delay of 2
 * seconds gives the instrument enough time to finish the measurement before
 * reading data.
 */

#if WIN32
#define WIN 1
#endif

#if WIN
#if !defined(_MFC_VER)      /* !MFC (!<afx.h>) */
#include <windows.h>
#endif  /* !_MFC_VER */
#endif
#include <stdio.h>

/*
 * "\\.\com<num>" in order to support "COM10" and above.
 *
 * More info: MSKB article Q115831:
 *
 *      http://support.microsoft.com/default.aspx?scid=kb;EN-US;q115831
 */
#define SIO_PORT            "\\\\.\\com1"
#define SIO_BAUDRATE        115200

#define SIO_INPUT_BUFSIZE   4096
#define SIO_OUTPUT_BUFSIZE  4096

#define MAX(a,b)    ( (a) > (b) ? (a) : (b) )

#if WIN
/* serial port handle ('CreateFile("COMx:",...)'): */
typedef HANDLE          sio_handle_t;
#endif

typedef unsigned char byte;

typedef struct {
    sio_handle_t handle;

    /*
     * I/O buffer (similar to 'FILE'):
     */
    struct {
        byte *base; /* address of allocated buffer */
        byte *ptr;  /* pointer to first available byte in buffer 'base' */
        int size;   /* size in bytes of allocated buffer 'base' */
        int cnt;    /* current number of bytes available in buffer at 'ptr' */
    } buf;

} sio_t;

/******************************************************************************/
static void Delay(double seconds)
{
    Sleep((DWORD)(seconds * 1000));
}

/******************************************************************************/
static void sio_error(char *func)
{
    fprintf(stderr,"%s() failed, error %ld\n",func,GetLastError());
}

/******************************************************************************/
static int sio_open(sio_t *sio,char *device)
{
    COMMTIMEOUTS timeouts;
    sio_handle_t handle;
    DCB dcb;

    memset(sio,0,sizeof(*sio));

    handle = CreateFile(
        device,                         // LPCTSTR lpFileName
        GENERIC_READ | GENERIC_WRITE,   // DWORD dwDesiredAccess
        0,                              // DWORD dwShareMode
        NULL,                           // LPSECURITY_ATTRIBUTES lpSecurityAttributes
        OPEN_EXISTING,                  // DWORD dwCreationDisposition
        0,                              // DWORD dwFlagsAndAttributes
        NULL                            // HANDLE hTemplateFile
    );
    if ( handle == INVALID_HANDLE_VALUE )
    {
        sio_error("CreateFile");
        return -1;
    }
    sio->handle = handle;

    dcb.DCBlength = sizeof(dcb);
    if ( !GetCommState(handle,&dcb) )
    {
        sio_error("GetCommState");
        return -1;
    }

    /*
     * Baud Rate:
     */
    dcb.BaudRate = SIO_BAUDRATE;

    /*
     * Character Size:
     */
    dcb.ByteSize = 8;

    /*
     * Parity:
     */
    dcb.Parity  = NOPARITY;
    dcb.fParity = TRUE;

    /*
     * Stop Bits:
     */
    dcb.StopBits = ONESTOPBIT;

    /*
     * Hand-shake:
     */
    dcb.fRtsControl        = RTS_CONTROL_ENABLE;
    dcb.fOutxCtsFlow       = FALSE;
    dcb.fDtrControl        = DTR_CONTROL_ENABLE;
    dcb.fOutxDsrFlow       = FALSE;
    dcb.fDsrSensitivity    = FALSE;
    dcb.fOutX              = FALSE;
    dcb.fInX               = FALSE;
    dcb.fTXContinueOnXoff  = FALSE;
    dcb.fAbortOnError      = TRUE;
    if ( !SetCommState(handle,&dcb) )
    {
        sio_error("SetCommState");
        return -1;
    }

    /*
     * Read timeout: MAXDWORD, 0, 0
     * - read operation is to return immediately with the characters
     *   that have already been received, even if no characters have
     *   been received (i.e. read operation does not block)
     */
    timeouts.ReadIntervalTimeout = MAXDWORD;
    timeouts.ReadTotalTimeoutMultiplier = 0;
    timeouts.ReadTotalTimeoutConstant = 0;
    /*
     * Write timeout: 0, n
     * - write operation will not block
     */
    timeouts.WriteTotalTimeoutMultiplier = 0;
    timeouts.WriteTotalTimeoutConstant = 0;
    if ( !SetCommTimeouts(handle,&timeouts) )
    {
        sio_error("SetCommTimeouts");
        return -1;
    }

    /*
     * Set up I/O buffers:
     */
    if ( !SetupComm(handle,SIO_INPUT_BUFSIZE,SIO_OUTPUT_BUFSIZE) )
    {
        sio_error("SetupComm");
        return -1;
    }

    sio->buf.size = MAX(SIO_INPUT_BUFSIZE,SIO_OUTPUT_BUFSIZE);
    sio->buf.base = malloc(sio->buf.size);
    if ( !sio->buf.base)
        return -1;
    sio->buf.ptr = sio->buf.base;
    sio->buf.cnt = 0;

    return 0;   /* OK */
}

/******************************************************************************/
static int sio_close(sio_t *sio)
{
    CloseHandle(sio->handle);
    if ( sio->buf.base )
        free(sio->buf.base);
    sio->buf.base = NULL;

    return 0;   /* OK */
}

/******************************************************************************/
static int sio_read(sio_t *sio,byte *buf,int bufsize)
{
    DWORD l;

    if ( !ReadFile(sio->handle,buf,1,&l,NULL) )
    {
        sio_error("ReadFile");
        return -1;
    }

    return l;
}

/******************************************************************************/
static int sio_write(sio_t *sio,byte *buf,int bufsize)
{
    DWORD l, len;

    for(len = 0; len < (DWORD)bufsize; len += l)
    {
        if ( !WriteFile(sio->handle,buf+len,bufsize-len,&l,NULL) )
        {
            sio_error("WriteFile");
            return -1;
        }
    }
    return len;
}

/******************************************************************************/
static int sio_fillbuf(sio_t *sio)
{
    register int l;

    l = sio_read(sio,sio->buf.base,sio->buf.size);
    if ( l <= 0 )
        return l;

#if 0
    fprintf(stderr,"sio_fillbuf(): sio_read(): %d\n",l);
#endif

    sio->buf.cnt = l;
    sio->buf.ptr = sio->buf.base;

    return l;
}

/******************************************************************************/
static int sio_getc(sio_t *sio,int *pchar)
{
    int ret;

    if ( !sio->buf.cnt && (ret = sio_fillbuf(sio)) <= 0 )
        return ret;

    sio->buf.cnt--;
    *pchar = *sio->buf.ptr++;

    return 1;   /* OK */
}

/******************************************************************************/
static int sio_gets(sio_t *sio,char *str)
{
    register int len;
    int c, ret;

    for(len = 0; ; )
    {
        if ( (ret = sio_getc(sio,&c)) < 0 )
            return ret;
        if ( ret == 0 )
            Sleep(10);  /* no data on input, don't hog CPU */
        else
        {
            if ( c == '\r' )
                continue;
            str[len++] = (char)c;
            if ( c == '\n' )
                break;
        }
    }
    str[len] = '\0';    /* terminate string */

    return 0;   /* OK */
}

/******************************************************************************/
static int sio_puts(sio_t *sio,char *str)
{
    char buf[1024];

    strcpy(buf,str);
    strcat(buf,"\n");

    return sio_write(sio,(byte *)buf,strlen(buf));
}

/******************************************************************************/
int main(int argc,char *argv[])
{
    char buffer[1024];
    sio_t sio;

    /*
     * Open connection to the instrument:
     */
    if ( sio_open(&sio,SIO_PORT) < 0 )
        return 1;

#if 0
    if ( sio_puts(&sio,"*IDN?") < 0 )
        return 1;
    if ( sio_gets(&sio,buffer) < 0 )
        return 1;
    puts(buffer);
#endif

#if 1
    /* Bring the instrument into a default state: */
    sio_puts(&sio,"*RST");
#endif
    /* Select three wattmeter configuration: */
    sio_puts(&sio,"ROUT:SYST \"3W\"");
    /* SYNC source = voltage phase 1: */
    sio_puts(&sio,"SYNC:SOUR VOLT1");
    /* Set voltage range on voltage channel 1 to 300 V: */
    sio_puts(&sio,"VOLT1:RANG 300.0");
    /* Set current channel 1 to autorange: */
    sio_puts(&sio,"CURR1:RANG:AUTO ON");
    /* Set averaging time to 1 second: */
    sio_puts(&sio,"APER 1.0");
    /* Select U, I, P measurement: */
    sio_puts(&sio,"FUNC \"VOLT1\",\"CURR1\",\"POW1:ACT\"");
    /* Run continuous measurements: */
    sio_puts(&sio,"INIT:CONT ON");

    Delay(2.0);        /* Wait 2 seconds */

    /* --- Reconstructed: the source excerpt ends on page 6-26 --- */
    sio_puts(&sio,"DATA?");             /* Query the measurement */
    memset(buffer,0,sizeof(buffer));    /* Clear buffer */
    sio_gets(&sio,buffer);              /* Read values */
    puts(buffer);                       /* Print the value on the screen */

    sio_close(&sio);

    return 0;
}
```

> **Note:** The source excerpt from the manual ends after `Delay(2.0);` (page 6-26), in the middle of the end of the RS-232 example. The lines after the “Reconstructed” comment above are not from the manual, but have been reconstructed following the same pattern as the Ethernet example: query with `sio_puts(&sio,"DATA?")`, read the response with `sio_gets`, print the value and close the port with `sio_close`.

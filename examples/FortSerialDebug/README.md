# FortSerialDebug 1.2.0 — field diagnostic guide

**English** · [Bahasa Indonesia](README.id.md) · [Main firmware](../../README.md)

Open [FortSerialDebug.ino](FortSerialDebug.ino). It needs only the Espressif Arduino core, with no external libraries. This is a separate application: the dashboard/interval tracker does not run while this sketch is installed. Reader settings are RAM-only and the meter receives function 03 only.

## Start without changing the installation

1. Keep module TXD → ESP32 GPIO18 and module RXD ← GPIO17, as confirmed for this installation. Module VCC → ESP32 3V3, TTL GND → GND, A+ → Fort58/A, B− → Fort59/B. Automatic direction: GPIO16 unconnected. Fort70/COM is not RS485 ground.
2. Open the INO in Arduino IDE and keep [partitions.csv](partitions.csv) beside it. Choose ESP32S3 Dev Module / 16 MB flash / OPI PSRAM. For native USB: Hardware CDC and JTAG, USB CDC On Boot Enabled. Erase All Flash Disabled.
3. Upload and open Serial Monitor at **115200 + Newline**. Locally compiled with Arduino ESP32 2.0.17 through PlatformIO; the IDE GUI and other core versions are not acceptance-tested.
4. Send the following block. It uses the current installation defaults; change ID/baud/parity from the meter menu when known. All parameters below are runtime commands, not source edits.

```text
x
pins 18 17
id 1
baud 9600
parity N
timeout 800
stats clear
diagnose
```

`diagnose` listens without TX for three seconds, probes communication, then runs the configured raw-register plan when a valid response/exception exists. A failed map does not cancel the evidence of a valid Modbus reply. The passive stage checks the selected UART format only; silence is normal for an unpolled slave.

## Choose the test by the observation

| Observation | Use | What to decide next |
| --- | --- | --- |
| No RX bytes | [01 passive RX](cases/01-passive-rx.txt), then [02 single request](cases/02-echo-or-no-reply.txt) | Separate silence without TX from silence after a request. Neither alone proves a wiring fault. |
| Exact request returns to RX | [02 echo/no reply](cases/02-echo-or-no-reply.txt) | TX=RX is echo evidence, not a meter response. Compare context and UART errors; do not assume its physical source. |
| CRC/parity/framing errors | [03 serial matrix](cases/03-serial-matrix.txt) | Compare the same ID across configured baud/parity modes; inspect driver counters and raw bytes. A wrong register by itself does not validate corrupted data. |
| CRC-valid exception | [04 exception/register](cases/04-register-exception.txt) | Communication replied; inspect exception code and the queried address/count independently. |
| Communication works, energy profile does not | [05 raw map audit](cases/05-raw-map-audit.txt) | Disable conversion, compare raw words/register support; no fabricated kWh. |
| Raw energy and ratios look plausible | [06 configured kWh](cases/06-configured-kwh.txt) | Decode with explicitly logged addresses/scale/order and compare the corresponding meter energy page. |
| Response may be late | [07 timeout comparison](cases/07-timeout-comparison.txt) | Hold ID/baud/parity/register constant and change timeout. A longer timeout does not bypass CRC. |
| Word order is uncertain | [08 word-order comparison](cases/08-word-order-comparison.txt) | Compare labeled normal/swapped interpretations. The sketch does not automatically choose whichever number looks plausible. |

Each linked TXT is a command recipe. Paste one recipe at a time; its `#` lines are comments. The console queues up to **16 complete commands**, including while a UART wait/listen is active; each line is at most **95 characters**. Queue/line overflow is reported explicitly. **x** immediately aborts and cancels queued commands. Commands following an ongoing scan are deferred until scan finishes. Upload once, select different tests through Serial.

## Runtime reference

| Command | Meaning and accepted values |
| --- | --- |
| `help`, `status` | Help or full settings, scan lists, audit plan and UART counters |
| `basic`, `listen 10` | Passive RX for 5 seconds or specified 1–30 seconds; no Modbus request |
| `intermediate`, `diagnose`, `deeper` | Probe only; staged diagnosis; raw plan plus optional configured energy decode |
| `id 7` | Reader target 1–247; broadcast ID0 is never used |
| `baud 19200`, `parity E` | Reader baud 1200–1000000; parity N/E/O, 8 data bits, 1 stop bit. Fort manual lists 1200–38400; wider diagnostic rates are experiments, not claims of device support. |
| `pins 18 17` | RX GPIO then TX GPIO, distinct. Guarded choices: 10–18,21,39–42,47. USB, flash/PSRAM, boot/console and the known I2C/button/buzzer/RGB pins are excluded. Default wiring remains18/17. |
| `timeout 1200` | Response budget 50–5000 ms, starts after TX flush |
| `interval 3000` | Repeated-read target 100–60000 ms; actual reads/printing can take longer |
| `probe 0080 1` | Set scan/intermediate probe: HEX PDU address, 1–3 words |
| `reg 0082 2` | One bounded raw read; HEX address (optional0x), 1–3 words; no40001 translation |
| `range 1 16` | Inclusive scan ID range |
| `scanmode current` | Scan selected IDs with current baud/parity only |
| `scanmode matrix` | Scan selected IDs across configured baud/parity lists |
| `bauds 9600 19200` | Replace list with 1–6 unique decimal rates; validated atomically |
| `parities NE` | Replace list with 1–3 unique N/E/O modes |
| `scan` | Probe configured combinations. Stop on matching valid data or exception, separately from energy profile. No settings are written to the meter. |
| `plan clear`, `plan add 0080 1`, `plan show` | Configure up to8 raw read queries for deeper; HEX addresses/count1–3 |
| `energy 0082`, `ratios 002C` | Set 2-word UInt32 energy and 3-word PT numerator/PT denominator/CT locations |
| `scale 0.001`, `order normal`, `order swap` | Positive finite scale1e-12–1e6 and UInt32 high/low word ordering; interpretation is printed explicitly |
| `decode off`, `decode on` | Disable/enable kWh interpretation, including read/poll; raw reg/plan remains available |
| `read`, `poll` | One configured kWh/CTPT reading or toggle repeated reads |
| `stats clear`, `x` | Clear local UART error counters; abort/cancel queued work |

Successful configuration changes print settings. Invalid values leave the old setting intact. Changes are not persisted; restart restores defaults. Changing UART/profile settings stops polling, so a partially applied multi-line configuration does not generate automatic reads. Function03, frame integrity and storage protections are fixed safety boundaries, not arbitrary runtime overrides.

## Scan size and elapsed time

Default matrix: IDs1–247 × six documented baud rates × N/E/O = **4,446 candidates**. For an initial test use one ID and the configured format:

```text
x
range 1 1
scanmode current
probe 0034 1
scan
```

For a limited matrix, use case03 or set smaller lists. Current mode has `ID count` candidates; matrix has `ID count × baud count × parity count`. No-reply time is roughly candidates × timeout plus transmission/silent-gap/printing costs. At the default800ms, a full silent scan is about an hour or longer. `status` prints the actual candidate count; `x` cancels immediately during waits.

## Read the result, then choose an action

| Log | Meaning | Next action |
| --- | --- | --- |
| `COMM_FOUND` / `VALID_RESPONSE` | Matching ID/function/count/length/CRC response | Continue the raw map, then compare configured decoding with the meter |
| `COMM_VALID_EXCEPTION` | Matching ID, FC83 and valid CRC | Code01: check documented function/model;02: address/count/map;03: quantity/value constraints;04: device-side failure. The sketch does not clear/reset the meter. |
| `EXACT_TX_ECHO` | Complete RX frame equals the8-byte request | Remains rejected; receiver waits for a separate valid response within the same budget |
| `MISMATCH`, `SHORT_FRAME`, `WRONG_FRAME` | Corrupt/incomplete or incompatible response shape | Preserve hex and UART counters; compare formats/timing before inferring an address-map fault |
| `TIMEOUT` | No RX bytes during the request budget | Compare passive/single/matrix tests; no automatic wiring verdict |
| `NO_VALID_RESPONSE` | Some RX, no accepted data/exception | Distinguish echo, corruption and shape mismatch using accompanying logs |
| `RX_OVERFLOW` | Only the first64 bytes of an oversized frame retained | Result rejected; inspect traffic volume and driver FIFO/buffer counters |
| `DECODE_DISABLED` / `PROFILE_NOT_CONFIRMED` | Conversion disabled or energy/CTPT rejected | Use raw plan; a prior valid protocol reply is still valid evidence |
| `UART_EVENTS` | Cumulative parity/framing/FIFO/buffer/break events and RX logic state | `stats clear` starts a comparison. Zero counters do not prove clean differential A/B signaling. |

The default map follows the visually checked manual pages: energy0x0082–83, PT/CT0x002C–2E, scale0.001, high word/byte first. The manual visibly prints energy length3 despite two addresses and ulong type; retain that ambiguity. Configuring another address/scale/order is an explicitly labeled experiment, not independent verification of that map. Primary float words are raw-audited, not guessed into a kWh value. [Manual audit](../../docs/DESIGN.md#manual-visual-audit-and-diagnostic-follow-up--26-october-2026).

## Boundaries and verification

No register writes, broadcast, meter resets, NVS writes or filesystem operations. No DE pin used for the supplied automatic module. RX64 bytes/frame; detailed logging capped at4 frames/request, passive logging8 chunks; remaining traffic is still counted/validated. Callback error counts are atomic and console output stays in loop context. UART buffer timing is not a logic-analyzer measurement; no RX does not prove no signal on A/B. Serial output itself affects timing.

Tests execute the actual INO with deterministic host UART/console mocks, including runtime settings, invalid values, multiple result classes, queuing, stop and conversion. PlatformIO compiles this sketch independently (`pio run -d examples/FortSerialDebug`). Hardware timing/mapping and the Arduino IDE GUI remain untested. Restore the main firmware after diagnostics; keep partition CSV and flash-erase selection consistent with its guide.

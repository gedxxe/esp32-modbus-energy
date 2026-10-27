# Firmware design

**English** · [Bahasa Indonesia](id/DESIGN.md)

Version 2.0.0 is organized around a single FC03 transaction path in the local FortMeter library. Main polling, connection discovery and the standalone sketch call the same FortTransport implementation, which delegates wire transactions to ModbusMaster 2.0.1+fort.1. This replaces the v1 software-gap collector rather than retaining it as a second interpretation of the same bytes.

## Fort register reference

The supplied scanned [Fort manual](../Fort%20Power%20Meter_User%20Manual_compressed.pdf) is the register reference. PDF page 26, printed page 24, contains the request `01 03 00 58 00 02 45 D8` for phase-A current. Its example data `0000 C317` means 4.9943 A. The response CRC printed in that table does not match a recomputation, so the implementation calculates CRC and does not copy the printed CRC into acceptance logic.

| PDU address | Words | Interpretation | Manual PDF page |
| --- | ---: | --- | ---: |
| 0x002C–0x002E | 3 | PT numerator, PT denominator, CT, uint16, range 1–9999 | 27 |
| 0x0042–0x0043 | 2 | Phase-A voltage, uint32 / 10000 V | 28 |
| 0x0058–0x0059 | 2 | Phase-A current, uint32 / 10000 A | 26, 29 |
| 0x006A–0x006B | 2 | Total active power, int32 / 10000 kW | 29 |
| 0x0080 | 1 | Frequency, uint16 / 100 Hz | 30 |
| 0x0082–0x0083 | 2 | Secondary import energy, uint32 / 1000 kWh | 30 |
| 0x008A–0x008B | 2 | Primary import energy, float32 word-order candidates | 30 |

The energy table visibly prints length 3 beside an unsigned-long value occupying two addresses. V2 reads two 16-bit registers, consistent with those addresses and the user's successful reference, while the raw diagnostic plan can explicitly compare one/two/three-word reads. It never treats an extra word as energy by default. UINT32 words are combined high first. Float candidates are shown in ABCD and CDAB order because a finite-looking number does not verify the meter's float order.

Direct current, voltage, frequency and power values follow the conversions in the reference sketch. Only the production energy calculation multiplies the secondary counter by the validated PT/CT ratio. The primary float is not used as a silent fallback when that ratio fails.

## UART and transceiver state

![Manual direction sequence](assets/rtu-timing.svg)

The confirmed module uses TX17→DI, RX18←RO and GPIO16→DE+/RE. FortTransport configures the line in receive mode before beginning UART1. ModbusMaster drains the receive stream, calls preTransmission to assert direction and wait 100 µs, writes its eight-byte request, flushes the UART transmitter, and calls postTransmission to deassert direction and wait 100 µs. Reception then completes when the expected response length has been collected, or returns a status at the configured timeout. A complete reply does not need a task-observed t3.5 delimiter.

The default inter-request spacing is 200 ms and the response timeout is 2000 ms, matching the successful sketch. The waiting path cooperates with the scheduler, services the watchdog and checks stop/resume. The adapter bounds pre-TX draining and capture storage so continuous input cannot turn a buffer operation into an unbounded loop. Automatic direction is an explicit connection setting, with no DE-/RE GPIO transitions in that mode.

The upstream transaction loop remains recognizable in `firmware/lib/FortMeter/src/ModbusMaster.cpp`. The documented patch adds runtime timeout, cancellation and initialization fixes. It does not replace the request builder with a custom scanner parser. `provenance.json` records upstream and current hashes, and `fort-hooks.patch` records changes. The wrapper separately verifies ID, function, requested byte count, exact captured length and CRC, including exception frames because upstream 2.0.1 returns those before its normal CRC check. Capture limits prevent oversized input from reaching the upstream ADU buffer without a bound.

Application code exposes only FC03 reads. A complete request echo is rejected and is never stripped to manufacture a response. Extra buffered bytes also fail strict acceptance. Raw traffic and ModbusMaster status remain visible even when a value is rejected. UART and I2C are hardware peripherals, so the GPIO direction callbacks and CRC loop do not bit-bang their serial waveforms.

## Task and lock model

![Core ownership](assets/architecture.svg)

Core 1's acquisition task owns UART1, ModbusMaster, direction GPIO, Preferences and tracker mutation. Core 0 owns the synchronous WebServer, button/OLED UI and RGB RMT channel 0. Core 0 submits value-copy commands rather than entering the UART or NVS driver. Core 1 publishes snapshots through a single-element overwrite queue. The diagnostic log and view also cross bounded queues, with the log retaining the newest 8 KiB and a discard count.

Ordinary controls carry a command ID, admission time and mode revision. Core 1 validates disruptive controls again at execution, rejecting requests older than ten seconds or belonging to another mode revision. LED/mute/raw-capture controls execute at transaction boundaries, so their NVS writes do not intentionally interrupt UART collection. At most six ordinary controls and sixteen inactive diagnostic inputs are handled per outer pass, leaving time for acquisition and idle/system tasks.

Stop/resume uses a separate mailbox and a cancellation generation. Only core 1 removes diagnostic inputs, which avoids the former cross-core reset/peek race. Input admitted after Resume belongs to a later generation and can enter a new session. The transport checks cancellation inside its receive loop as well as during delays, including when input remains continuously available.

I2C is protected by a mutex, with RTC operations bounded by driver timeouts and OLED refresh allowed to skip a busy lock. Filesystem writes use a separate bounded mutex, and no normal path holds both locks while waiting for network output. HTTP response writes are nonblocking in 1 KiB chunks with a five-second budget and watchdog/LED service. The upstream request parser remains a separate on-device load-testing boundary.

## Acquisition and scheduling

A failed contact attempt reads only the manual handshake register and backs off for three seconds, avoiding a burst through the entire profile when the link is unavailable. Once contact succeeds, normal polling targets a two-second period, ratios refresh on their existing retry/backoff schedule, and electrical comparison values refresh approximately every ten seconds. Each transaction has its own bound, so target periods can lengthen under timeouts. These are cooperative periods, not hard real-time deadlines.

The secondary energy read is independent of ratio validation. Its raw kWh remains visible when CT/PT is unavailable, but the primary counter and Tracker receive no fabricated or unscaled substitute. The energy timestamp is bracketed by RTC reads, and slow or inconsistent brackets interrupt continuity instead of producing a complete interval.

## RTC, counter conversion and continuity

The DS3231 stores UTC. WIB is applied only for display, day selection and the nine scheduled windows. The production counter is `uint32_secondary × 0.001 × PT_num / PT_den × CT`, evaluated in double precision with validated nonzero ratios. Changes to ratios, raw-counter decreases, excessive increases, invalid RTC, reboot and sample gaps beyond 15 seconds all break continuity. Completed intervals survive that interruption. Interpolation is limited to accepted neighboring samples inside the gap limit.

`clock_guard.h` validates calendar bounds and protects the retained day even when RTC health is false. A queued phone timestamp is compensated for whole seconds spent waiting, bounded to ten seconds, then checked again before writing. Automatic rollover also refuses a day earlier than the retained records. The OLED rotation flag applies before any boot/recovery drawing and does not affect timekeeping.

## Scan controller

Discovery enumerates the saved ID/format first, then the selected current-format or 18-format range without duplicate candidates. Its default probe is 0x0058/two words. A valid data response or CRC-checked exception is repeated once. Only a second valid contact promotes a candidate. The scan result remains distinct from profile compatibility, because an exception can prove protocol contact without proving the requested register is readable.

Direction is retained from the selected connection throughout the search. Settings are not reconfigured on the meter and no broadcast/write functions are called. At the default timeout, a fully silent 4,446-candidate matrix takes about three hours. Failed probes are not misreported as proof of a defective module. Cancellation is serviced during a request and the saved production connection is restored afterward.

## Persistence and export

The v2 connection record uses a new magic while retaining its 12-byte layout, assigning former padding bytes to direction and a reserved byte. CRC-valid v1 connection records are migrated to the confirmed manual installation while keeping slave ID, baud and parity. Active-day/LED records are unchanged. A failed migration write is surfaced as storage failure rather than reported as saved.

A day checkpoint has a magic and CRC. CSV replacement writes a temporary file and uses a backup-preserving install sequence. Failure keeps recoverable copies. LittleFS never formats automatically, and its physical boot confirmation is available only after mount failure. CSV downloads copy a small bounded file under lock, then release filesystem ownership before network transfer. Retention is limited to the configured 31-day archive window.

## HTTP and browser delivery

The API publishes transport identity, direction, last Modbus status, primary/secondary energy and direct comparison registers. HTTP 202 only acknowledges queue admission. The browser waits for the exact result in the last-eight completion ring before treating ordinary controls as executed, while Stop/Resume remains available through its priority path. Input and payload schemas prevent malformed rows from crashing the device, archive, trace or process views.

The communication view replays observed event spacing at 1× with polling/network delay. It represents queued TX, completed UART transmission, received bytes and transaction outcome, not measured propagation or A/B voltage. Process percentages sample non-idle task state at tick boundaries over roughly one second. ISR work is attributed to the interrupted task and stack high-water values are historical minima. Neither animation nor a process table substitutes for on-target acceptance.

## Standalone Arduino diagnostic

The standalone INO includes FortDiagnostics from the same FortMeter library as production. There is no generated copy of the old parser. The preserved `diagnostic_engine.h` and `build-diagnostic.py` paths are compatibility/provenance entry points. Arduino IDE installation uses the supplied FortMeter library ZIP so its patched hooks and notices match the firmware source. Every test recipe can also be entered one command at a time in the web console, and test settings stay in RAM.

## References

The root scanned Fort manual supplies the register map, the user's reported working sketch supplies the confirmed manual-direction baseline, and [ModbusMaster v2.0.1](https://github.com/4-20ma/ModbusMaster/tree/v2.0.1) supplies the transaction implementation. Local software results and the remaining integrated hardware checks are recorded in [VALIDATION](VALIDATION.md), with field steps in [DIAGNOSTICS](DIAGNOSTICS.md).

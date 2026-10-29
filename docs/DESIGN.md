# Firmware design

**English** · [Bahasa Indonesia](id/DESIGN.md)

The shared discovery controller, named flash registry, immutable checkpoint source IDs and refactored settings codec are described in [Meter discovery](METER_DISCOVERY.md). The scanner remains FC03-only and does not infer a manufacturer from raw values.

Meter-specific Fort examples below remain valid for Fort. For Acrel, select the saved model first and use [Meter profiles](METER_PROFILES.md) for its register map, terminals, conversion and diagnostic workflow.

Version 2.6.0 is organized around a single FC03 transaction path in the local FortMeter library. Main polling, connection discovery and the standalone sketch call the same FortTransport implementation, which delegates wire transactions to ModbusMaster 2.0.1+fort.1. This replaces the v1 software-gap collector rather than retaining it as a second interpretation of the same bytes.

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

## Wi-Fi state and persistence

The Wi-Fi driver runs on core 0, while core 1 validates and stores the 108-byte v2 credential record under the `wifi` Preferences key. The record explicitly encodes mode, 33 SSID bytes, 65 key bytes, a four-byte AP IPv4 preference and CRC16, avoiding structure-padding dependencies. CRC detects a damaged record and does not encrypt credentials. WiFi.persistent(false) prevents the driver from maintaining a second persistent configuration outside the application's core 1 ownership.

An accepted network command must pass normal-mode/revision/age guards and save successfully before a one-entry queue hands it to core 0. The eight-second deferred application preserves time for its HTTP execution result. A failed save keeps the current driver configuration, and a corrupt boot record leaves the default AP available. Switching to AP retains client credentials without exposing the saved password through GET endpoints.

NetworkPolicy in network.h makes rollover-safe decisions without driver calls. Core 0 samples the link on a 250 ms target, which bounded HTTP work can defer, and retains an AP during connection attempts and retry backoff, and allows a 15-second grace after client DHCP before disabling AP in client-only mode. Losing the client link restores AP, while explicit connection attempts repeat every 30 seconds. AP + client keeps both interfaces on the shared radio, and overlapping LAN/AP subnets are checked with the less-specific prefix before choosing another AP address. WLAN transition timing, DHCP and router isolation still require on-device acceptance.

The diagnostic HTTP view now carries actual session settings and evidence counts, with production connection/status substituted when maintenance is inactive. Structured browser controls prepare the same commands used by the standalone console. Capture comparisons require matching direction, direction pin and settling/gap timing as well as their existing register/profile settings, preventing a changed transport setup from being interpreted as only an A/B change. The full route-to-owner map and operator steps are in [WEB](WEB.md).

## Network UI and clock changes in 2.2

Network settings now use an explicit 108-byte v2 record, with a four-byte preferred AP IPv4 address before its CRC. The old 104-byte v1 record remains readable and supplies 192.168.4.1. Serialization never depends on struct padding. The AP keeps a /24 network, prefers the configured address and still avoids an overlapping client subnet.

SSID scan is asynchronous through the core 0 driver and shares a bounded 16-row cache between web and OLED. Saved credentials remain with core 1, and both input surfaces create the same SET_NETWORK command. The pure OledNetworkMenu state machine handles mode, network selection, masked ASCII entry and IP tools without touching GPIO or NVS, allowing host transition tests. Physical button repeat starts after 450 ms and steps at 80 ms, with a suppressed release event to avoid an extra character.

The status API publishes device UTC, its monotonic sample age and a clock revision. Browser presentation extrapolates seconds without RTC writes, retaining its anchor for five minutes unless the device/session/time validity changes. OLED presentation uses the same sampled-age concept, while the acquisition tracker continues to receive only validated RTC reads. UART event pages are capped at eight entries, with a more flag preserving catch-up and sequence-gap reporting.

## Idle rendering ownership

Display preferences have an independent eight-byte CRC record and pass through core 1 like LED settings. Core 0 observes physical input and consumes the entire wake press through a release guard before the existing debouncer events can reach the monitoring/menu controls. A pure IdleScreen policy covers timeout, disable, wake and rollover without peripheral calls.

The artwork is stored as per-frame RLE packets with checked offsets. build-oled.py decodes all 772 frames and verifies their combined raw SHA256 before checking/generated C++ output, so the scattered reference headers are unnecessary at deployment. Playback decodes one 1 KiB working bitmap, uses the existing display rotation and bounded I2C mutex, and advances by elapsed time at a nominal 42 ms period. HTTP work and I2C transfer latency may drop frames, while acquisition remains independent and never receives animation-derived samples.

## Radio discovery and HTTP scheduling

Wi-Fi discovery and client association share the radio on core 0. The scanner therefore waits 300 ms after preparation, suspends an unfinished association, retries driver rejection/failure at 750 ms up to three attempts and bounds each accepted scan to 12 seconds within a 20-second operation. Its zero-initialized IDF configuration explicitly sets active dwell to 100–200 ms and home-channel dwell to 50 ms. It waits for the Arduino event task to finish collecting results, then receives completion status and timestamp through a one-element RTOS mailbox. Core 0 copies up to 16 strongest visible networks and releases Arduino's list. A deadline is judged against completion time rather than a later HTTP-loop observation, and the prior list survives a failed scan.

The managed adapter targets the pinned Arduino ESP32 2.0.17 scan internals. Its purpose is to avoid the wrapper's uninitialized configuration fields and dwell × 20 completion timeout while retaining a single owner of Arduino's result storage. It must be reviewed when upgrading that framework. Physical scan success, AP continuity and driver resource failures still require radio acceptance.

The browser scheduler serializes all API fetches through full response-body reception. Each active request has an eight-second deadline, with queue wait excluded. POSTs take precedence over queued telemetry, and explicit diagnostic Stop/Resume may abort an active GET while leaving an accepted POST intact. On the firmware side, an empty connection waiting longer than 250 ms yields when another socket is available. The upstream parser and five-second bounded writer retain their existing limits. This is request scheduling for the single-client WebServer, not a change to core 1 acquisition.

## Browser animation ownership

FrameAssembler consumes validated event metadata rather than parsing Modbus bytes. It rejects discontinuous sequences, mismatched link/pin context and outcomes without an observed TX-complete event. FlowPlayback owns a constant-size visual queue with one active transaction and the newest pending transaction. Its RAF accumulator caps a single browser frame step at 48 ms, preventing a delayed render from teleporting across the track. Rendering uses the supplied WakeSlider as an inert visual and keeps its smooth presentation timing separate from the device microsecond timestamps. No animation state is sent to acquisition or persistence.

DeviceClock retains its existing UTC/sample-age anchor and memoizes the supplied Counter per clock unit, so unchanged units do not rebuild their numeral strips on each status sample. Native timer accessibility exposes the complete HH:mm:ss value. NavTabs measures button offsets through ResizeObserver and moves one shared selection surface, while the supplied StarBorder provides its border effect. Reduced motion disables replay and decorative canvas, and hidden/failed/restarted event streams discard visual state.


## Primary checkpoint pipeline

The ten-point raw recorder is separate from the retained legacy Tracker. Its pure admission, continuity and explicit serialization live in [checkpoint_log.h](../firmware/include/checkpoint_log.h), while main.cpp owns the RTC bracket, urgent UART scheduling, NVS/LittleFS commits and queue projections. The web never creates a capture. [CHECKPOINTS](CHECKPOINTS.md) documents the 426-byte format, first-success window, rollover protection and derived differences.

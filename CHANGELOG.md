# Changelog

**English** · [Bahasa Indonesia](CHANGELOG.id.md)




## 2.0.0 / 2026-10-27

The measurement path now follows the user's successful ModbusMaster sketch, with manual DE-/RE on GPIO16, TX17/RX18, 100 µs direction settling and 0x0058/two words as the first contact request. Production polling, scanner probes and the standalone sketch consume one source-shared FortMeter library, replacing the former software-gap collector. The preserved generator/header paths provide compatibility rather than a second parser.

Connection records migrate from CRC-valid v1 settings while retaining ID/baud/parity and selecting the confirmed manual installation. Discovery repeats a valid candidate before offering it, raw secondary kWh stays available without CT/PT, and direct current/voltage/frequency values are exposed for comparison. Production interval quality and completed records remain protected, while primary float word order stays an explicit diagnostic comparison.

The release adds a self-contained Arduino library ZIP, pinned ModbusMaster provenance and actual-library regressions, and rewrites the English/Indonesian operating, wiring and architecture guides around the new path. Direction is selectable in the dashboard, the live register panel names its addresses, and the web defaults to the manual circuit. The integrated release still requires field acceptance even though the reference sketch was reported working.

## 1.4.1 / 2026-10-27

- Regenerate and commit browser inputs/header/manifest together. Make PlatformIO's stale-asset error actionable without a Python traceback. A normal firmware build still needs no Node installation.
- Require a continuous observed quiet interval before TX. Discard late pre-TX bytes, withhold requests on sustained traffic and expose a separate bus-busy count. Improve observation of minimum-turnaround echo/reply separation. Retry failed UART initialization.
- Revalidate queued commands against mode revision/age, publish execution outcomes and keep priority diagnostic stop/resume independent of ordinary UI actions. Replace cross-core queue reset/peek with generation-stamped cancellation and atomic dequeue.
- Protect retained days even with an invalid RTC. Recheck/age-compensate queued time sync and reject backward automatic rollover. Bound queue work per acquisition pass.
- Validate browser payload rows, retain the last usable snapshot on malformed responses, block clock sync in maintenance and gate diagnostic commands on initial state. Bound API socket writes and service the watchdog between chunks.
- Add deterministic late-RX, busy-bus, legal-turnaround, UART-retry, command-mode, clock and stalled-writer regressions. Main firmware and standalone diagnostic 1.3.1 share the corrected transport. Physical Fort acceptance remains a field test.

## 1.4.0 / 2026-10-27

- Put all standalone diagnostic commands in the main firmware through one generated/tested engine shared by production polling and active tests. Keep trial settings/data in RAM and restore saved production pins/serial settings on resume.
- Accept matching valid data or exceptions during discovery, independently of energy compatibility. Add current-format search and editable probe/count. Keep full matrix explicit.
- Reject exact echo while waiting for a separate reply. Expose request prefixes with trailing bytes, pre-TX activity, malformed/deadline/overflow evidence and corrupt frames preceding a valid response. Tune RX buffering without claiming wire-level timing.
- Add a phone diagnostic console, bounded nonblocking USB log output, downloadable logs and stop/resume mailboxes. Keep accepted commands queued under internal backpressure.
- Add a timestamped UART event ring and event-driven packet trails with no idle motion. Add core tick sampling, owned/idle task state and heap/PSRAM/stack monitoring.
- Expand real-engine native tests, embedded-gzip browser tests and bilingual operator/technical/acceptance guides. Physical module health and installed Fort register acceptance still require hardware evidence.

## Standalone diagnostic 1.3.0 / 2026-10-26

- Continue the configurable raw register plan after a silent first probe. Report protocol evidence independently of configured kWh decoding.
- Add optional RX weak-bias observation and paired user-labeled A/B captures, with configuration/abort guards and explicit unresolved physical states.
- Rewrite field instructions to name Serial Monitor INPUT, Enter/Send and OUTPUT copied to support. Label all eight recipes. Add an automatic-module wiring diagram and correct assembly guides.
- Reinspect PDF register images. Audit the ambiguous energy entry with raw one/two/three-word reads. Extend actual-INO tests. Main firmware stays 1.3.0.

## Standalone diagnostic 1.2.0 / 2026-10-26

- Add validated Serial configuration for UART pins/timing, probe, ID/format scan lists/mode, raw-register plan and energy address/scale/word order/decode toggle.
- Preserve commands typed during waits in a bounded queue. Emergency stop clears queued work, HEX prefixes do not abort, and invalid input leaves configuration intact.
- Rewrite English/Indonesian field guides around eight pasteable case recipes, result interpretation and next actions. Execute every recipe through the real INO in host tests.
- Keep function03 and integrity/storage constraints fixed. Main firmware remains1.3.0.

## Standalone diagnostic 1.1.0 / 2026-10-26

- Separate passive RX/no-TX, protocol response and energy-profile checks. Add basic/intermediate/deeper/diagnose and bounded raw register commands.
- Detect matching valid exceptions during communication scan without treating exception data as kWh. Retain CRC/ID/function/length validation.
- Add atomic UART parity/framing/FIFO/buffer/break counters and tests executing the real INO with deterministic serial mocks.
- Reinspect the original manual page images and explicitly retain the printed energy-length ambiguity. Main firmware remains 1.3.0.

## 1.3.0 / 2026-10-26

- Add a passive, fixed-RAM capture of the next 12 existing Modbus attempts, with per-attempt UART settings, TX/RX hex, overflow and exact-echo classification. CRC/response validation remains unchanged.
- Add bilingual web capture/start/stop/export controls. Poll full trace data only while its panel is open. Preserve captured frames until a new capture or restart.
- Add a standalone Arduino IDE FortSerialDebug sketch without external libraries: read-only kWh/CT/PT, bounded Serial debugging, abortable ID/baud/parity scan, and an optional matching 16 MB partition layout/PlatformIO build.
- Document the supplied automatic-direction four-pin module separately from the manual DE/RE wiring reference. Add native/browser tests and standalone compilation to CI.

## 1.2.1 / 2026-10-26

- Correct the installation RGB default to GPIO48 after confirmation on the user’s board. Keep GPIO38 documented as a board-revision alternative.
- Rotate every OLED screen 180° by default through one board setting, before the first boot frame.
- Add onboard RGB Off/Solid/Hue/Blink/Breathing with medium brightness defaults and asynchronous RMT transmission on core 0.
- Add English/Indonesian web controls for mode, color, brightness and cycle duration. Persist a versioned CRC-protected LED record on core 1, with storage status exposed separately from energy recording.
- Document GPIO48/GPIO38 board revisions and distinguish RGB from four-channel RGBW. Extend native/browser verification without changing meter polling or interval rules.

## Repository maintenance / 2026-10-26

- Keep one supported N16R8 hardware target and package only its named flashing images.
- Remove obsolete test-only tooling, stale release archives, build caches, temporary files and an unused banner asset.

## 1.2.0 / 2026-10-26

- Rebuilt the dashboard as an offline React application with actual pinned React Bits FadeContent and SpotlightCard components.
- Added English-first UI and a persistent Indonesian language selector, including scan states, action results and diagnostics. Displayed readings use the received values, without synthetic count-up animation.
- Split browser source from generated firmware assets. Added deterministic gzip output, source/hash checks, a 160 KiB compressed budget and a pinned Node build runtime. Normal PlatformIO builds do not require Node.
- Added chunked, watchdog-fed flash delivery through nonblocking lwIP sends with a transfer deadline.
- Added stable English `quality_code` / `error_code` fields while retaining legacy status `quality`. New CSV shift/quality values and OLED labels use English. Existing archives and NVS record layouts remain intact.
- Rewrote the README and primary technical guides in English with Indonesian editions. Added boot and RS485 timing diagrams, task/queue/lock details and exact build instructions.
- Kept register addresses/scales, GPIO assignments, acquisition ownership, interval schedule and physical storage-format confirmation unchanged.
- Recorded third-party terms and upstream provenance separately from the original GPL-3.0-only code.

## 1.1.0 / 2026-10-26

- Added range-limited Fort scanning across IDs 1–247, six baud rates and N/E/O parity, with progress/cancel/apply controls.
- Validated candidates using energy and CT/PT function-03 responses. CRC, ID, count and ratio checks remain mandatory.
- Kept UART access on core 1, interrupted interval continuity during scan, restored previous settings and required explicit application of results.

## 1.0.1 / 2026-10-22

- Added AGENTS.md, pin-level wiring/netlist, local diagrams/screenshots, pinned dependency documentation and CI checks.
- Verified Fort terminal 58/A and 59/B in the scanned manual and distinguished switch-input 70/COM.
- Preserved backup files on failed archive restoration/replacement. Added fault-injection tests.
- Limited failed CT/PT retries to at least five seconds and fed the watchdog per Modbus attempt.
- Unified version metadata and complete source/documentation packaging.
- Added the full GPLv3 license and scope notice after the owner selected GPL-3.0-only.

## 1.0.0 / 2026-10-22

Initial ESP32-S3 reader: AP/dashboard, SSD1306, DS3231, buttons/buzzer, checked Modbus RTU, CT/PT conversion, six morning and three evening intervals, dual-core execution, NVS checkpoint and LittleFS CSV records.

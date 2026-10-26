# Changelog

**English** · [Bahasa Indonesia](CHANGELOG.id.md)



## Standalone diagnostic 1.1.0 — 2026-10-26

- Separate passive RX/no-TX, protocol response and energy-profile checks; add basic/intermediate/deeper/diagnose and bounded raw register commands.
- Detect matching valid exceptions during communication scan without treating exception data as kWh; retain CRC/ID/function/length validation.
- Add atomic UART parity/framing/FIFO/buffer/break counters and tests executing the real INO with deterministic serial mocks.
- Reinspect the original manual page images and explicitly retain the printed energy-length ambiguity. Main firmware remains 1.3.0.

## 1.3.0 — 2026-10-26

- Add a passive, fixed-RAM capture of the next 12 existing Modbus attempts, with per-attempt UART settings, TX/RX hex, overflow and exact-echo classification. CRC/response validation remains unchanged.
- Add bilingual web capture/start/stop/export controls. Poll full trace data only while its panel is open; preserve captured frames until a new capture or restart.
- Add a standalone Arduino IDE FortSerialDebug sketch without external libraries: read-only kWh/CT/PT, bounded Serial debugging, abortable ID/baud/parity scan, and an optional matching 16 MB partition layout/PlatformIO build.
- Document the supplied automatic-direction four-pin module separately from the manual DE/RE wiring reference. Add native/browser tests and standalone compilation to CI.

## 1.2.1 — 2026-10-26

- Correct the installation RGB default to GPIO48 after confirmation on the user’s board; keep GPIO38 documented as a board-revision alternative.
- Rotate every OLED screen 180° by default through one board setting, before the first boot frame.
- Add onboard RGB Off/Solid/Hue/Blink/Breathing with medium brightness defaults and asynchronous RMT transmission on core 0.
- Add English/Indonesian web controls for mode, color, brightness and cycle duration. Persist a versioned CRC-protected LED record on core 1, with storage status exposed separately from energy recording.
- Document GPIO48/GPIO38 board revisions and distinguish RGB from four-channel RGBW. Extend native/browser verification without changing meter polling or interval rules.

## Repository maintenance — 2026-10-26

- Keep one supported N16R8 hardware target and package only its named flashing images.
- Remove obsolete test-only tooling, stale release archives, build caches, temporary files and an unused banner asset.

## 1.2.0 — 2026-10-26

- Rebuilt the dashboard as an offline React application with actual pinned React Bits FadeContent and SpotlightCard components.
- Added English-first UI and a persistent Indonesian language selector, including scan states, action results and diagnostics. Displayed readings use the received values, without synthetic count-up animation.
- Split browser source from generated firmware assets. Added deterministic gzip output, source/hash checks, a 160 KiB compressed budget and a pinned Node build runtime. Normal PlatformIO builds do not require Node.
- Added chunked, watchdog-fed flash delivery through nonblocking lwIP sends with a transfer deadline.
- Added stable English `quality_code` / `error_code` fields while retaining legacy status `quality`. New CSV shift/quality values and OLED labels use English; existing archives and NVS record layouts remain intact.
- Rewrote the README and primary technical guides in English with Indonesian editions. Added boot and RS485 timing diagrams, task/queue/lock details and exact build instructions.
- Kept register addresses/scales, GPIO assignments, acquisition ownership, interval schedule and physical storage-format confirmation unchanged.
- Recorded third-party terms and upstream provenance separately from the original GPL-3.0-only code.

## 1.1.0 — 2026-10-26

- Added range-limited Fort scanning across IDs 1–247, six baud rates and N/E/O parity, with progress/cancel/apply controls.
- Validated candidates using energy and CT/PT function-03 responses; CRC, ID, count and ratio checks remain mandatory.
- Kept UART access on core 1, interrupted interval continuity during scan, restored previous settings and required explicit application of results.

## 1.0.1 — 2026-10-22

- Added AGENTS.md, pin-level wiring/netlist, local diagrams/screenshots, pinned dependency documentation and CI checks.
- Verified Fort terminal 58/A and 59/B in the scanned manual and distinguished switch-input 70/COM.
- Preserved backup files on failed archive restoration/replacement; added fault-injection tests.
- Limited failed CT/PT retries to at least five seconds and fed the watchdog per Modbus attempt.
- Unified version metadata and complete source/documentation packaging.
- Added the full GPLv3 license and scope notice after the owner selected GPL-3.0-only.

## 1.0.0 — 2026-10-22

Initial ESP32-S3 reader: AP/dashboard, SSD1306, DS3231, buttons/buzzer, checked Modbus RTU, CT/PT conversion, six morning and three evening intervals, dual-core execution, NVS checkpoint and LittleFS CSV records.

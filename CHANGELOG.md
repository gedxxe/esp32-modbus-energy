# Changelog

**English** · [Bahasa Indonesia](CHANGELOG.id.md)

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

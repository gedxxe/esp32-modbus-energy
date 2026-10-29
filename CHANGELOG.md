# Changelog

## 3.1.0

Added the Habuer field-reference profile with Float32 PF/frequency, direct cumulative kWh/kvarh and four independent required reads. Meter capabilities, contact probes, audit plans and discovery bindings now come from shared descriptors, with unsupported registers omitted from acquisition and UI. Existing profile IDs, historical conversion math and checkpoint layouts are preserved. Fixed saved Enterprise-login detection when the outer identity defaults to the username.

## 3.0.0

Added optional WPA2-Enterprise PEAP / EAP-MSCHAPv2 with CA-chain and certificate-date validation plus an optional server-name constraint. The primary form uses username/password and defaults the outer identity to the username, with certificate settings under Advanced Wi-Fi settings. Provisioning is restricted to the local protected AP, saved credentials are never returned in status, and changing the trust target requires entering credentials again. The Wi-Fi schema migrates existing Personal/open profiles and stores CA files by SHA256 without changing the flash partition layout.

The physical build now pins pioarduino 55.03.311, Arduino-ESP32 3.3.11 and ESP-IDF 5.5.5 for the supported EAP APIs. The watchdog setup is adapted to IDF 5, while Modbus source, register profiles and checkpoint schedules retain their existing definitions. Added Enterprise diagnostics, browser coverage and commissioning guides in both languages. Corporate authentication and certificate rejection still require authorized hardware/RADIUS tests.

The RGB adapter uses the current RMT TX API to avoid the legacy/new-driver boot conflict. The Windows SDK cache handles project paths containing spaces and verifies the compiled certificate settings before reuse.

## 2.6.0

Added bounded FC03 discovery with editable multi-address plans, serial framing trials, retained raw confirmations and four named flash records. Reserved IDs survive recovery to an older bank, while schema-3 checkpoints distinguish saved installations and read older records. Serial enumeration, codecs and the HTTP adapter are separated into testable modules. Fixed stale scan results after configuration changes, Acrel ratio bounds, Fort-only flow labels, stop-bit reporting and discovery cancellation. Added bilingual field/developer guidance and browser coverage.

## 2.5.0

Added explicit Fort and Acrel ADL400 V1.3 profiles shared by acquisition, scanner, diagnostics and the web catalog. Transformer layouts, register widths/scales and terminal labels now follow the selected model. Settings migrate without changing installed Fort configurations, and checkpoint schema 2 retains each row’s model while reading schema 1 as Fort. Added bilingual commissioning, alternate full wiring and native/browser coverage.

**English** · [Bahasa Indonesia](CHANGELOG.id.md)

## 2.4.0 / 28 October 2026

Scheduled cumulative kWh checkpoints become the primary record: 06:00–08:00 and 17:00–19:00 WIB at half-hour boundaries. The logger saves the first accepted read requested at or after each target and completed within five seconds, retaining its actual timestamps and source metadata. Missed points are never backfilled or interpolated. Optional adjacent-point differences preserve the original readings and reject changed settings or interrupted continuity.

Core 1 persists an explicit CRC record immediately to NVS and LittleFS, recovers valid backups/temporary records and protects an unarchived previous day before rollover. The dashboard, OLED capture pages, 31-day history and CSV expose the stored cumulative values independently of network availability at capture time. The earlier nine interval windows, NVS key and CSV format remain available as a separate compatibility analysis. Existing Modbus transport and register acceptance are retained.

## 2.3.3 / 28 October 2026

The moving TX/RX wake now rises up to 36 px with a broader crest, giving the dense 2 px tracks a sound-wave shape during travel. Resting dots, responsive point density, alternating directions and the existing arrival fade remain intact. The larger envelope fits inside the track without touching packet labels or endpoint text, and a timeout still leaves the RX lane inactive.

## 2.3.2 / 28 October 2026

The TX/RX tracks now use 2 px points, with a gentle 3 px peak, instead of stretched bars. Their density follows the available width from 64 to 256 points per lane. The existing movement, arrival easing and 360 ms fade remain intact.

The primary kWh display now uses the supplied React Bits Counter with a 650 ms rolling transition, light stagger and three fixed decimals. It preserves English/Indonesian separators, displays missing readings as unavailable and respects reduced motion. Its source matches the supplied component, and the animation follows accepted readings rather than inventing intermediate energy samples. The browser motion regression records frames directly to avoid depending on delayed test-driver calls.

## 2.3.1 / 28 October 2026

TX and RX replay now accelerate and decelerate with quintic easing instead of moving at a constant speed. On arrival, the packet keeps its endpoint position while it and the WakeSlider wave fade together over 360 ms. A static resting track remains underneath, so clearing the active wave no longer flattens it in one frame. A complete request/response replay takes about 3.2 seconds, while the captured UART timestamps retain their original timing.

Regression checks cover endpoint continuity, gradual packet and wave opacity, alternating TX/RX and timeout without an invented response. The representative video ends after the replay has returned to idle rather than cutting off an active packet.

## 2.3.0 / 28 October 2026

The communication view replaces millisecond packet flashes and intersecting taps with two separate WakeSlider tracks. A completed observed transaction plays TX for 1.1 seconds, pauses 140 ms and plays RX for 1.1 seconds only when bytes were actually captured. Playback keeps one current transaction and the newest pending transaction, while the unscaled device timestamps and raw event history remain available. Reboot, missing sequences, changed serial settings, hidden pages and unavailable telemetry clear unfinished playback.

The supplied Counter now renders the RTC-based clock with quiet rolling digits, and StarBorder decorates native menu buttons over a sliding selection background. The old underline is removed. All three supplied JavaScript/CSS pairs match the pinned React Bits upstream hashes, Motion is pinned at 14.7.1, dependency notices are included offline and reduced-motion behavior retains static readings and navigation.

## 2.2.2 / 27 October 2026

The user-supplied Midea XBM is now the first OLED frame and remains visible for three seconds before the existing 1.5-second loading animation and startup sound. It uses the existing Adafruit SSD1306/GFX stack, preserves the original pixels and placement, follows the master 180° rotation and skips the artwork hold when the OLED is absent. `OLED_BOOT_SPLASH_MS` in board.h controls the duration, and the asset generator verifies the original bitmap hash.

## 2.2.1 / 27 October 2026

Wi-Fi discovery now initializes the complete IDF scan configuration, waits for the scan-done callback and records driver failures instead of relying on Arduino's four-second scanComplete deadline. A bounded preparation/retry policy avoids overlapping unfinished association, retains the last SSID list and reports generation, attempt count, reason and driver code in the web panel and USB log.

Browser API requests share a single connection slot through response-body completion, with an eight-second active-request deadline and priority diagnostic Stop/Resume. The server yields empty speculative sockets when another client is waiting. Scan status recovers from an interrupted acknowledgement without automatically starting a replacement scan.

The communication panel adds original device SVG symbols, separate TX/RX lanes, labeled frame motion, last-observed DE-/RE state, transaction milestones and raw bytes. Timeout creates no RX animation. CSS handles page reveals while React Bits SpotlightCard remains active, reducing the compressed dashboard payload. The regression suite now exercises scan failures, lost acknowledgements, delayed HTTP responses and timeout/CRC rendering.

## 2.2.0 / 27 October 2026

Network setup now starts with an asynchronous SSID scan and picker instead of manual SSID entry. The web and OLED can save a private AP IPv4 preference, with v1 Wi-Fi record compatibility and the existing recovery AP policy. OLED options include mode selection, visible-network selection, masked password entry and IP-octet tools.

A flash-resident, lossless 772-frame pack replaces the scattered animation reference. Its web-configurable screensaver defaults to 60 seconds, accepts 10–3600 seconds or disable, and consumes the first physical wake press. The display and tracker share their existing ownership/locks, with no second OLED initialization or animation loop blocking acquisition.

The interface uses blue/violet gradients and glass surfaces with contrast/transparency fallbacks. Clock presentation now advances between status samples without automatic RTC writes and re-anchors every five minutes or after a revised/expired reference. UART event pages are limited to eight entries, with catch-up metadata and slower retry-aware polling. Native tests include the actual decoded frame pack, OLED menu policy, v1/v2 IP records and wake/release handling, while browser tests use the generated gzip assets.

## 2.1.0 / 27 October 2026

The local dashboard now exposes AP/client/combined Wi-Fi configuration with DHCP addresses, a deferred connection change and recovery AP behavior. Credentials pass through core 1 validation and versioned CRC-protected NVS before core 0 applies driver changes, and saved passwords are not returned by status endpoints.

Diagnostic session settings and evidence counters are visible in the web console, with structured register/trial controls and a primary-float shortcut. Capture comparison now rejects changed direction, direction pin and settling/gap timing. The header and OLED follow the current reachable IP, and the new web guide maps every production API route to its interface. Native and generated-byte browser coverage includes the new settings, fallback policy and malformed payloads.

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

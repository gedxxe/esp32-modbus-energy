# Agent instructions / Midea Energy Station

## Purpose and confirmed hardware

Maintain firmware and documentation for an ESP32-S3 N16R8 that reads a selected Fort, Acrel ADL400 Habuer, Acrel ACR10R-DxxTE or Acrel APM800/801/810 or Schneider PM5100 power meter over Modbus RTU and serves a local dashboard. The user's Fort RS485 port is unused: the ESP32 is the only master. The purchased RS485 module is confirmed compatible with ESP32 3.3 V TTL. Treat these as established facts. Do not repeatedly question them or invent a PLC/BMS connection.

The root Fort PDF is the primary meter reference. It is a scanned document. Verify the relevant page image before changing terminal assignments, registers, scaling, or protocol details. The documented meter communication terminals are 58/A and 59/B. Terminal 70/COM belongs to the switch inputs and is not an RS485 ground connection.

The user now confirms a manual-direction transceiver: GPIO17 -> DI, GPIO18 <- RO, GPIO16 -> tied DE and /RE. This supersedes the earlier four-pin automatic-module baseline. Use 100 us settling before TX and after switching to RX, following the user's successful ModbusMaster sketch. Keep automatic direction as an explicit alternative, never the default or an inferred rewiring diagnosis.

The only supported build/release target is `esp32s3` for the physical N16R8 board. Keep experimental tooling and generated caches out of the deployable source/package.

## Sources of truth

- `firmware/platformio.ini`: target, platform, pinned library versions, flash/PSRAM, core affinity.
- `firmware/include/board.h`: GPIO, OLED address, AP credentials, timezone offset.
- `firmware/include/device_identity.h`: full factory-MAC Device ID, unique AP/hostname defaults and explicit legacy AP switch. Never derive a LoRa Node ID automatically or publish real unit identities in fixtures.
- `firmware/include/settings_bank.h`: core-1-only meter/Wi-Fi generation banks with exact readback, legacy import only before any bank exists, degraded-copy reporting and fail-closed recovery. Fresh units require an explicit meter-settings save.
- `firmware/include/capture_archive.h`: version-aware staging promotion that preserves newer archives and conflicting immutable points. Call with the filesystem lock held.
- `firmware/include/version.h`: firmware release version.
- `web/Application.jsx`, `web/views/`, `web/device.js`, `web/i18n.js`: browser views, transport and language catalog.
- `web/flow-playback.js` and `web/components/FlowPlayback.jsx`: continuous observed-frame grouping, readable TX/RX replay and a one-active/one-newest-pending queue. Keep presentation duration separate from actual UART timestamps and never animate RX without captured bytes. Preserve the eased arrival and explicit TX/RX fade phases, retaining the arrived position until opacity reaches zero.
- `web/request.js`: single API socket scheduling through complete body reception, active deadlines and priority Stop/Resume. Do not abort accepted POSTs to refresh telemetry.
- `web/form-validation.js`: shared field validation for Enterprise and LoRa. Reveal incomplete controls inside closed details, show local feedback and respect reduced motion when focusing. Keep LoRa save actions separate from runtime Pair/Sample actions, and prevent unsaved drafts from being presented as the active radio configuration.
- `web/bundle.json` and `tools/build-web.cjs`: generated asset provenance, sizes and reproducible build.
- `firmware/lib/FortMeter/src/SerialSweep.h` and `MeterDiscovery.h`: bounded shared enumeration, editable FC03 plans, two-response evidence, one/two stop bits and cancellation. A response does not identify a manufacturer.
- `firmware/include/meter_config.h`, `meter_registry.h`, `meter_registry_store.h` and `meter_lab.h`: explicit codecs, named inventory, two-bank recovery and CRC-protected ID reservations. Reserve IDs before committing a bank and never reuse them after rollback.
- `firmware/src/meter_lab_web.cpp` and `web/components/MeterLab.jsx`: queue-only HTTP adapter and discovery/registry controls. Keep unknown findings separate from verified production profiles.
- `firmware/include/checkpoint_log.h`: primary ten-point daily cumulative kWh log, actual read timestamps, immutable captures, five-second admission window and explicit CRC serialization. Schema 3 is 446 bytes with immutable saved IDs. Preserve schema 1/2 readers at 426 bytes.
- `firmware/lib/FortMeter/src/MeterProfiles.h`: immutable meter profile IDs and typed register/ratio descriptors. Fort remains ID 0 for old settings and schema-1 captures. Acrel ADL400 follows the supplied V1.3 map. Habuer is ID 2 and follows the supplied field-reference sketch with four independent reads and direct meter totals. Never repurpose an archived profile ID for different scaling. Review docs/METER_PROFILES.md and the local PDF page images before changing it.
- New immutable profiles: ACR ID 3 and APM ID 4 require an existing verified saved-device binding before production. Read docs/ACREL_ACR_APM.md and the supplied PDF page images. ACR ABCD/kWh is a commissioning candidate. APM uses decimal 3000 from the table, not 3050 from its conflicting example. Preserve IEEE754 payloads in the existing raw 32-bit checkpoint slot and reject non-finite reconstructed values.
- ADL400 3.3.0 follows the owner-tested total counter 0x0000, UInt32 ABCD /100 with PT/CT once. Keep exactly one visible ADL400 key acrel-adl400. Historical ID 1 keeps forward semantics and is not selectable; current total uses immutable ID 5. Config migration preserves serial settings and archives. Read docs/ADL400_FIX.md and docs/references/adl400-energy-field-reference.ino.txt. Current ADL400 contact uses energy 0x0000/2, while PT 0x008D/1 and CT 0x008E/1 are independent requests through profiles::readRatioData. Do not restore the optional 0x0064 current probe as an energy prerequisite. Do not add an operator source selector or fit an LCD correction offset.
- `firmware/lib/FortMeter/src/MeterReadings.h`: shared required-reading policy, typed validation and atomic snapshot acceptance. Preserve Habuer starting addresses 0x0030/0x0032/0x0034/0x0035 as separate requests, and never manufacture unsupported current, voltage, power or CT/PT registers.
- `firmware/include/energy.h`: retained legacy interval schedule, continuity/quality logic and CRC/frame validation.
- `firmware/include/debounce.h`: button event timing.
- `firmware/lib/FortMeter/src/FortTransport.h` and `FortDiagnostics.h`: source-shared ModbusMaster transport and diagnostic console for main firmware and the standalone INO. The old diagnostic_engine.h and build-diagnostic.py paths are compatibility/provenance entry points. Do not create a second parser there.
- `firmware/lib/FortMeter/third_party/ModbusMaster/`: pinned v2.0.1 source provenance, service-hook patch and upstream license. Keep actual-library native tests and the source hashes in sync.
- `firmware/include/flow.h`: bounded timestamped UART-event ring. Preserve sequence gaps and distinguish observed events from schematic animation.
- `firmware/include/process_monitor.h`: bounded IRAM tick sampling. Do not present tick samples as exact instruction-time CPU accounting.
- `firmware/include/trace.h`: fixed-size passive UART capture and diagnostic classification. Never bypass CRC/frame validation or silently strip echo bytes in the main firmware.
- `examples/FortSerialDebug/`: standalone Arduino diagnostic. Keep it read-only and use the same FortMeter source library as production. Runtime options and the eight cases/*.txt recipes must match both language guides and the actual-INO host tests. Do not collapse transport detection into energy-profile acceptance.
- `firmware/include/led.h`: RGB modes, brightness, rollover-safe animation and versioned CRC-protected NVS encoding. `rgb_driver.h` and `rgb_driver.cpp` own the IDF 5 asynchronous RMT adapter. Initialize it first on core 0, retain its persistent symbol buffer until completion, and never link the legacy RMT driver.
- `firmware/include/screen_saver.h`, `firmware/assets/oled/` and `tools/build-oled.py`: first-frame boot artwork, idle policy, consumed wake press, versioned display preferences and lossless RLE frame generation. Keep the boot XBM in LSB-first format and its 3-second hold before the loading animation. Preserve artwork attribution and keep the original reference outside deployable source.
- `firmware/include/oled_menu.h`: button-driven Wi-Fi modes, scan selection, masked password editor and custom AP IPv4 editor. Keep menu policy host-testable and writes queued through core 1.
- `web/components/EnergyCounter.jsx`: animate only accepted numeric kWh readings with three locale-aware decimals. Preserve the missing-value placeholder and reduced motion, and keep measured values separate from rolling numeral positions.
- `web/components/DeviceClock.jsx`: monotonic presentation from a device UTC reference, re-anchored every five minutes or on a clock revision. Never synchronize RTC automatically from browser polling.
- `firmware/include/enterprise_wifi.h` and `firmware/src/enterprise_wifi.cpp`: PEAP-only authentication, pinned ESP-IDF 5.5.5 MSCHAPv2 defaults, CA-chain/time validation using the full SDK public trust bundle by default or a pinned custom CA, with an optional explicit server-name constraint and immutable CA storage. Never disable validation or fall back to another authentication method. Keep the custom SDK date/key-usage checks and compile guards enabled. The verified shared SDK bootstrap cache avoids ESP-IDF whitespace limits without moving production source files. Provision Enterprise credentials only through the protected AP interface, and do not claim that stock NVS storage is encrypted.
- `firmware/include/network_attempt.h` owns bounded preparation/authentication/DHCP stages and retained RAM failure evidence. Only a fresh matching GOT_IP event confirms a station connection. `firmware/src/enterprise_station.cpp` checks the production SDK adapter calls, while certificate parsing/storage and security guards remain in `enterprise_wifi.cpp`. New Enterprise credentials remain AP-only, but an unchanged saved login may be retried through the client interface without resending secrets. LoRa key configuration uses authorized dashboard commands on either local interface.
- `firmware/include/network_settings.h` and `web/components/NetworkControls.jsx`: AP/client/dual configuration, versioned CRC record, retry/grace timing and local-address display. Wi-Fi driver calls belong to core 0 and credential Preferences to core 1. Never return stored client passwords through HTTP status.
- `firmware/include/clock_guard.h`, `command_policy.h`, `bounded_write.h`: retained-day/queued-time protection, execution-mode arbitration and bounded HTTP writes.
- `firmware/include/lora_protocol.h`, `lora_service.h` and `firmware/src/lora_service.cpp`: optional Ra-01SH SX1262 raw-LoRa node, disabled by default at editable 915 MHz. Keep SPI and IRQ processing on its core-0 task, Preferences/archive reads on core 1, bounded BUSY/startup waits, addressed HMAC/session/ACK validation, immutable retries and freshness limits. Never infer pairing from radio initialization or add over-air meter writes. The separate C3 gateway must follow docs/LORA_PROTOCOL.md.
- `docs/LORA_PTMP.md` and `docs/GATEWAY_PROMPT.md`: shared-receiver topology, identifier scope, airtime planning and the separate C3 implementation handoff. Accept matching outstanding ACKs during retry backoff, retain bounded first-TX contention, and do not present polling as TDMA or claim physical multi-node acceptance from software tests.
- `firmware/include/verified_record.h` and `recovery.h`: verified NVS writes and validated archive promotion. Preserve recoverable staging before truncation, quarantine corrupt canonical files before promotion, and pause production if saved meter settings fail recovery. See docs/POWER_LOSS.md.
- `firmware/src/main.cpp`: hardware drivers, RTOS ownership, persistence and HTTP API.
- `docs/DESIGN.md`: manual page references and documented limitations.

Keep the README, wiring SVG, netlist, diagrams, tests and release package consistent with these files. Name the root guide `AGENTS.md` and root overview `README.md` using this exact case for GitHub/Linux portability.

Schneider PM5100 uses immutable profile ID 6 (`schneider-pm5100`). See docs/SCHNEIDER_PM5100.md and docs/references/pm5100-registers.json. Its one-based workbook register 2700 is PDU 2699, two-register Float32 import kWh with no additional CT/VT multiplier. PF uses the explicit PF4Q_F32 decoder. Never truncate its separate four-register INT64 Wh counter into the existing 32-bit raw checkpoint slot.

## Firmware invariants

1. Read the meter with Modbus function 03 only. Do not add register writes, automatic meter reconfiguration or resets without an explicit product requirement.
2. Reject bad CRC, wrong ID/function/length and exception frames. CRC integrity is never bypassed to match a typo in the manual.
3. Use the selected profile energy basis. Apply CT/PT once to secondary integer counters, and do not multiply meter-reported totals again. Keep cumulative calculations in `double`. Never treat missing data as zero.
4. Primary checkpoints are 06:00, 06:30, 07:00, 07:30, 08:00 and 17:00, 17:30, 18:00, 18:30, 19:00 WIB. Store actual cumulative readings with their request/response RTC timestamps, never interpolation or late backfill. The earlier nine consumption windows remain a separate legacy analysis/API and must not overwrite checkpoint or historical data. All day boundaries are WIB (UTC+7). DS3231 stores UTC.
5. Missing/invalid RTC, reboots, counter resets, changed CT/PT and excessive sample gaps must not yield a complete interval. Preserve completed intervals.
6. Core 1 exclusively owns UART, Preferences and tracker mutation. Core 0 handles HTTP/UI. Exchange copies through RTOS queues. Lock I2C and filesystem access. Keep waits bounded and leave time for idle/Wi-Fi tasks.
7. Do not automatically format a failed filesystem. Retain the documented physical confirmation procedure and preserve recoverable backup files on failed operations.
8. Core 0 exclusively owns RGB RMT channel 0. Keep transmission asynchronous, skip busy frames and write LED Preferences only on core 1. GPIO48 is the confirmed onboard LED pin for this installation.
9. Active diagnostics must interrupt interval continuity, keep trial settings/values out of production persistence and restore board pins/saved serial configuration on resume. Stop/resume stays bounded. Never drop an accepted queued command silently. Read-only report/status inspection must not pause recording.
10. Checkpoint captures persist immediately through core 1 to explicit CRC records in NVS and LittleFS. Protect an unarchived previous day before rollover, block further rollover when both recovery slots are occupied, and preserve valid backups/temporary records. Never infer a captured checkpoint merely from a live browser value.
11. Boot without waiting for USB Serial. An absent OLED must not prevent AP/measurement startup.

## Diagnostic evidence and operator instructions

- Every field command example must name its destination: **web Command input + Run command**, or **Arduino IDE Serial Monitor INPUT + Enter/Send** with baud/line ending. Explicitly distinguish copying resulting OUTPUT to the support chat. Do not use an unqualified “send this block”. Label computer-terminal commands separately.
- Never infer module absent, unpowered or damaged solely from UART silence or weak-pull sampling. The four-pin module exposes no presence/power-good telemetry. Keep measured observations, user-labeled connection states and physical hypotheses separate.
- Diagnostic multi-address plans must continue after a silent first probe. Keep production map acceptance separate from a valid Modbus data/exception response. Compare snapshots only for matching settings and completed tests.

## Documentation and visuals

Write connected explanatory paragraphs with enough context for an embedded engineer. Avoid clipped fragments, repeated semicolons and em-dash phrasing in first-party documentation, UI copy and diagnostic prose. Keep concrete steps and tables where they help the reader act or compare.

- English is the primary language. Keep README.md and technical guides in English with linked Indonesian counterparts (README.id.md and docs/id/). The web UI must switch between English and Indonesian, defaulting to English. Keep steps and pin-to-pin instructions concrete.
- Browser sources live in web/. Use real vendored React Bits components with their upstream license and provenance. Keep third-party terms separate from original GPL-3.0-only code. Do not relabel dependencies as GPL.
- Build browser assets with npm run build:web. Commit the generated gzip header and web/bundle.json. Plain PlatformIO builds verify hashes without requiring Node. Keep the gzip payload below 160 KiB. Test the generated bytes, not an alternate mock UI.
- Use repository-local SVGs for wiring, icons/badges, topology and OLED illustrations. No external image/CDN dependency for the README or embedded dashboard.
- Generate vector assets with `tools/generate_docs.py`. Edit its data/layout rather than hand-diverging generated SVGs.
- Label UI screenshots and OLED illustrations as simulated/representative. Never present them as hardware acceptance evidence.
- The installed manual-direction full wiring diagram must show power/ground, shared I2C, RO/DI direction, tied DE-/RE, Fort A/B terminals, all three buttons and the buzzer driver. Explain repeated net labels and that logical port positions are not a board photograph.
- Keep uncertain physical package pin order explicit: a transistor's B/C/E locations depend on its part/package. Do not guess them.
- Version badges must match the pinned build configuration. The user selected GNU GPL version 3 only (`GPL-3.0-only`). Preserve LICENSE/NOTICE and do not relicense third-party dependencies or the Fort manual. Do not invent a GitHub owner/repository URL, passing CI badge or hardware test result.
- Markdown links used in repository files must be relative and case-correct. Chat links use absolute paths.

## Verification

Run checks appropriate to the change. Do not broaden/repeat them after they pass without a new reason.

- Native logic on Windows: `./tools/test.ps1` (g++ required).
- Native logic on Linux/macOS: `g++ -std=c++11 -Wall -Wextra -Werror -Ifirmware/include firmware/test/test_energy.cpp firmware/src/oled_frames.cpp -o /tmp/fort-test && /tmp/fort-test`.
- Documentation, assets, versions and relative links: `python tools/check_docs.py`.
- Firmware: from `firmware`, `pio run`. Use the installed PlatformIO Python module if `pio` is absent from PATH.
- Standalone sketch: `pio run -d examples/FortSerialDebug` (same ESP32-S3 N16R8 target and local FortMeter library).
- Enterprise browser checks: `node tools/preview-test.cjs [playwright-module-path] [browser-channel] --enterprise-only`.
- Discovery browser checks: `node tools/preview-test.cjs [playwright-module-path] [browser-channel] --meter-lab-only`.
- Browser/UI and SVG visual checks: `node tools/preview-test.cjs [playwright-module-path] [browser-channel]`. The script uses a localhost mock, not real hardware.

After changing firmware, rebuild before copying binaries. Update `docs/VALIDATION.md` with actual outcomes and limitations, rebuild the release package, verify its manifest and ZIP integrity. Include all documentation/assets/tools in the ZIP. Never package `.pio`, temporary files or dependency directories. Follow `output/firmware/FLASHING.md` for image offsets.

## GitHub readiness

Review `.gitignore`, relative image links, reproducible dependency versions, CI and the license status. Preparing a repository for GitHub does not itself request publishing, creating a remote, or pushing. Preserve the source PDF and user files. Do not make generated ZIPs or binaries the only copy of source code.

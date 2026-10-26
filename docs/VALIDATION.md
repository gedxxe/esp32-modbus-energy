# Validation record — 26 October 2026

**English** · [Bahasa Indonesia](id/VALIDATION.md)

Firmware **1.3.0**. Results below were obtained locally; they are separate from physical acceptance and remote CI. See [COMMISSIONING](COMMISSIONING.md) for hardware checks.

## Native logic — passed

`tools/test.ps1`, g++ C++11 with `-Wall -Wextra -Werror`.

- CRC example from the Fort manual, corrected response, bit corruption, wrong ID/count/length and invalid CRC rejection.
- All nine interval windows; interpolated boundaries, late startup, gaps at/above 15 s, missing data, clock reversal, NaN/negative/reset/date handling and preserved completed intervals after reboot interruption.
- Button bounce/short/long-press behavior, including millis rollover.
- Archive fault injection: failed rotation/install/restore retains recoverable copies.
- CT/PT retry/backoff, scan ranges, 4,446 unique candidates, preferred-ID mode priority, single-ID range, duplicate start, cancel/restart/found/exhausted and elapsed-time rollover.
- RGB Off/Solid/Hue/Blink/Breathing, brightness scaling, CRC-protected record round-trip/corruption/length rejection, parameter bounds and animation millis rollover.
- Raw capture: disabled/start/stop/reset, twelve-attempt bound/freeze, exact echo versus valid response, short/CRC/overflow/wrong-ID/exception classification. Echo and overflow prefixes never pass the response validator.
- Stable English quality codes and compatibility of the legacy status names.

These tests execute the same header logic used by the firmware. They do not run the ESP-IDF tasks or peripheral drivers.

## Browser application — passed

`tools/preview-test.cjs` with Playwright and installed Microsoft Edge. The test server reads the **gzip byte arrays in the generated firmware header**; it does not substitute another HTML implementation. The JSON/CSV APIs are explicitly simulated.

- English default; Indonesian switching and decimal formatting; language retained after reload; English fallback when localStorage is blocked.
- Six morning/three evening rows; missing intervals are not displayed as zero completed consumption.
- Serial config fields, phone time request, POST token and CSV download selection.
- LED mode/color/brightness/cycle POST fields and token; draft retained across polls; all five modes; applied-state reload; storage warning, driver unavailable, disconnected controls and Indonesian labels. These use simulated NVS/API results, not a physical persistence test.
- Scan range validation, progress/disabled controls, reload recovery, cancellation, explicit result application, no match, translated HTTP error and five-second POST timeout recovery.
- Raw panel: no trace fetch while closed, explicit capture/stop, exact-echo/CRC/timeout/valid labels, POST token, text export contents, translated labels, API-failure controls and 320–1440 px capture layouts.
- Stale/offline status, retained last sample and hidden live power; mutation buttons remain disabled when disconnected, including after navigation.
- Widths 320, 390, 768, 1440 and 2560 px without document overflow; reduced-motion rendering; no JavaScript page errors or external asset requests.
- Vendored React Bits file contents match their recorded upstream Git blob hashes.
- All **17 SVGs** rendered with text bounds inside their viewBox. Dashboard, phone and RGB-panel screenshots were inspected visually.

RGB control illustrations: [desktop](assets/ui-led-desktop.png), [phone](assets/ui-led-mobile.png). OLED 180° orientation, actual GPIO48 light output, RMT timing/jitter and restart persistence require physical acceptance.

Screenshots contain simulated data: [desktop](assets/ui-desktop.png), [phone](assets/ui-mobile.png), [Indonesian phone](assets/ui-mobile-id.png), [scan](assets/ui-scan-desktop.png).

## Browser payload — passed

Node **24.22.1**, esbuild **0.28.2**, React/React DOM **19.3.1**, GSAP **3.15.0**. `npm run build:web` and `npm run check:web` completed. Input hashes normalize text line endings; gzip OS metadata is normalized so generated files can be checked across Windows/Linux.

| Endpoint | Uncompressed bytes | Gzip bytes |
| --- | ---: | ---: |
| / | 1030 | 527 |
| /assets/app.js | 397475 | 134464 |
| /assets/app.css | 16974 | 4442 |
| /assets/third-party.txt | 5755 | 1433 |

Combined payload: **140866 bytes**, within the 160 KiB build limit. PlatformIO's Python-only source/header guard passed before the hardware firmware build. Source maps, external fonts and CDN imports are not shipped. The web application/animations execute in the browser; the device serves flash-mapped bytes in bounded chunks.

## Firmware — passed

`pio run -e esp32s3`, Espressif32 **6.10.0** / Arduino ESP32 **2.0.17**, exit code 0. Final compile logs contained no compiler warnings/errors.

| Target | Static RAM / 327,680 bytes | Program / app partition |
| --- | ---: | ---: |
| ESP32-S3 N16R8, 16 MB flash / OPI PSRAM | 47,784 (14.6%) | 1,010,581 / 6,553,600 (15.4%) |

Static RAM is not total runtime heap/stack use. Successful compilation does not measure HTTP transfer latency or validate physical UART turnaround. Hardware factory offsets are bootloader 0x0, partitions 0x8000, boot_app0 0xE000 and app 0x10000.

## Documentation, package and CI

`tools/check_docs.py` checks nested English/Indonesian links and anchors, SVGs, pin/netlist agreement, versions, full GPL text and generated web hashes. The packager includes web source and dependency notices alongside firmware/docs, compares app/factory components to the current build and verifies ZIP CRC plus SHA-256 manifest entries.

The earlier 1.1.0 CI run passed logic/docs and browser but failed firmware setup because the pip cache searched for requirements.txt/pyproject.toml rather than requirements-dev.txt. The workflow now specifies the correct cache-dependency-path. It also checks the committed React build with pinned Node 24.22.1. Remote status for this source commit must be read from GitHub Actions; no remote pass is inferred from local results.

## Hardware limits

The user confirmed GPIO48 as the correct onboard LED pin on 26 October 2026. Complete hardware acceptance for version 1.3.0 is not recorded here. The user's 23 October log showed an earlier application booting on a bare ESP32-S3 with no I2C modules attached; that is not acceptance evidence for this release.

Fort UART scanning/scaling, OLED/RTC I2C, battery backup, buttons/buzzer, AP radio under multiple clients, noise, power-cut recovery and endurance remain unverified on physical hardware. Neither a DOM test nor a C++ unit test covers them.

## Repository maintenance

After removing the obsolete test target/tooling and unused banner, `pio run -e esp32s3` passed with the same 47,088-byte static RAM / 996,293-byte program footprint. The 17 remaining SVGs rendered without text overflow. The Python source/header guard also passed after local Node dependencies were removed.

The release packager selects the five named N16R8 BIN files explicitly. Temporary render/build files, installed dependencies and old release archives are removed after verification; the current hardware package, browser source/generated header, documentation assets and original Fort manual are retained. A later developer build recreates caches as needed.

## GPIO48 documentation correction — 26 October 2026

The user changed `RGB_LED_PIN` to 48 in board.h; that value is retained. README/design/commissioning, agent instructions, fallback UI and simulated RGB screenshots now agree with it. The browser mock reads the pin directly from board.h to avoid future screenshot drift. GPIO38 remains documented only as a board-revision alternative.

The corrected source passed the documentation/hash checks and the embedded-asset browser suite. The N16R8 firmware rebuilt successfully without compiler warnings: static RAM 47,240 bytes, program 1,002,577 bytes, browser gzip 138,395 bytes. The source/binaries/package were regenerated together; full hardware acceptance remains separate from the user's pin confirmation.

## Raw diagnostics and standalone sketch — 1.3.0

The new native classifier/capture cases and the embedded-asset browser suite passed locally. Representative raw capture screenshots use simulated echo/CRC/timeout/valid frames: [desktop](assets/ui-trace-desktop.png), [phone](assets/ui-trace-mobile.png). These are not a capture from the user's Fort. Native tests do not exercise the peripheral driver or establish the source of observed echo.

The main firmware built successfully with no compiler warnings. Added snapshot memory remains fixed and is included in the static-RAM figure; runtime queue/stack high-water marks and four-client latency are unmeasured.

`FortSerialDebug.ino` compiled as an Arduino sketch with Arduino ESP32 2.0.17 / espressif32 6.10.0, with no external library dependencies. Standalone diagnostic 1.2.0 static RAM: **21,068 bytes**; program: **323,793 bytes**. The included PlatformIO configuration also compiled successfully after INO-to-C++ source filtering was verified. The Arduino IDE GUI, other ESP32 core versions, serial interaction and physical Fort reads are not hardware-tested. Its optional PlatformIO entry is compiled in CI as the same N16R8 board target. The sketch does not modify persistent storage; upload-tool partition/erase selections remain a separate concern addressed by its guide and supplied CSV.

## Layered standalone diagnostic 1.1.0 — checked locally

The original manual images at PDF pages 18, 27 and 30 were visually inspected. The energy-length inconsistency is retained in DESIGN; no installed-meter acceptance is inferred. The sketch compiles on Arduino ESP32 2.0.17 without warnings, and `tools/test.ps1` now also compiles/runs the actual INO against deterministic host UART/Serial mocks. Cases cover passive listening sends zero requests; valid exception/data establishes a reply; wrong ID/CRC/echo does not; scan stops on valid transport independently of energy; exceptions never yield kWh; valid 32-bit/ratio conversion; FC03-only requests; register bounds; abort and UART counters. This is behavioral evidence for the code, not evidence of clean A/B waveforms, correct installed register mapping or on-target UART callback timing. The main firmware is unchanged at 1.3.0.

## Runtime diagnostic 1.2.0 — checked locally

The actual-INO host suite passed configurable probe/pins/timeouts/interval, scan-mode/list counts, duplicate/invalid-input rejection, raw-plan limits, decode-off no-TX/no-kWh, changed address/scale/word-order conversion, command retention during UART waits, comment/HEX-prefix behavior, queue overflow/stop and all eight pasted case TXT recipes. Each recipe completed without unknown commands/queue overflow; every transmitted request remained eight-byte FC03 with a valid CRC and nonbroadcast ID. The ESP32-S3 sketch compiled successfully without compiler warnings: 21,068 bytes static RAM, 323,793 bytes program. The main1.3.0 firmware and browser assets were not changed. Hardware callback timing, pins, signaling and installed-map correctness remain outside these tests.

## Evidence-based diagnostic 1.3.0 — 2026-10-26

The actual-INO host suite passed silent-first-address fallback, data versus exception reports, exact echo followed by a separately delimited good response, unavailable/partial-write UART, pre-TX RX accounting, weak-bias following/HIGH/LOW/variable observations, UART restoration on abort, matched/mismatched/aborted paired captures, contradictory isolation, and local-activity comparisons. All eight command recipes passed again, including the expanded raw one/two/three-word energy-entry audit. Production native logic tests also passed.

The standalone ESP32-S3 N16R8 sketch compiled with espressif32 6.10.0 / Arduino ESP32 2.0.17 and no compiler warnings: **21,452 bytes static RAM; 331,109 bytes program**. No external libraries are required. The original PDF27/30 page images were inspected directly; the printed energy-length discrepancy remains unresolved. This does not verify the installed meter's map or scaling.

Documentation checks passed: 23 Markdown files, 197 local links/anchors, 18 SVGs and the 31-wire manual-alternative netlist. Regression checks now require the actual diagnostic version and explicit Serial Monitor INPUT/OUTPUT destinations in both field guides and all recipes. Both generated wiring SVGs were rendered in Edge and inspected. Their port locations remain schematic, not a board photograph.

The main firmware 1.3.0 and browser payload were unchanged, so their previous build/browser results were not rerun or relabeled as new hardware tests. The rebuilt package validates its ZIP CRC and every manifest SHA256 against the archived bytes. UART driver timing, RX weak-pull behavior, module presence/health, differential A/B signals and physical Fort register acceptance remain untested. The software deliberately reports ambiguity where the interface lacks the telemetry to distinguish causes.

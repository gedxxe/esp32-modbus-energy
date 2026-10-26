# Validation record — 26 October 2026

**English** · [Bahasa Indonesia](id/VALIDATION.md)

Firmware **1.2.0**. Results below were obtained locally; they are separate from physical acceptance and remote CI. See [COMMISSIONING](COMMISSIONING.md) for hardware checks.

## Native logic — passed

`tools/test.ps1`, g++ C++11 with `-Wall -Wextra -Werror`.

- CRC example from the Fort manual, corrected response, bit corruption, wrong ID/count/length and invalid CRC rejection.
- All nine interval windows; interpolated boundaries, late startup, gaps at/above 15 s, missing data, clock reversal, NaN/negative/reset/date handling and preserved completed intervals after reboot interruption.
- Button bounce/short/long-press behavior, including millis rollover.
- Archive fault injection: failed rotation/install/restore retains recoverable copies.
- CT/PT retry/backoff, scan ranges, 4,446 unique candidates, preferred-ID mode priority, single-ID range, duplicate start, cancel/restart/found/exhausted and elapsed-time rollover.
- Stable English quality codes and compatibility of the legacy status names.

These tests execute the same header logic used by the firmware. They do not run the ESP-IDF tasks or peripheral drivers.

## Browser application — passed

`tools/preview-test.cjs` with Playwright and installed Microsoft Edge. The test server reads the **gzip byte arrays in the generated firmware header**; it does not substitute another HTML implementation. The JSON/CSV APIs are explicitly simulated.

- English default; Indonesian switching and decimal formatting; language retained after reload; English fallback when localStorage is blocked.
- Six morning/three evening rows; missing intervals are not displayed as zero completed consumption.
- Serial config fields, phone time request, POST token and CSV download selection.
- Scan range validation, progress/disabled controls, reload recovery, cancellation, explicit result application, no match, translated HTTP error and five-second POST timeout recovery.
- Stale/offline status, retained last sample and hidden live power; mutation buttons remain disabled when disconnected, including after navigation.
- Widths 320, 390, 768, 1440 and 2560 px without document overflow; reduced-motion rendering; no JavaScript page errors or external asset requests.
- Vendored React Bits file contents match their recorded upstream Git blob hashes.
- All **18 SVGs** rendered with text bounds inside their viewBox. Dashboard and phone screenshots were also inspected visually.

Screenshots contain simulated data: [desktop](assets/ui-desktop.png), [phone](assets/ui-mobile.png), [Indonesian phone](assets/ui-mobile-id.png), [scan](assets/ui-scan-desktop.png).

## Browser payload — passed

Node **24.22.1**, esbuild **0.28.2**, React/React DOM **19.3.1**, GSAP **3.15.0**. `npm run build:web` and `npm run check:web` completed. Input hashes normalize text line endings; gzip OS metadata is normalized so generated files can be checked across Windows/Linux.

| Endpoint | Uncompressed bytes | Gzip bytes |
| --- | ---: | ---: |
| / | 1030 | 527 |
| /assets/app.js | 385639 | 130545 |
| /assets/app.css | 15755 | 4164 |
| /assets/third-party.txt | 5755 | 1433 |

Combined payload: **136669 bytes**, within the 160 KiB build limit. PlatformIO's Python-only source/header guard passed before both firmware builds. Source maps, external fonts and CDN imports are not shipped. The web application/animations execute in the browser; the device serves flash-mapped bytes in bounded chunks.

## Firmware — passed

`pio run -e esp32s3 -e cirkit`, Espressif32 **6.10.0** / Arduino ESP32 **2.0.17**, exit code 0 for both. Final compile logs contained no compiler warnings/errors.

| Target | Static RAM / 327,680 bytes | Program / app partition |
| --- | ---: | ---: |
| ESP32-S3 N16R8, 16 MB flash / OPI PSRAM | 47,088 (14.4%) | 996,293 / 6,553,600 (15.2%) |
| Cirkit profile, 8 MB flash / no PSRAM | 46,896 (14.3%) | 992,773 / 3,342,336 (29.7%) |

Static RAM is not total runtime heap/stack use. Successful compilation does not measure HTTP transfer latency or validate physical UART turnaround. Hardware factory offsets are bootloader 0x0, partitions 0x8000, boot_app0 0xE000 and app 0x10000.

## Documentation, package and CI

`tools/check_docs.py` checks nested English/Indonesian links and anchors, SVGs, pin/netlist agreement, versions, full GPL text and generated web hashes. The packager includes web source and dependency notices alongside firmware/docs, compares app/factory components to the current build and verifies ZIP CRC plus SHA-256 manifest entries.

The earlier 1.1.0 CI run passed logic/docs and browser but failed firmware setup because the pip cache searched for requirements.txt/pyproject.toml rather than requirements-dev.txt. The workflow now specifies the correct cache-dependency-path. It also checks the committed React build with pinned Node 24.22.1. Remote status for this source commit must be read from GitHub Actions; no remote pass is inferred from local results.

## Hardware and emulator limits

Version 1.2.0 has not been flashed/tested on the user's physical board. The user's 23 October log showed an earlier application booting on a bare ESP32-S3 with no I2C modules attached; that is not acceptance evidence for this release.

Fort UART scanning/scaling, OLED/RTC I2C, battery backup, buttons/buzzer, AP radio under multiple clients, noise, power-cut recovery and endurance remain unverified on physical hardware. Neither a DOM test nor a C++ unit test covers them.

Historical Cirkit importer tests recognized the three-role ZIP and passed the invalid-header stage with a 16 MB bundle, then stopped at the emulator's 8 MB size mismatch. The dedicated profile's image headers/partitions are checked for 8 MB. A complete emulator boot/AP/RTC/Modbus run is still unverified because the test browser session timed out. No new emulator acceptance claim is made here.

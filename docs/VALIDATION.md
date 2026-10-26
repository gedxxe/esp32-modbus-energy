# Validation record — 27 October 2026

**English** · [Bahasa Indonesia](id/VALIDATION.md)

Firmware **1.4.1**, standalone **FortSerialDebug 1.3.1**. This record describes local software checks. Physical acceptance of the installed Fort is still required; use [COMMISSIONING](COMMISSIONING.md) and the [field diagnostic guide](DIAGNOSTICS.md). Previous release changes remain in [CHANGELOG](../CHANGELOG.md).

## Firmware compilation — passed

Both targets compiled with the pinned espressif32 **6.10.0** / Arduino ESP32 **2.0.17** toolchain, with no compiler warnings/errors in the final output. The target is the physical ESP32-S3 N16R8: 16 MB flash, OPI PSRAM.

| Build | Static RAM / 327,680 bytes | Program / 6,553,600 bytes |
| --- | ---: | ---: |
| `pio run -e esp32s3` in `firmware/` | 79,328 | 1,094,969 |
| `pio run -d examples/FortSerialDebug` | 21,476 | 332,101 |

Static RAM is not runtime heap/stack usage. Core 0's loop stack is 16 KiB and acquisition stack is 12 KiB. Queue storage, peripheral-driver allocations, real high-water values and sustained multi-client latency need on-device measurement. The standalone sketch has no external library dependencies; the Arduino IDE GUI and other ESP32 core versions were not exercised.

## Native regressions — passed

`tools/test.ps1` builds three C++11 suites with `-Wall -Wextra -Werror`. The standalone suite includes the actual INO, and the production diagnostic suite includes the generated engine used by main firmware.

- CRC corruption, incorrect ID/function/count/length, short/oversized/deadline-tail frames, exact echoes, concatenated request prefixes and valid exceptions. A valid exception establishes transport only and never produces kWh.
- Delayed valid bytes arriving **during** the pre-TX quiet guard are discarded. Sustained pre-TX RX reaches a bounded no-TX bus-busy outcome. Exact echo plus a byte-wise 19200/8E1 response at minimum legal turnaround is accepted as two separately observed frames. A failed UART initialization is retried at the same serial settings. These new cases execute against both the real INO and generated production engine.
- Silent-first-address fallback, configured raw plans, runtime pins/ID/baud/parity/timeouts/profile settings, scan lists/current-format mode, weak-pull observations, paired-capture configuration/abort guards, queued console input and all eight field recipes. Every emitted test request remains FC03 with a valid CRC and nonbroadcast ID.
- Scan enumeration, preferred ID/format ordering, 4,446 unique matrix candidates, 247 current-format candidates, single-ID completion, invalid ranges, cancellation/restart and millis rollover.
- Nine scheduled windows, interpolation, continuity gaps, missing samples, reboot/RTC/counter discontinuity, CT/PT changes, NaN/negative values and completed-record preservation. Backward local dates remain blocked independently of RTC availability; queued clock requests are age-compensated and revalidated at bounds/rollover.
- Ordinary command mode/revision/10-second age guards, including scan/maintenance transitions and rollover. These pure guards do not execute FreeRTOS queues concurrently; the main integration was reviewed and compiled.
- Bounded HTTP writer under partial progress, retry/no-progress, fatal error and timer rollover. Progress does not renew the total deadline; chunks remain at most 1 KiB and service callbacks run between attempts. These are socket fault-injection mocks, not a Wi-Fi saturation test.
- Archive replacement fault recovery, RGB modes/versioned CRC-protected settings, debouncing and button timing, raw trace bounds/freeze/stop, UART event ordering and cursor wrap.

Tests do not prove real line timing, bus topology, module power/health, correct physical register mapping or actual installed-meter scaling. Frame separation uses task-observed buffer arrivals. RTOS stalls can hide a physical gap; RTU has no transaction ID for conclusive attribution of every delayed reply.

## Embedded browser — passed

`tools/preview-test.cjs` with Playwright and installed Microsoft Edge reads **the generated gzip arrays** in `firmware/include/web_ui.h`; APIs are simulated. No substitute HTML UI is tested.

- English default/Indonesian switching, persistence and blocked-storage fallback; all six morning and three evening rows; missing values remain missing.
- Serial settings, phone time, LED modes/drafts/brightness/storage feedback, mute, scan mode/probe/range/progress/cancel/apply, CSV selection/download and raw capture/export.
- HTTP 202 does not unlock ordinary controls before the exact execution result. Delayed completion and explicit rejection are exercised. Clock sync is disabled in maintenance; initial diagnostic state must load before Run is enabled. Priority Stop remains available while an ordinary action waits.
- Malformed status slots, archive rows, process/core rows, raw-trace frames and UART events retain the last usable data and mark the panel stale/unavailable without crashing the application.
- Diagnostic command/paste guard, pause/resume/log download, EN/ID labels and closed-panel polling. Process table, heap/PSRAM and stack metrics are simulated.
- UART motion requires new events; idle time and pre-reboot history create no packet motion. Reduced motion is honored. All animation is a timestamp replay with network delay, not a wire analyzer.
- Layouts from 320 to 2560 px, no document overflow, no JavaScript page errors, no external asset requests. Vendored React Bits files match upstream Git blob hashes. All 19 SVGs render with text inside their viewBox.

Representative, simulated screenshots: [dashboard](assets/ui-desktop.png), [phone](assets/ui-mobile.png), [scan](assets/ui-scan-desktop.png), [raw capture](assets/ui-trace-desktop.png), [communication](assets/ui-flow-desktop.png), [process monitor](assets/ui-process-desktop.png), [phone diagnostics](assets/ui-diagnostic-mobile.png). These are not hardware evidence.

## Asset provenance and documentation — passed

Node **24.22.1**, esbuild **0.28.2**, React/React DOM **19.3.1**, GSAP **3.15.0**. The browser header and manifest were rebuilt together after adding validation source. Python's input/header hash guard passed inside the successful PlatformIO build. A normal firmware checkout/build does not require Node. A deliberate extra web input was also tested through actual PlatformIO: it failed with the intended recovery instructions and no Python traceback; the temporary input was removed and the original provenance check passed again.

| Endpoint | Uncompressed bytes | Gzip bytes |
| --- | ---: | ---: |
| / | 1030 | 527 |
| /assets/app.js | 425195 | 143462 |
| /assets/app.css | 20283 | 5172 |
| /assets/third-party.txt | 5755 | 1433 |

Combined payload: **150,594 bytes gzip**, below 160 KiB. Input hashes normalize text line endings; generated gzip metadata is normalized. The build includes no source maps, external fonts or CDN assets. Browser code executes on the phone/computer; ESP32 serves compressed flash bytes.

`tools/check_docs.py` passed: **25 Markdown files, 221 relative links/anchors, 19 SVGs, 31 manual-alternative wires**, exact pin/version/license checks and generated provenance. Generated assets come from `tools/generate_docs.py`. Confirmed installation wiring remains module TXD → GPIO18, RXD ← GPIO17; GPIO16 unused on the automatic module; Fort terminals 58/A and 59/B. No register/scaling or terminal assignments changed in this audit.

## Package and remote checks

`tools/package.py` compares the copied app with the current build, checks bootloader/partition/boot_app0/app offsets in the merged factory image and verifies every archived manifest SHA256 plus ZIP CRC. The package includes sources, docs, notices, tooling and the original manual; excludes caches/dependency directories. Flash offsets remain 0x0 / 0x8000 / 0xE000 / 0x10000 as documented in [FLASHING](FLASHING.md).

Remote CI must be read for the actual pushed commit. Local checks do not imply a passing GitHub Actions run.

## Remaining physical checks

Fort register acceptance/display agreement, RS485 turnaround/noise/echo behavior, I2C devices and RTC battery, OLED orientation, buttons/buzzer, GPIO48 LED output, power-cut persistence/recovery, HTTP parser limits and multiple AP clients remain unverified on the installed hardware. HTTP response writes are bounded; the upstream request parser is not replaced or claimed to withstand arbitrary hostile request streams. The four-pin RS485 module exposes no presence/power-good signal; a silent path cannot prove which physical component is absent or defective.

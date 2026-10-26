# Agent instructions — Midea Fort Energy Station

## Purpose and confirmed hardware

Maintain firmware and documentation for an ESP32-S3 N16R8 that reads a Fort power meter over Modbus RTU and serves a local dashboard. The user's Fort RS485 port is unused: the ESP32 is the only master. The purchased RS485 module is confirmed compatible with ESP32 3.3 V TTL. Treat these as established facts; do not repeatedly question them or invent a PLC/BMS connection.

The root Fort PDF is the primary meter reference. It is a scanned document. Verify the relevant page image before changing terminal assignments, registers, scaling, or protocol details. The documented meter communication terminals are 58/A and 59/B. Terminal 70/COM belongs to the switch inputs and is not an RS485 ground connection.

The supplied module photo shows a four-pin automatic-direction VCC/TXD/RXD/GND interface; GPIO16 is unconnected on that module. The user reports crossed module TXD -> GPIO18 and RXD <- GPIO17. Keep that confirmed installation wiring as the diagnostic baseline; do not default to rewiring as the diagnosis without evidence. Keep the manual RO/DI/DE/RE schematic explicitly identified as an alternative.

The only supported build/release target is `esp32s3` for the physical N16R8 board. Keep experimental tooling and generated caches out of the deployable source/package.

## Sources of truth

- `firmware/platformio.ini`: target, platform, pinned library versions, flash/PSRAM, core affinity.
- `firmware/include/board.h`: GPIO, OLED address, AP credentials, timezone offset.
- `firmware/include/version.h`: firmware release version.
- `web/Application.jsx`, `web/views/`, `web/device.js`, `web/i18n.js`: browser views, transport and language catalog.
- `web/bundle.json` and `tools/build-web.cjs`: generated asset provenance, sizes and reproducible build.
- `firmware/include/energy.h`: interval schedule, continuity and quality logic, CRC/frame validation.
- `firmware/include/debounce.h`: button event timing.
- `firmware/include/trace.h`: fixed-size passive UART capture and diagnostic classification; never bypass CRC/frame validation or silently strip echo bytes in the main firmware.
- `examples/FortSerialDebug/`: standalone Arduino diagnostic; keep it read-only and independent of external libraries.
- `firmware/include/led.h`: RGB modes, brightness, rollover-safe animation and versioned CRC-protected NVS encoding.
- `firmware/src/main.cpp`: hardware drivers, RTOS ownership, persistence and HTTP API.
- `docs/DESIGN.md`: manual page references and documented limitations.

Keep the README, wiring SVG, netlist, diagrams, tests and release package consistent with these files. Name the root guide `AGENTS.md` and root overview `README.md` using this exact case for GitHub/Linux portability.

## Firmware invariants

1. Read the meter with Modbus function 03 only. Do not add register writes, automatic meter reconfiguration or resets without an explicit product requirement.
2. Reject bad CRC, wrong ID/function/length and exception frames. CRC integrity is never bypassed to match a typo in the manual.
3. Use the secondary integer energy counter and apply CT/PT once. Keep cumulative calculations in `double`; never treat missing data as zero.
4. Morning windows are 06:00–09:00 in six half hours. Evening windows are 16:30–18:00 in three half hours. All labels and day boundaries are WIB (UTC+7); DS3231 stores UTC.
5. Missing/invalid RTC, reboots, counter resets, changed CT/PT and excessive sample gaps must not yield a complete interval. Preserve completed intervals.
6. Core 1 exclusively owns UART, Preferences and tracker mutation. Core 0 handles HTTP/UI. Exchange copies through RTOS queues; lock I2C and filesystem access. Keep waits bounded and leave time for idle/Wi-Fi tasks.
7. Do not automatically format a failed filesystem. Retain the documented physical confirmation procedure and preserve recoverable backup files on failed operations.
8. Core 0 exclusively owns RGB RMT channel 0. Keep transmission asynchronous, skip busy frames and write LED Preferences only on core 1. GPIO48 is the confirmed onboard LED pin for this installation.
9. Boot without waiting for USB Serial. An absent OLED must not prevent AP/measurement startup.

## Documentation and visuals

- English is the primary language. Keep README.md and technical guides in English with linked Indonesian counterparts (README.id.md and docs/id/). The web UI must switch between English and Indonesian, defaulting to English. Keep steps and pin-to-pin instructions concrete.
- Browser sources live in web/. Use real vendored React Bits components with their upstream license and provenance. Keep third-party terms separate from original GPL-3.0-only code; do not relabel dependencies as GPL.
- Build browser assets with npm run build:web. Commit the generated gzip header and web/bundle.json; plain PlatformIO builds verify hashes without requiring Node. Keep the gzip payload below 160 KiB. Test the generated bytes, not an alternate mock UI.
- Use repository-local SVGs for wiring, icons/badges, topology and OLED illustrations; no external image/CDN dependency for the README or embedded dashboard.
- Generate vector assets with `tools/generate_docs.py`; edit its data/layout rather than hand-diverging generated SVGs.
- Label UI screenshots and OLED illustrations as simulated/representative. Never present them as hardware acceptance evidence.
- The full wiring diagram must show power/ground, shared I2C, RO/DI direction, tied DE-/RE, Fort A/B terminals, all three buttons and the buzzer driver. Explain repeated net labels and that logical port positions are not a board photograph.
- Keep uncertain physical package pin order explicit: a transistor's B/C/E locations depend on its part/package. Do not guess them.
- Version badges must match the pinned build configuration. The user selected GNU GPL version 3 only (`GPL-3.0-only`); preserve LICENSE/NOTICE and do not relicense third-party dependencies or the Fort manual. Do not invent a GitHub owner/repository URL, passing CI badge or hardware test result.
- Markdown links used in repository files must be relative and case-correct; chat links use absolute paths.

## Verification

Run checks appropriate to the change; do not broaden/repeat them after they pass without a new reason.

- Native logic on Windows: `./tools/test.ps1` (g++ required).
- Native logic on Linux/macOS: `g++ -std=c++11 -Wall -Wextra -Werror -Ifirmware/include firmware/test/test_energy.cpp -o /tmp/fort-test && /tmp/fort-test`.
- Documentation, assets, versions and relative links: `python tools/check_docs.py`.
- Firmware: from `firmware`, `pio run`. Use the installed PlatformIO Python module if `pio` is absent from PATH.
- Standalone sketch: `pio run -d examples/FortSerialDebug` (same ESP32-S3 N16R8 target; no external libraries).
- Browser/UI and SVG visual checks: `node tools/preview-test.cjs [playwright-module-path] [browser-channel]`. The script uses a localhost mock, not real hardware.

After changing firmware, rebuild before copying binaries. Update `docs/VALIDATION.md` with actual outcomes and limitations, rebuild the release package, verify its manifest and ZIP integrity. Include all documentation/assets/tools in the ZIP; never package `.pio`, temporary files or dependency directories. Follow `output/firmware/FLASHING.md` for image offsets.

## GitHub readiness

Review `.gitignore`, relative image links, reproducible dependency versions, CI and the license status. Preparing a repository for GitHub does not itself request publishing, creating a remote, or pushing. Preserve the source PDF and user files. Do not make generated ZIPs or binaries the only copy of source code.

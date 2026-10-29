# Validation record / 30 October 2026

**English** · [Bahasa Indonesia](id/VALIDATION.md)

Firmware, FortMeter and FortSerialDebug are **3.0.0**. This record covers software checks for the integrated release, which follows the user's successful ModbusMaster reference with GPIO16 driving tied DE and /RE. The user reported that reference working on the Fort, while the integrated release still needs the separate hardware checks in [COMMISSIONING](COMMISSIONING.md). Earlier release records are summarized in [CHANGELOG](../CHANGELOG.md).


## Enterprise client and runtime migration in 3.0.0

All three native suites, the full generated-byte browser regression, the separate discovery suite and the Enterprise suite passed. The Enterprise browser cases cover advertised-security selection, the username/password form, default outer identity, optional server DNS, missing-CA rejection before submission, CA/credential reuse after reload, trust-target changes, AP-only provisioning, clock errors, an authentication-rejection pause, English/Indonesian copy and mobile layout. Network codec tests cover the 779-byte CRC record, legacy 104/108-byte migration, field bounds, optional DNS, stale-secret-tail removal and the Enterprise-only retry rejection policy.

The actual custom SDK was built with certificate date checking enabled, certificate key-usage checks retained, Arduino and its event task on core 0, 16 MB flash and OPI PSRAM. The shared SDK helper reuses a cache only when its configuration and SDK library hashes match. The application still compiles from the original workspace. The configuration header was renamed to network_settings.h to avoid Arduino 3's Network.h collision on Windows.

The IDF 5 RGB adapter compiled through the real RMT TX/copy-encoder API. Inspection of the final ELF found the new driver and RgbDriver methods, with no legacy RMT conflict constructor linked. Its persistent symbol buffer and zero-time completion polling preserve asynchronous writes on core 0. Physical waveform, color order and LED brightness still require board acceptance.

The public Windows profile exporter was exercised without exporting passwords or private keys. It correctly reported that the referenced CA was unavailable in the accessible certificate stores. This is a missing-trust-material result, not an authentication attempt. No corporate CA, employee account or successful corporate association is bundled or claimed. See [Enterprise Wi-Fi](ENTERPRISE_WIFI.md) for the source of public CA material and the remaining RADIUS tests.


The first Linux CI run exposed a bootstrap dependency mismatch: installing upstream platformio alone allowed the platform to install a newer pioarduino distribution into its managed child environment. tools/setup-core.py now installs only pioarduino 6.1.19 into that environment, and the build guard checks both its version and interpreter. Both local targets passed again under the pinned distribution. The shared SDK cache is also used on Linux, allowing the standalone target to reuse the same verified SDK instead of replacing it.

## Discovery and named devices in 2.6.0

The three native suites passed with the actual shared ModbusMaster source and standalone INO. Added cases cover continuing a multi-address plan after silence, repeated data and exception evidence, editable plan bounds, two-stop-bit UART reconfiguration, cancellation, four-finding capacity, typed ratio layouts and configuration migration without changing the existing Fort or Acrel register definitions. Registry tests cover explicit CRC records, failed writes, recovery from the older bank and ID reservations that prevent reuse after rollback. Checkpoint schema 3 retains the saved-device ID, while schema 1 and 2 records remain readable.

The full generated-byte Microsoft Edge regression passed, including all 27 vector assets and the existing 320–2560 px layouts. A separate discovery suite checks plan validation, finding selection, named-save requests, reload, supported-profile activation, the active-record deletion guard, JSON download, malformed-response retention, priority Stop, English/Indonesian switching and closed-panel polling. Representative discovery controls and saved-device cards were visually inspected. These tests use simulated APIs and do not establish flash endurance, physical power-cut recovery or radio/UART timing.

The registry uses four 700-byte entries in a 2,818-byte explicit CRC bank, with two alternating banks and a separate six-byte CRC-protected ID reservation. Each finding retains both attempts' observed TX/RX, response status and duration. The duration includes driver service and spacing, so it is not a wire-latency measurement. A confirmed exception establishes a valid protocol response, while a matching register shape provides only a mapping hint. Manufacturer identity and energy scaling still require the operator's manual/LCD comparison, as explained in [Meter discovery](METER_DISCOVERY.md).

The scanner, registry codecs, persistence adapter and HTTP representation are separate modules. Core 1 continues to own UART, Preferences and measurement state, and core 0 reads copied snapshots and queues commands. Unknown findings remain available for naming and report export without becoming an invented production energy map. The hardware acceptance items below still apply to this release.

## Meter profiles in 2.5.0

Both physical-target builds and all three native suites passed after the profile changes. The main image uses 85,144 bytes of static RAM and 1,427,045 program bytes, while the standalone image uses 30,284 and 337,785 bytes. The generated browser payload is 139,155 bytes gzip, below 163,840 bytes.

New actual-library cases verify the Acrel one-register `0x0064` probe, the two-register `0x000A` counter, `0x008D` PT/CT decoding, 0.01 kWh resolution, signed power and switching back to the Fort map. Native policies verify the 240-candidate Acrel IDs 1–16 matrix, exclusion of 38400 baud for V1.3, preservation of the profile in candidates, v2 settings migration, schema-1 Fort capture decoding and schema-2 Acrel capture round trips. Unknown stored profile IDs are rejected, and optional differences compare profile identity. The actual standalone INO executes both profile commands and all eight recipes, whose commands explicitly select Fort.

The full Microsoft Edge suite passed on the shipped gzip arrays, including model save/reload, Acrel register width and terminal labels, Fort restoration and the existing 320–2560 px regressions. Frame grouping now rejects a profile change within a transaction. All 26 SVGs passed rendering and text bounds, including the new complete Acrel wiring. The representative model-selector and checkpoint previews were visually inspected.

The local Acrel V1.3 PDF pages 9, 15–19 were inspected as rendered images. The active-energy unit/CTPT interpretation is cross-checked against a later manufacturer document and its revision difference is explicitly retained in [Meter profiles](METER_PROFILES.md). The original V1.3 does not explicitly state two-word order, and later MID power scales differ. This release is not a physical ADL400 acceptance result. LCD agreement, model revision, installed ratios, checkpoint persistence under power loss and real bus timing remain field checks.

## Compilation

Both physical-target application builds passed with pinned pioarduino **55.03.311**, Arduino-ESP32 **3.3.11**, ESP-IDF **5.5.5** and PlatformIO Core **6.1.19**. The application build logs contain no compiler warnings or errors. The first custom SDK bootstrap includes upstream component diagnostics and is not represented as a warning-free SDK build.

| Build | Static RAM / 327,680 bytes | Program / 6,553,600 bytes |
| --- | ---: | ---: |
| Main `esp32s3` environment | 133,060 | 1,841,719 |
| Standalone FortSerialDebug | 36,840 | 367,776 |

Static RAM does not include every runtime heap, queue, stack or peripheral allocation. Core 0's loop stack is 16 KiB and the acquisition stack is 12 KiB, with actual headroom exposed by the process monitor for on-device inspection. The standalone sketch now uses the supplied FortMeter library, which contains the pinned ModbusMaster source. Both compile through PlatformIO, but the Arduino IDE GUI and other ESP32 core versions were not exercised.

## Native regression tests

`tools/test.ps1` passed all three C++11 suites with `-Wall -Wextra -Werror`. The diagnostic tests compile the actual vendored ModbusMaster implementation with the shared adapter, and the standalone suite includes the real INO. UART traffic and time are injected by a host stub rather than replacing ModbusMaster with a mock reader.

The transport checks cover the exact `01 03 00 58 00 02 45 D8` request, manual direction callback order, byte-wise replies completed by their expected length, wrong CRC/ID/function/count, short and oversized input, exact echoes and CRC-verified exceptions. They also exercise failed UART initialization and retry, partial writes, cancellation before TX and during reception, deadline-tail rejection, automatic mode without direction writes, queued console input and deferred Serial `resume` handling. A reply arriving after the response deadline cannot become accepted data merely because its bytes later pass CRC.

Network tests cover mode/SSID/key validation, explicit open-network handling, 64-digit WPA keys, truncated/corrupted records, client retry and AP grace, lost-link recovery, combined mode, timer rollover and overlapping LAN subnets including a more-specific /28. These exercise the same NetworkPolicy helpers used by the firmware. The rollover test explicitly narrows its constructed timestamp to uint32_t so the injected case is identical on Windows and 64-bit Linux. The actual Wi-Fi driver, DHCP exchange and NVS hardware failures are compiled/reviewed integration paths rather than host-executed radio tests. Capture comparison regressions also reject mismatched direction, direction pin and settle/gap timing.

Runtime configuration, raw plans, bounded scans, weak-pull observations, paired-capture guards, all eight field recipes and read-only FC03 output are covered. The scan enumerator checks the 4,446-candidate matrix, current-format ranges, preferred settings, cancellation and timer rollover. Settings tests check CRC-valid v1 migration to manual mode and retention of an explicitly selected v2 automatic mode.

The existing energy and device suites also pass for the nine scheduled windows, interpolation and continuity gaps, missing/invalid time, reboot and counter discontinuity, CT/PT changes, completed-record preservation, backward-day protection, queued clock correction, command mode/revision/age guards, archive fault recovery, LED persistence, buttons and trace/event bounds. Bounded HTTP writer tests inject partial progress, no progress, fatal errors and timer rollover without extending the total deadline.

These tests establish software behavior under the injected cases. They do not measure physical UART timing, transceiver health, line noise, register acceptance or energy agreement with the installed meter. FreeRTOS ownership is reviewed and compiled, while pure command-policy tests do not execute concurrent hardware tasks. RTU carries no transaction ID or register address in its response, which limits attribution of delayed replies.

## Browser and vector checks

`tools/preview-test.cjs` passed with Playwright and installed Microsoft Edge, using the **generated gzip arrays** in `firmware/include/web_ui.h` with simulated APIs. It exercises the shipped browser bytes rather than a substitute page.

Coverage includes English/Indonesian switching and persistence, blocked storage, all interval rows, direct register values and freshness, manual/automatic direction save and reload, scan/probe/cancel/apply, clock synchronization, LED controls, CSV and raw capture export. Command controls wait for the matching execution result after HTTP 202, and priority Stop remains available while an ordinary action is pending. Maintenance blocks clock changes and Run waits for the initial diagnostic state.

The added browser cases cover AP/client/combined mode submission and reload, password reuse without returning stored keys, explicit open networks, automatic SSID scanning/selection, custom AP IPv4 validation, recovery AP presentation, malformed network state and preservation of an operator draft after a failed refresh. Structured diagnostic reads and trial settings reach the console endpoint, session settings update from the owner response, and invalid diagnostic payloads disable ordinary actions while leaving Stop available.

Malformed status, archive, process, trace and UART-event payloads retain the last usable data with a stale/unavailable state. Diagnostics, process metrics and raw trace polling stop when their panel closes. UART animation requires new events, honors reduced motion and does not turn idle time or pre-reboot history into live packets. Its readable replay uses presentation timing, while the unscaled device timestamps remain available.

Layouts from 320 to 2560 px passed without document overflow, JavaScript page errors or external asset requests. The test verifies upstream React Bits blob hashes and renders all **28 SVGs**, checking text bounds against their viewBox. Representative images contain simulated data: [dashboard](assets/ui-desktop.png), [phone](assets/ui-mobile.png), [scanner](assets/ui-scan-desktop.png), [communication](assets/ui-flow-desktop.png), [process monitor](assets/ui-process-desktop.png) and [diagnostic console](assets/ui-diagnostic-mobile.png).

## Asset and source provenance

The browser build uses Node **24.22.1**, esbuild **0.28.2**, React/React DOM **19.3.1** with React Bits SpotlightCard, WakeSlider, Counter and StarBorder, plus Motion **14.7.1**. GSAP **3.15.0** remains pinned for the preserved FadeContent reference but is not imported into the current runtime bundle. The generated header and manifest agree, and the Python hash guard passed inside the successful firmware build. A normal PlatformIO build needs no Node installation.

| Endpoint | Uncompressed bytes | Gzip bytes |
| --- | ---: | ---: |
| `/` | 1,031 | 533 |
| `/assets/app.js` | 434,532 | 137,475 |
| `/assets/app.css` | 40,035 | 9,674 |
| `/assets/third-party.txt` | 10,218 | 1,539 |

The combined payload is **149,221 bytes gzip**, below 160 KiB, without source maps, remote fonts or CDN assets. Text input hashes normalize line endings and the gzip metadata is reproducible. ModbusMaster **2.0.1+fort.1** source hashes and its documented service-hook patch pass the separate provenance checker, with upstream license headers preserved.

The documentation checker validates 37 Markdown guides and their relative links/anchors and the complete web/firmware API route map in both languages. It also checks pinned versions, GPIO assignments, generated assets and all **31 installed manual-wiring connections**. The installed wiring is TX17 to DI, RX18 from RO and DIR16 to tied DE and /RE, with Fort terminals 58/A and 59/B. Relevant scanned manual page images were inspected for the current, voltage, power, frequency, ratio and energy definitions. The energy-length table inconsistency and printed example CRC error remain documented in [DESIGN](DESIGN.md#fort-register-reference).

## Packaging and remaining hardware work

The packager verifies the copied app against the current build, compares every component at its merged factory offset, checks the ZIP CRC and verifies each archived manifest SHA256. The release includes source, documentation, tooling, notices, the original Fort and Acrel manuals and a separate Arduino FortMeter library ZIP, while excluding build caches and installed dependencies. Image offsets remain 0x0 / 0x8000 / 0xE000 / 0x10000 as described in [FLASHING](FLASHING.md). Remote CI must be read for the actual pushed commit, since local checks alone do not establish its result.

The physical AP/client/dual transitions, router-loss recovery, DHCP/subnet changes and the integrated Fort register/display comparison, RS485 turnaround and noise behavior, OLED/RTC and battery, buttons/buzzer, GPIO48 output, power-cut recovery and multiple AP clients still require physical acceptance. HTTP response writes are bounded, but the upstream request parser has not been replaced or certified against arbitrary hostile traffic. The transceiver exposes no presence or power-good input, so UART silence or local RX activity cannot identify a disconnected or defective component with certainty.

## Added display and network coverage in 2.2

Native tests cover the OLED menu transitions and editors, invalid private IPv4 input, reading the legacy 104-byte network record and writing the 108-byte custom-IP record. Idle tests cover the default timeout boundary, a consumed wake press held through a long-press interval, release guarding, disable and timer rollover. They link the actual generated frame pack, exercise its bounded decoder and reject truncated/oversized output. The generator verifies all 772 decompressed frames against their combined raw SHA256, preserving 790,528 original bytes in a 266,739-byte lossless pack. Import check also reproduces the pack from the supplied reference sequence.

Generated-byte browser tests cover automatic and repeated Wi-Fi scans, coalescing repeated SSIDs to their strongest supported entry, choosing a network, preserving a custom-IP draft, display timeout save/reload/disable, malformed display data and a clock that keeps advancing across identical status samples without posting RTC changes. The interface uses blue/violet glass surfaces and reduced-transparency/contrast fallbacks. Representative controls are shown in [OLED](OLED.md), alongside logical frame samples rather than physical display photographs.

The nominal 42 ms animation target, I2C occupancy, physical wake latency, long-hold character repeat, actual AP discovery/association and sustained AP clients during playback remain hardware acceptance items. A passing host decoder or browser mock does not establish those physical timings. UART event retry reasons describe HTTP availability, not electrical evidence about the Fort.

## Wi-Fi and communication correction in 2.2.1

The final main build uses 83,088 bytes of static RAM and 1,378,569 bytes of program space. The standalone build remains at 30,284 bytes of static RAM and 336,577 bytes of program space. Both compilation outputs completed without warnings or errors. All three native suites passed after the final diagnostic version update, including the scanner's preparation, retry exhaustion, radio/overall deadline and unsigned rollover cases.

The generated-byte browser suite passed the additional lost-acknowledgement recovery, retained SSID list after radio failure, temporary HTTP failure, 3.1-second response delay and repeated-scan cases. UART checks cover direction labels, vector symbols, retained raw frames, CRC mismatch, timeout with no fabricated RX motion and reboot history. The actual request module also passed a Node test that keeps response bodies open to verify serialization, removes aborted queued GETs, preempts a running GET for Stop and preserves an accepted POST when Resume is queued. The full existing dashboard suite and 23 vector checks passed with installed Microsoft Edge at 320–2560 px.

The radio adapter initializes the pinned IDF scan structure and consumes Arduino's scan-done callback through an RTOS mailbox. The actual radio, AP service during off-channel scanning, DHCP association and empty-socket behavior were compiled and reviewed, with their policies or browser-facing behavior tested on the host. No physical radio acceptance was performed for this release, so these results do not establish that the particular installed board/router combination has passed scanning.

## Boot artwork in 2.2.2

The integrated main build passed with 83,088 bytes of static RAM and 1,380,281 bytes of program space, adding 1712 bytes of program space without changing static RAM. The three native suites passed with the actual generated OLED source linked into the host build. The generator verifies the supplied XBM's 1350 bytes against SHA256 `f4e713c873ba49341b8c764f93f0ef0016f6768e9e6637c46c3a0c1b7c2887cc`, and its LSB-first layout was checked against the installed Adafruit GFX `drawXBitmap()` implementation. The representative logical render was visually inspected, and all 24 SVGs passed the browser text-bound/render check.

Every shipped browser asset has the same size and SHA256 as 2.2.1. Only release/input provenance changed, so the previous full dashboard regression remains applicable, while the new vector and request-module checks passed locally. Boot ordering was reviewed in the actual setup path: OLED rotation and first-frame transfer precede RTOS object allocation, the 3000 ms hold yields with RGB service, then the original 1500 ms loading/sound routine executes. Missing OLED skips the artwork hold. Physical OLED timing/orientation and startup-to-AP latency still need on-device acceptance.

## Readable browser motion in 2.3.0

The integrated build passed at 83,088 bytes of static RAM and 1,397,497 bytes of program space. The main firmware's acquisition, Modbus transport, GPIO configuration, RTC ownership and boot logo behavior are retained. Native tests passed all three suites, including the actual standalone INO and pinned ModbusMaster source. The offline browser bundle is 134,158 bytes gzip, below the 163,840-byte limit.

The supplied WakeSlider, Counter and StarBorder JavaScript/CSS pairs were compared with upstream commit e12c22192ac7b4b452c5e2feb9b586d74765d696 before being copied unchanged. The upstream Git blob hashes pass the generated-byte preview guard, Motion is pinned at 14.7.1 and four Motion-package MIT notices are served offline. Source hashes and generated output agree.

The actual frame assembler passed valid/CRC/timeout grouping, incomplete sequences, changed direction pins, orphan outcomes, explicit reset, local UART failure and uint32 sequence rollover. The browser suite passed real-payload rendering at 320–2560 px, rolling clock units without RTC POSTs, actual WakeSlider/StarBorder mounts, missing underline/taps, alternating TX/RX and no returning frame for a timeout. A packet position sampled after 320 ms was farther along its track while still short of the far endpoint, replacing the former sub-100 ms flash. Navigation's shared highlight is clipped during viewport changes so its transition cannot widen the document.

The existing scan, Wi-Fi, diagnostics, stop/resume, records, LED, process, stale-data, English/Indonesian and reduced-motion regressions passed. A 5.23-second representative MP4 records 127 changing frames from the shipped UI with simulated API data, encoded at 30 fps. The video is documentation and is not served by the ESP32. These checks validate browser behavior and compiled firmware rather than physical timing or field acceptance on the installed phone, OLED and meter.

## Arrival easing and fade in 2.3.1

Both builds and all three native suites passed. The main image uses 83,088 bytes of static RAM and 1,397,769 bytes of program space, adding 272 program bytes over 2.3.0. The standalone image remains at 30,284 and 336,577 bytes respectively. The generated browser payload is 134,421 bytes gzip, including the easing and resting-track changes.

The complete generated-byte browser suite passed with Microsoft Edge at 320–2560 px, including scanner recovery, diagnostics, Stop/Resume, clock, records, languages and reduced motion. New animation checks sample rendering frames inside the browser for both TX and RX. They require a fade from opacity above 0.9 to below 0.1, intermediate opacity values, no increase during the fade, less than one pixel of position movement after arrival and matching packet/wave opacity. The pure easing checks also verify bounded monotonic travel, zero endpoint velocity and continuity between movement and fade.

The refreshed representative MP4 contains 90 changing browser frames over 3.6 seconds, encoded at 30 fps, and ends after playback reaches idle. It uses simulated API data and is documentation rather than physical timing evidence. The playback change does not alter the acquisition path, UART timestamp capture, RTC writes or Modbus acceptance rules.

## Dense points and the primary energy counter in 2.3.2

Both ESP32-S3 builds and all three native suites passed. Main static RAM remains 83,088 bytes, with 1,398,057 bytes of program space. The standalone image remains at 30,284 and 336,577 bytes respectively. The generated web payload is 134,707 bytes gzip and the checked input hashes match the firmware header.

The full generated-byte browser suite passed in Microsoft Edge at 320–2560 px. It verifies point counts between 64 and 256, fixed 2 px point width and at most 3 px height, along with the existing movement/fade and no-RX-on-timeout checks. Travel timing now comes from samples taken inside requestAnimationFrame, avoiding the delayed Playwright calls that made the previous CI position assertion fail. The measured replay must cross the track, paint intermediate positions and remain visible in motion for at least 900 ms.

The supplied Counter JavaScript and CSS match the unchanged vendored source. Main-energy cases exercise the carry from 999.999 to 1,000.001, decreasing readings, a legitimate zero, missing energy, localized English/Indonesian labels and reduced motion. Initial/missing readings do not spin from zero. The representative counter recording was visually checked at intermediate digit positions and after settling. It contains 74 changing browser frames over 2.93 seconds, while the updated point replay contains 89 changing frames over 3.57 seconds. Both videos use simulated API data and are excluded from the embedded payload. Integrated physical-board acceptance remains separate from these software checks.

## Larger moving wave in 2.3.3

Both firmware builds, all three native suites and the generated-byte browser suite passed. Main static RAM remains 83,088 bytes and program space remains 1,398,057 bytes. The standalone image remains at 30,284 bytes of static RAM and 336,577 bytes of program space. The generated web payload is still 134,707 bytes gzip.

The waveform keeps its fixed 2 px width and 64–256 responsive points, with a 36 px moving envelope and broader crest. Browser frame sampling requires a peak of at least 24 px, bounded by 36 px, in both TX and RX replay. The existing eased arrival, gradual fade, timeout without RX, reduced-motion and 320–2560 px layout checks passed. Desktop and phone previews were visually inspected for clearance from the packet badge and endpoint text. The refreshed simulated replay records 90 changing browser frames over 3.6 seconds. UART capture, communication settings, meter acceptance and energy calculations are unchanged, and these browser checks do not establish physical communication timing.

## Primary cumulative checkpoints in 2.4.0

The main and standalone builds passed at 85,144 / 1,420,553 bytes and 30,284 / 336,577 bytes of static RAM/program space respectively. All three native suites passed with the new capture engine linked into the actual firmware logic tests. Coverage includes all ten revised schedule times, pre-target rejection, the five-second deadline, immutable recorded/missed rows, whole-second RTC quantization, uninterrupted adjacent differences, reboot generations, CRC byte corruption/truncation and refusal to overwrite an unarchived prior day. Existing legacy interval, transport and diagnostic tests remain passing.

The complete generated-byte browser suite passed in Microsoft Edge, including cumulative rows, actual read times, missing checkpoints, morning/evening selection, optional differences, historical-day fetch/download, malformed live payload retention and rejection of an invalid archive without showing today's data under its date. Existing diagnostics, network, counters, process/flow, languages and reduced-motion cases passed at 320–2560 px. The 138,037-byte gzip payload is below the 163,840-byte limit, and all 25 SVGs passed rendering/text bounds.

Persistence and scheduling compile through the real Preferences/LittleFS, RTC and Modbus adapters. Native tests execute the pure codec/admission/retention policy and the existing archive fault helper, while browser APIs are simulated. Physical capture latency, flash power-cut recovery, storage exhaustion, NVS failure, radio loss during a checkpoint and agreement with the installed Fort still require the documented commissioning checks. No physical acceptance result is claimed.

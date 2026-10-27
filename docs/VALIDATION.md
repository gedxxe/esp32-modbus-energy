# Validation record / 27 October 2026

**English** · [Bahasa Indonesia](id/VALIDATION.md)

Firmware, FortMeter and FortSerialDebug are **2.2.1**. This record covers software checks for the integrated release, which follows the user's successful ModbusMaster reference with GPIO16 driving tied DE and /RE. The user reported that reference working on the Fort, while the integrated release still needs the separate hardware checks in [COMMISSIONING](COMMISSIONING.md). Earlier release records are summarized in [CHANGELOG](../CHANGELOG.md).

## Compilation

Both builds passed with the pinned espressif32 **6.10.0** and Arduino ESP32 **2.0.17** toolchain for the physical ESP32-S3 N16R8, with 16 MB flash and OPI PSRAM. The final compiler output contained no warnings or errors.

| Build | Static RAM / 327,680 bytes | Program / 6,553,600 bytes |
| --- | ---: | ---: |
| Main `esp32s3` environment | 83,088 | 1,378,569 |
| Standalone FortSerialDebug | 30,284 | 336,577 |

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

Malformed status, archive, process, trace and UART-event payloads retain the last usable data with a stale/unavailable state. Diagnostics, process metrics and raw trace polling stop when their panel closes. UART animation requires new events, honors reduced motion and does not turn idle time or pre-reboot history into live packets. Its timestamp replay includes network delay and is not a wire analyzer.

Layouts from 320 to 2560 px passed without document overflow, JavaScript page errors or external asset requests. The test verifies upstream React Bits blob hashes and renders all **23 SVGs**, checking text bounds against their viewBox. Representative images contain simulated data: [dashboard](assets/ui-desktop.png), [phone](assets/ui-mobile.png), [scanner](assets/ui-scan-desktop.png), [communication](assets/ui-flow-desktop.png), [process monitor](assets/ui-process-desktop.png) and [diagnostic console](assets/ui-diagnostic-mobile.png).

## Asset and source provenance

The browser build uses Node **24.22.1**, esbuild **0.28.2**, React/React DOM **19.3.1** with React Bits SpotlightCard. GSAP **3.15.0** remains pinned for the preserved FadeContent reference but is not imported into the current runtime bundle. The generated header and manifest agree, and the Python hash guard passed inside the successful firmware build. A normal PlatformIO build needs no Node installation.

| Endpoint | Uncompressed bytes | Gzip bytes |
| --- | ---: | ---: |
| `/` | 1,030 | 527 |
| `/assets/app.js` | 343,813 | 107,676 |
| `/assets/app.css` | 28,797 | 7,297 |
| `/assets/third-party.txt` | 5,755 | 1,433 |

The combined payload is **116,933 bytes gzip**, below 160 KiB, without source maps, remote fonts or CDN assets. Text input hashes normalize line endings and the gzip metadata is reproducible. ModbusMaster **2.0.1+fort.1** source hashes and its documented service-hook patch pass the separate provenance checker, with upstream license headers preserved.

The documentation checker validates 29 Markdown guides and 197 relative links/anchors and the complete web/firmware API route map in both languages. It also checks pinned versions, GPIO assignments, generated assets and all **31 installed manual-wiring connections**. The installed wiring is TX17 to DI, RX18 from RO and DIR16 to tied DE and /RE, with Fort terminals 58/A and 59/B. Relevant scanned manual page images were inspected for the current, voltage, power, frequency, ratio and energy definitions. The energy-length table inconsistency and printed example CRC error remain documented in [DESIGN](DESIGN.md#fort-register-reference).

## Packaging and remaining hardware work

The packager verifies the copied app against the current build, compares every component at its merged factory offset, checks the ZIP CRC and verifies each archived manifest SHA256. The release includes source, documentation, tooling, notices, the original Fort manual and a separate Arduino FortMeter library ZIP, while excluding build caches and installed dependencies. Image offsets remain 0x0 / 0x8000 / 0xE000 / 0x10000 as described in [FLASHING](FLASHING.md). Remote CI must be read for the actual pushed commit, since local checks alone do not establish its result.

The physical AP/client/dual transitions, router-loss recovery, DHCP/subnet changes and the integrated Fort register/display comparison, RS485 turnaround and noise behavior, OLED/RTC and battery, buttons/buzzer, GPIO48 output, power-cut recovery and multiple AP clients still require physical acceptance. HTTP response writes are bounded, but the upstream request parser has not been replaced or certified against arbitrary hostile traffic. The transceiver exposes no presence or power-good input, so UART silence or local RX activity cannot identify a disconnected or defective component with certainty.

## Added display and network coverage in 2.2

Native tests cover the OLED menu transitions and editors, invalid private IPv4 input, reading the legacy 104-byte network record and writing the 108-byte custom-IP record. Idle tests cover the default timeout boundary, a consumed wake press held through a long-press interval, release guarding, disable and timer rollover. They link the actual generated frame pack, exercise its bounded decoder and reject truncated/oversized output. The generator verifies all 772 decompressed frames against their combined raw SHA256, preserving 790,528 original bytes in a 266,739-byte lossless pack. Import check also reproduces the pack from the supplied reference sequence.

Generated-byte browser tests cover automatic and repeated Wi-Fi scans, coalescing repeated SSIDs to their strongest supported entry, choosing a network, preserving a custom-IP draft, display timeout save/reload/disable, malformed display data and a clock that keeps advancing across identical status samples without posting RTC changes. The interface uses blue/violet glass surfaces and reduced-transparency/contrast fallbacks. Representative controls are shown in [OLED](OLED.md), alongside logical frame samples rather than physical display photographs.

The nominal 42 ms animation target, I2C occupancy, physical wake latency, long-hold character repeat, actual AP discovery/association and sustained AP clients during playback remain hardware acceptance items. A passing host decoder or browser mock does not establish those physical timings. UART event retry reasons describe HTTP availability, not electrical evidence about the Fort.

## Wi-Fi and communication correction in 2.2.1

The final main build uses 83,088 bytes of static RAM and 1,378,569 bytes of program space. The standalone build remains at 30,284 bytes of static RAM and 336,577 bytes of program space. Both compilation outputs completed without warnings or errors. All three native suites passed after the final diagnostic version update, including the scanner's preparation, retry exhaustion, radio/overall deadline and unsigned rollover cases.

The generated-byte browser suite passed the additional lost-acknowledgement recovery, retained SSID list after radio failure, temporary HTTP failure, 3.1-second response delay and repeated-scan cases. UART checks cover direction labels, vector symbols, retained raw frames, CRC mismatch, timeout with no fabricated RX motion and reboot history. The actual request module also passed a Node test that keeps response bodies open to verify serialization, removes aborted queued GETs, preempts a running GET for Stop and preserves an accepted POST when Resume is queued. The full existing dashboard suite and 23 vector checks passed with installed Microsoft Edge at 320–2560 px.

The radio adapter initializes the pinned IDF scan structure and consumes Arduino's scan-done callback through an RTOS mailbox. The actual radio, AP service during off-channel scanning, DHCP association and empty-socket behavior were compiled and reviewed, with their policies or browser-facing behavior tested on the host. No physical radio acceptance was performed for this release, so these results do not establish that the particular installed board/router combination has passed scanning.

# Validation record / through 8 November 2026

**English** · [Bahasa Indonesia](id/VALIDATION.md)

Firmware, FortMeter and FortSerialDebug are **3.5.0**. This record covers software checks for the integrated release, which follows the user's successful ModbusMaster reference with GPIO16 driving tied DE and /RE. The user reported that reference working on the Fort, while the integrated release still needs the separate hardware checks in [COMMISSIONING](COMMISSIONING.md). Earlier release records are summarized in [CHANGELOG](../CHANGELOG.md).

## 8 November 2026: supervised direct Wi-Fi pilot hardening (draft PR #21)

The gateway-independent ESP32-S3 HTTPS uplink was re-audited for long-running failure modes. The new draft branch proposes **non-destructive** recovery changes, without altering radio packets, Modbus polling, meter identities, the ten immutable checkpoint times, NVS/LittleFS layout or website credentials. The separate C3 gateway and website are unchanged.

- A 30-day-out-of-range ACK cursor or corrupt historical archive pauses **historical** checkpoint replay, retaining the exact durable cursor and explicit `iot.archiveError`, while fresh authenticated telemetry and presence continue. After a bounded burst of historical uploads, due telemetry/heartbeat gets service.
- The HTTPS worker no longer self-deletes because the filesystem mutex was busy on its initial cursor read. Missing ACK files are valid for new enrollment; existing cursor files that both fail CRC/identity validation now stop historical replay with `archive_cursor_corrupt` rather than silently resetting the acknowledged day to zero. Live telemetry/heartbeat continue while invalid bytes are preserved for inspection. A failed local cursor write retries the same immutable event identity with backoff instead of silently skipping it. Ordinary reading backoff does not starve the 60-second heartbeat; server HTTP 429 rate limiting remains globally respected. Network retry and backfill deadlines now use ESP-IDF 64-bit monotonic uptime, so a stale HTTP rate-limit deadline cannot re-activate after `millis()` wraps during a long offline period.
- Corrupt `/capture.pending` is preserved in a bounded forensic quarantine before a truncating write. Failure of rename or exhausted quarantine names is a reported filesystem failure, **never automatic deletion**. The archive scanner no longer silently stops after 256 entries: it indexes up to 4,096 and 64 distinct candidate days; the 65th distinct day now invalidates completeness regardless of ascending or descending listing order; incomplete scans now block day advancement and filesystem archive writes, preserving separately verified NVS measurements, while yielding to the watchdog.

Native tests include historical-window boundaries, backfill fairness, independent retry deadlines and rollover, failed staging rename/exhausted quarantine names, and out-of-order archive candidates beyond the old scan bound. The firmware and browser still require successful **current PR CI** before merging. The tests cannot directly prove every physical DS3231/flash reset interleaving; explicit power-cut, Wi-Fi reconnect, HTTPS receipt, heap/stack and real Modbus endurance tests remain mandatory. For supervised deployment and credential handling see [direct Wi-Fi pilot checklist](DIRECT_WIFI_PILOT.md) and [IoT protocol](IOT_HTTPS.md). Issue [#18](https://github.com/gedxxe/esp32-modbus-energy/issues/18) remains open for unverified hardware and transactional RTC recovery.

## 6 November 2026: PM5110 D+R hardware evidence and production correction

The owner then supplied **real PM5110 FC03 wire logs**, distinguishing the correct active energy basis from the earlier Delivered-only profile. Hardware was verified at slave 1, 19200 8E1; **D+R register 2704 / PDU 0x0A8F** returned CRC-valid Float32 bytes `48 C9 82 60` = **412691.00000 kWh**. Delivered-only PDU 0x0A8B returned **411196.75000 kWh**. These are separate cumulative bases, not an observed interval jump. Neither the new primary profile nor the separate website/gateway is yet known to be accepted in the complete production firmware. The website's inspected `iot-contract.js` currently does not list the D+R key.

The corrective branch adds ID 7 `schneider-pm5100-dr` as the selectable primary recording source. ID 6 is preserved as archived Delivered-only data; saved settings decode to ID 7 without rewriting historical captures. New regression assertions cover CRC-valid 0x0A8F/2 transport, Float32 decoding, persisted profile separation, reset of incompatible saved-device binding, diagnostic selection, web profile selection, and IoT event payloads. These **new assertions must pass CI** before accepting the branch; merely writing the tests is not evidence of a passing build. See [PM5110 D+R guide](SCHNEIDER_PM5100.md).

## 6 November 2026, earlier software-only audit: Schneider PM5100 profile

The owner-supplied PM51xx/PM53xx R01 XLS was read directly with a temporary xlrd reader, without modifying the source workbook or adding a runtime dependency. The selected rows and workbook SHA256 are recorded in [the profile reference](references/pm5100-registers.json). The new immutable ID is 6, with one-based register 2700 converted to PDU 2699 for Float32 delivered/import kWh. CT/VT is not applied a second time. The shared decoder handles the workbook's four-quadrant PF format, while non-finite kWh is rejected atomically.

The final native suite passed, including the actual standalone INO with the shared ModbusMaster source. PM5100 coverage verifies exact FC03 0x0A8B/2 frames, all selected PDU offsets, positive/negative PF quadrants and limits, no duplicate ratio multiplication, invalid counter isolation, and profile-6 checkpoint encode/decode with original Float32 bits. Existing Fort, ADL400, Habuer, ACR and APM regressions passed. The generated-byte browser test passed PM5100 and ACR/APM selection, profile-specific probe plans, commissioning behavior, reload, EN/ID and mobile overflow checks. Its saved-device scenario now waits for the newly requested candidate rather than interacting with a previous discovery result. Screenshots are simulated, not hardware evidence.

The ESP32-S3 development firmware and an isolated release build without the private IoT credentials header compiled successfully. The sanitized release binary was checked to contain every current generated gzip asset. Dashboard assets total 163,099 gzip bytes, below 160 KiB. No private header was read or copied into the release tree. The working private header and locally deleted public template were preserved; only the public template was restored from Git in the isolated release tree. Packaging now refuses a working tree containing that private header, since headers and compiled credentials must not enter a release ZIP.

No physical PM5100, live RADIUS or LoRa gateway test was performed. Float32 precision limits and delivered/import energy semantics are documented in [the PM5100 guide](SCHNEIDER_PM5100.md). Standalone hardware-target compilation was not repeated because it selects a different shared SDK allocator; the actual INO host build and production shared-library target compilation are the evidence for this change. The separate website/gateway must recognize the profile key/ID when provisioning the new meter. Public release images contain the example IoT configuration, while per-device development uploads can continue using the owner's private local header.

## 6 November 2026: correct cross-target SDK allocator verification

The PR #15 logic/documentation and browser jobs passed. The main ESP32-S3
N16R8 firmware also compiled successfully with the verified PSRAM-backed
Mbed TLS SDK. The firmware workflow failed **later**, in the optional
standalone USB diagnostic build. That separate sketch has no HTTPS uplink and
uses the default internal Mbed TLS allocation mode; the shared SDK bootstrap
had incorrectly required the external allocator from this target as well.

The bootstrap now gates the main firmware on external Mbed TLS allocation
and PSRAM, while the standalone diagnostic retains the original CA bundle,
date and Wi-Fi key-usage checks without an irrelevant memory-policy requirement.
All SDK artifacts are verified before cache stamping. No security validation
was turned off. Full CI and the real device's heartbeat HTTP 200 ACK remain
independent acceptance criteria.

## 5 November 2026: development HTTPS TLS-memory diagnosis

Field status reported ESP_ERR_HTTP_CONNECT (0x7002) and
MBEDTLS_ERR_SSL_ALLOC_FAILED (-0x7F00), consistent with an allocation
failure before HTTP response. The N16R8 custom SDK now requests and
verifies the Mbed TLS external-PSRAM allocator, while the uploader
publishes separate internal/PSRAM free and largest-contiguous-block
metrics. A real-world TLS handshake and authenticated cloud receipt
are still pending physical verification; a successful compile alone
is insufficient. Certificate-date, CA and hostname checks remain on.
This is a development-only provisioning arrangement, not hardened
production credential storage.

## 5 November 2026: station preparation and dashboard panels

Fixed a timestamp-underflow path where applyNetwork created a connection attempt after the loop had sampled its current time. Preparation now refreshes that time, guards against a stale earlier timestamp and reconciles ESP-IDF association before evaluating its five-second deadline. The station disconnect request is checked and no longer depends on a cached Arduino connection status. Only a fresh matching GOT_IP event can confirm online status. Dashboard help surfaces now have full padding, wrapping and local SVG icons, and the connection failure appears once beside the form result. The generated dashboard is 163,056 gzip bytes, within the 160 KiB limit. Native/browser/hardware tests were not run, following the owner instruction to avoid additional tests. The ESP32-S3 target compiled successfully with the updated embedded assets. The app and factory images were regenerated for the release package. Corporate authentication remains unverified.

## 3.5.0 fleet audit and urgent ADL400 patch

The fleet changes previously passed the native suites and generated-byte dashboard checks. The final owner-supplied ADL400 reference then required a further patch: FC03 `0x0000/2` is the contact/energy probe, with independent PT `0x008D/1` and CT `0x008E/1`. UInt32 ABCD /100 and PT × CT once remain unchanged. Production and shared standalone diagnostics use the same ratio helper. No additional native, browser or hardware tests were run after that patch, as explicitly requested by the owner. Both the ESP32-S3 main firmware and standalone INO compiled successfully after this ADL400 patch. Native/browser/RF tests were not rerun. GitHub CI is skipped for this urgent patch at the owner request. A full release package and field acceptance must not be inferred from earlier pre-patch results.


## Connection lifecycle and local commissioning in 3.4.3

A read-only check of the user's ESP32 at its supplied client IP showed firmware 3.4.2, an active Personal connection, `enterpriseProvisioning=false` and the recovery AP at `192.168.4.1`. This establishes that the reported form was being used through the client interface, where installing a new Enterprise login was refused. It does not establish an attempted or rejected RADIUS authentication. The revised form explains that interface requirement before credential entry, permits a secret-free retry of an unchanged saved profile and shows the connection outcome beside Save.

The final ESP32-S3 target compiled successfully with 149,156 static RAM bytes and 1,922,034 program bytes. The standalone diagnostic also compiled successfully with 36,840 and 373,572 bytes. Final dashboard assets total 159,324 gzip bytes, below 160 KiB. The existing upstream RadioLib USB-CDC/sleep warning remains visible.

All five native suites and the independent Python HMAC vector passed. The new suite compiles the production Enterprise SDK adapter and injects failures at each EAP configuration step, including custom CA installation. It covers the default public trust bundle, optional blank DNS, explicit private trust, missing clock, credential clearing, Personal after Enterprise, event admission, preparation/authentication/DHCP deadlines, preserved failure evidence and millisecond rollover. The implementation requires a matching association followed by GOT_IP before reporting online, rather than accepting an old station IP.

The generated-byte browser checks passed Enterprise submission/reload, editable replacement credentials, default/custom trust, client-interface instructions before input, secret-free saved-login retry, visible progress, timeout and retained failure details, both languages and phone layout. LoRa checks passed new key installation through AP/client dashboard interfaces, saved-key reuse, dirty-form action guards and overlap checks from 320 to 2560 CSS pixels. The full dashboard regression and all 34 SVGs passed. Screenshots were reviewed as simulated UI examples. Documentation and generated assets passed the repository checker.

The package is rebuilt from the final binaries and verified against its SHA256 manifest and ZIP contents. These local results are distinct from GitHub Actions, whose result must be checked for the pushed commit. Corporate PEAP authentication, RF multi-node operation and physical power-cut tests remain pending field acceptance. The live observation above is not an authentication or RF acceptance test.


## CI and LoRa controls in 3.4.2

The corrected 3.4.2 commit `5b76177` subsequently passed all three jobs in [GitHub run 41765048413](https://github.com/gedxxe/esp32-modbus-energy/actions/runs/41765048413).

GitHub run 41759933608 for commit 138c14c failed in the native power/LoRa test because Ubuntu GCC rejected a misleading single-line loop under `-Werror`. Its firmware and browser jobs succeeded. Local MinGW GCC 6.3.0 had not reported that warning, so the earlier local pass was not evidence of a passing Linux workflow. The loop now has an explicit block and the CI step prints its compiler version. Warning enforcement remains enabled.

The final ESP32-S3 build succeeded with 149,108 static RAM bytes and 1,917,362 program bytes. The diagnostic INO also built successfully with 36,840 and 373,572 bytes. The generated dashboard is 157,727 gzip bytes. All four native suites and the independent HMAC check passed locally. The upstream RadioLib USB-CDC/sleep warning remains visible.

The LoRa browser regression first reproduced Pair being enabled while the form displayed unsaved RF changes. It now confirms that Pair/Sample wait for saved settings. It also checks a required field inside closed Advanced settings, reveal/focus, local feedback, redacted keys and normal save/reload. Expanded controls were checked for pairwise overlap, panel overflow, label/control separation and minimum 44-pixel buttons at 320, 360, 390, 768, 1024, 1440 and 2560 CSS pixels in both languages. Desktop and phone illustrations were visually reviewed and remain simulated.

The Enterprise regression passed after its field validation was shared with LoRa. The full dashboard suite passed with all 34 SVGs. An initial local full-suite attempt exposed a wave probe that retained only its first central frame. The corrected probe measures the maximum observed crest across the same central window, preserving the existing 24–36-pixel amplitude, travel, fade and reduced-motion assertions. Production flow animation and vendored React Bits source were not changed.

These local checks and the failing baseline run are distinct evidence. The newly pushed revision must be checked through its own GitHub workflow rather than inferred from a local build. RF multi-node operation, physical power cuts and corporate RADIUS authentication remain pending hardware tests.

## Enterprise submission and PTMP preparation in 3.4.1

The final ESP32-S3 target compiled successfully with 149,108 static RAM bytes and 1,916,546 program bytes. The standalone diagnostic also compiled, using 36,840 and 373,572 bytes. Final generated web assets occupy 156,911 gzip bytes. RadioLib's existing generic USB-CDC/sleep warning remains visible, and no warning was suppressed to obtain these outcomes.

The pre-fix Enterprise regression loaded a saved custom-identity profile, selected another Enterprise SSID and failed because automatic outer identity stayed unchecked. After the fix, this case posts the username/password using automatic identity. The generated-byte Edge suite also passed an explicitly blank identity inside closed Advanced settings, automatic reveal/focus with a local error, save confirmation beside the button, credential reuse, public/custom trust paths, invalid DNS rejection, AP-only provisioning feedback, Personal after Enterprise, both languages and mobile width. These are form/API checks with simulated device responses, not RADIUS authentication evidence.

All four native suites and the independent Python HMAC vector check passed. New radio cases cover three distinct node addresses sharing one gateway, wrong-node/session rejection, matching outstanding ACK acceptance during backoff, rejection before any TX or during active TX, bounded contention and alternating live/archive progress when telemetry is permanently overdue. Existing power-loss, checkpoint, Modbus and persisted-setting regressions remain in those suites. The original public HELLO fixture stays at firmware text 3.4.0 to exercise the unchanged v1 contract rather than rewriting a historical vector.

The full generated-byte dashboard regression passed with all 34 SVGs. The final Enterprise and LoRa suites passed, including the bilingual PTMP identifier example, key privacy, malformed/absent-radio states and responsive layout. UI and the generated PTMP diagram were visually inspected. Screenshots use simulated values. [PTMP setup](LORA_PTMP.md) and the [gateway prompt](GATEWAY_PROMPT.md) describe receiver-side requirements and the shared-radio limits.

No C3 gateway firmware was implemented or compiled in this node project. Physical multi-node RF, real delayed ACKs, range/collision measurements, RADIUS access and physical power-cut tests remain pending. Software addressing tests establish protocol policy, not simultaneous RF capacity or a successful field installation.


## Power-loss hardening and SX1262 node in 3.4.0

The physical ESP32-S3 application and standalone diagnostic compiled successfully. The application uses 149,100 bytes of static RAM and 1,915,618 program bytes. The INO uses 36,840 and 373,572 bytes. The final offline dashboard is 156,104 gzip bytes, below the 163,840-byte budget. RadioLib 7.8.1 is pinned and its MIT notice is preserved. Its generic USB-CDC/sleep build warning remains visible, while this application does not enter ESP32 deep sleep.

All four native suites and the independent Python HMAC vector check passed. New fault models cover every truncated 80-byte radio bank and 446-byte capture record, corrupt inactive-bank fallback, false successful writes, bad readback, valid/corrupt archive promotion failures, a rejected final readback with its valid backup preserved, recovery-generation ordering and legacy semantic rejection. Radio cases cover default 915 MHz/off, field bounds, exact little-endian frame bytes, ACK/session/address rejection, pacing through millis rollover and the shared-library reservation hook in the actual standalone INO. These tests do not emulate the electrical behavior of flash or RF hardware.

The generated-byte LoRa browser suite passed editable carrier/IDs, key generation/redaction/reuse, duplicate-ID rejection, explicit unconfirmed pairing, simulated ACK status, AP-only key provisioning, absent/malformed radio behavior, recovery status, both languages and mobile width. The full dashboard regression passed with all 33 SVGs rendered, including the new wiring, radio topology and storage recovery diagrams. The optional radio netlist has 12 connections in addition to the existing installed wiring. UI screenshots are simulated. Local Chromium cache attempts were unavailable/incompatible, so working Edge was used. Both final Edge runs exited successfully. The unsuccessful Chromium launch attempts were not counted as passing tests.

The primary five-second capture rules, stored formats, immutable profile IDs, Fort/ADL400/Habuer/ACR/APM mappings and partition layout are preserved. Corrupt or missing meter settings beside retained data now stop production until a verified setting is saved. The brownout build guard, NVS/file verification and bounded radio BUSY/reset path were checked in the real target build. Radio archives are queued through core 1 and do not trigger maintenance mode or change the meter.

No physical power-cut, brownout transient, SPI/DIO measurement, antenna/RF-power check, range test or C3 gateway exchange was performed here. The gateway firmware remains a separate development. Pairing requires a matching authenticated ACK and cannot be inferred from successful initialization. See [power-loss audit](POWER_LOSS.md), [radio wiring/configuration](LORA.md) and [protocol v1](LORA_PROTOCOL.md) for failure limits and bench procedures.

## Enterprise login repair in 3.3.1

The ESP32-S3 application compiled successfully with the real IDF 5.5.5 EAP APIs and the full public certificate bundle. The standalone INO also compiled successfully, using 36,840 static RAM bytes and 373,456 program bytes. Main application static RAM is 146,396 bytes and program size is 1,856,539 bytes. The final dashboard occupies 151,552 gzip bytes, below 163,840. These are build sizes, not runtime heap measurements.

All three native suites passed, including public-bundle and private-CA CRC record round trips, legacy settings and trust-target credential reuse checks. The generated-byte Enterprise Edge suite passed username/password submission with no uploaded certificate, public-profile reload without a fake custom CA fingerprint, explicit custom CA/name submission and reuse, clock/rejection diagnostics, protected AP access guidance, an inactive-hotspot explanation, both languages and phone layout. A separate regression verifies that unsaved EAP credentials are omitted when switching to a Personal request through client HTTP. The full dashboard regression and all 29 SVG renders also passed. The form uses an arbitrary Corporate test SSID and a changed recovery AP name/address, rather than depending on a Midea client SSID.

Existing custom-CA profiles never fall back to public trust if their file is missing. Public trust does not need LittleFS, while all certificate date/key-usage checks and the optional DNS constraint remain enforced. The SSID, meter profiles, serial acquisition, checkpoint record format and schedules were not changed. The [Enterprise guide](ENTERPRISE_WIFI.md) gives the corrected steps and explains public versus private CA trust. The first Enterprise and full dashboard runs exited successfully. Later Windows Edge runs completed every assertion but stalled during browser-process shutdown, so only their verified test-owned processes were stopped. That host cleanup issue does not establish a firmware failure. UI images and API fixtures are simulated, and no physical RADIUS authentication, DHCP or certificate-negative test was performed here.

## ADL400 field-scan correction in 3.3.0

Both ESP32-S3 builds passed: the main firmware and the actual standalone INO. The generated web payload is 151,121 bytes gzip. All three native suites passed, including valid DC-addressed (slave 220) frames through the actual ModbusMaster library, UInt32 9798 at 0x0000, PT 1/CT 80, 97.98 secondary and 7838.40 final kWh, ABCD voltage/frequency, the one-shot comparison report, reverse-zero/nonzero observations and changed-ratio rejection. A valid exception at total energy does not cause fallback to another counter. Native tests also cover a verified saved binding for the new total map, old configuration migration retaining slave 220 and legacy/new checkpoint source identity.

The full generated-byte Edge regression and separate ADL400, discovery, Habuer and ACR/APM browser suites passed. The ADL400 case checks exactly one visible option, ID220 persistence, total 0x0000 and 0x53xx labels, base versus scaled engineering values, the comparison preset/command, both languages and mobile width. All 29 SVGs rendered. Test fixture values are software evidence, not an additional hardware acceptance claim.

The owner's successful scan is the field reference. The integrated release has not been connected to that meter here. The LCD difference of 0.5 kWh is below the 0.8 kWh raw-count step for PT1/CT80 and is not compensated by a fitted factor. See [ADL400 correction](ADL400_FIX.md) for the source mapping, original mismatches, exact engineering units and explicit command destinations. Historical forward ID1 is preserved internally and hidden from selection, while current ADL400 uses total ID5. Fort and Habuer behavior, checkpoint schema and schedules remain covered by the existing regressions.

## ACR/APM integration in 3.2.0

The native suites pass with the actual shared ModbusMaster source. Added tests exercise both new profile commands, exact independent PF/frequency/active/reactive request addresses, signed Int16 power, Float32 Wh conversion, no additional CT/PT query, non-finite snapshot rejection and retained last values. Registry checks cover unverified, missing and mismatched saved bindings, and restoration of the older default discovery plan. Float32 captures round-trip through the unchanged 446-byte schema, while a CRC-valid record containing NaN raw bits is rejected. Fort, ADL400 and Habuer retain their existing IDs and arithmetic.

The generated-byte ACR/APM browser suite passes selection of all five options, each new model's two-query discovery plan, the commissioning notice, explicit saved binding/activation, reload, live values, English/Indonesian text and 390px overflow checks. The full browser suite, older discovery suite and Habuer suite also passed, including all 29 SVG renders. The final generated payload is 150,749 gzip bytes, below 163,840. UI screenshots are simulated software fixtures, not hardware evidence.

Both physical-target builds passed using the pinned pioarduino Core 6.1.19, platform 55.03.311, Arduino 3.3.11 and IDF 5.5.5. The local Core had advanced to 6.2.0 and was restored with the repository's setup helper rather than bypassing its build guard. The final application is rebuilt after the last generated-web change before release packaging. Original manuals remain included with their separate ownership.

No ACR or APM meter was connected for these checks. ACR units/word order are a commissioning candidate, and APM table address 3000 conflicts with example address 3050. The existing verified named-device binding gates production for these two profiles. The [ACR/APM guide](ACREL_ACR_APM.md) records inspected PDF pages, all descriptor conversions, explicit command destinations and remaining LCD checks. A successful software build does not resolve those manual inconsistencies.

## Habuer integration and shared reading policy in 3.1.0

The physical ESP32-S3 N16R8 target and standalone diagnostic both compiled under the pinned pioarduino 6.1.19 Core, platform 55.03.311, Arduino 3.3.11 and ESP-IDF 5.5.5. The main image uses 133,084 bytes of static RAM and 1,839,259 program bytes. The standalone uses 36,840 and 369,956 bytes. The committed browser payload is 150,174 gzip bytes, below the 163,840-byte limit. These are build metrics, not runtime heap or CPU measurements.

All three native suites passed. New cases execute the real ModbusMaster library with independent FC03 requests at 0x0030, 0x0032, 0x0034 and 0x0035, verify Float32 ABCD and UInt32 scaling, continue after a missing first parameter, reject invalid PF/non-finite frequency, retain the previous accepted snapshot and omit CT/PT transactions for direct meter totals. Profile ID 2 survives registry and schema-3 checkpoint round trips, identity conversion is enforced for direct-total captures, and Fort/Acrel arithmetic and old record readers remain covered. Discovery plans now derive eight deduplicated signatures from the catalog without changing the persisted evidence layout. Optional Enterprise identity reuse also has a native regression.

The full generated-byte Edge browser regression, discovery suite, Enterprise suite and Habuer suite passed. Habuer checks cover selecting and saving the model, the 0x0030 probe, PF/frequency/active/reactive values, absent unsupported current/voltage/power cards, partial-snapshot diagnostics, accepted-value retention, English/Indonesian text, reload, mobile width, named finding activation and switching back to Fort. All 29 SVGs rendered. The Habuer desktop/mobile screenshots use simulated data and are representative UI illustrations.

The profile follows the owner's working [reference sketch](HABUER.md), whose original bytes and SHA256 are retained. No Habuer vendor manual, exact model, current/voltage/power register or numeric terminal assignment was provided. The integrated firmware was not flashed to a Habuer meter here. Compare active kWh, PF, frequency and reactive kvarh with the installed LCD and verify timing, restart recovery and boundary captures on hardware before accepting the installation. Reactive energy is live telemetry, while persistent checkpoints remain the active-kWh series.

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


## Standalone Wi-Fi IoT archive audit, November 5, 2026

The existing `main` firmware supports development-only direct HTTPS POST to the website `/api/iot-ingest` using ESP32 station Wi-Fi. This does not require a LoRa radio or gateway. The website's published event contract and its enabled-gated device bearer authentication were inspected; the website repository was not edited.

The audit found a real historical backfill bug: `main.cpp` archives checkpoints as `/c<day>.bin` and `/c<day>.bin.bak`, while `iot_uplink.cpp` looked for `/c<day>` and `/c<day>.bak`. Current/previous in-memory captures still worked, which masked the older-day failure. This correction uses a single pure archive path formatter inside the uploader and native regression assertions for canonical and backup filenames.

The worker now differentiates a genuinely absent archived day (for example, the ESP32 was powered off) from an existing but unreadable/corrupted file or a temporarily busy filesystem. Busy I/O is retried; an unrecoverable stored archive blocks further cursor advancement rather than silently skipping it. The HTTPS worker also avoids starting requests within the 15-second pre/post margins around each of the ten scheduled WIB checkpoint windows. The Modbus task, RTC writer, capture serialization, Wi-Fi station settings, LoRa protocol and website database are unchanged.

GitHub Actions native logic/documentation tests passed on the audit branch, including archive path and checkpoint quiet-window regressions. The software tests do **not** demonstrate sustained board Wi-Fi connectivity, certificate-chain acceptance against the actual development Vercel hostname, real website receipt persistence, physical Modbus timing under TLS load, unplug/replug recovery or 31-day loss scenarios. The public/default AP recovery credentials and compiled development bearer still require field security review. No on-device or Neon end-to-end result is claimed here; merge only after CI and development hardware acceptance.

## Archive filename and retention hardening (2026-11-06, review-only)

The firmware's filesystem retention and boot recovery must recognize only
**exact** production paths:
- Legacy CSV: `/d<local-day>.csv` or `/d<local-day>.csv.bak`.
- Scheduled kWh captures: `/c<local-day>.bin` or `/c<local-day>.bin.bak`.

The pure filename parser applies to the startup capture-directory scan.
Archive pruning no longer scans or mutates a live directory iterator:
it computes only the exact expired local day and checks at most two
canonical pathnames after the current archive save succeeds.
Files with suffixes such as `.bad`, `.tmp`, `.old`, quarantined
`/q*` prefixes, and unrelated/manual files are neither pruned nor
mistakenly treated as startup capture candidates.
The day must be a valid 2024–2099 local-day number, without zero padding.
Only **exactly 31 days old** canonical files can be retired after a
successful same-day archive write; a multi-year time jump is not a sweep.

The API's history visibility window still covers the current local day plus
30 previous days; the on-disk binary format, EEPROM/NVS keys, serial
settings, Modbus timing, HTTPS payload format, and accepted HTTP routes
are unchanged.

Regression coverage: `firmware/test/test_energy.cpp::testStrictArchiveRetention`.
CI native C++ tests, PlatformIO ESP32-S3 build and browser tests are
required before enabling this change on a device. This is a code/CI
hardening pass, **not** a claim of successful physical power-loss or
multi-year RTC-jump verification.

The broader separation of acquisition, hardware drivers, and HTTP from
`firmware/src/main.cpp` remains a separate architectural project.
Large cross-core moves are deliberately not part of this low-risk change.

### Long-run software storage and RTC ACK safety hardening (PR #22)

The 65th distinct production-day archive now marks the boot recovery scan incomplete even if it sorts newer than all 64 indexed days; this closes an ordering-dependent false `complete=true` result. The legacy five-minute NVS/CSV saver verifies exact existing bytes and avoids writing unchanged data, but retries missing/mismatched/short-read records through the original verified storage path, and reconciles leftover CSV staging before skipping a write. Native tests cover ascending and descending saturation, 300 unchanged NVS calls, forced corruption, torn writes and chunked CSV mismatches. When HTTPS uplink is enabled, an RTC backward-day repair now waits for a verified/absent persisted ACK cursor rather than treating a failed cursor load as a zero cloud ACK. These are software-only changes; the separate LoRa protocol, Modbus register profiles, raw checkpoint schema and normal time-sync rules are unchanged.

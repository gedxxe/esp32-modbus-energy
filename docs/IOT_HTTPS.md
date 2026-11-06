# HTTPS IoT uplink: development-only companion to Energy Record

This optional node feature mirrors the **existing** Energy Record `iot-contract.js` schema v1 and the development endpoint `/api/iot-ingest`. It also implements the website's [v0.11.48-alpha device-presence heartbeat](https://github.com/gedxxe/Midea-JRE-UNILAND-Waste-Calculator/blob/codex/v0.11.48-alpha/docs/iot.md#device-presence-without-modbus-readings). The website repository is a reference only; this branch changes the client alone. Do **not** enable uploads to production. The original Modbus reader, DS3231 acquisition, local UI, ten daily checkpoints, LoRa paths, and archived report formats remain unchanged.

## Ten-minute telemetry and remote Read now

- Valid live kWh telemetry is sent at startup and then no more frequently than **600 seconds after a successful telemetry receipt**, except an authorized manual request. Durable scheduled checkpoints retain their original WIB capture windows and backlog priority; the 60-second authenticated heartbeat still indicates device contact independently of kWh.
- The website stores one optional refresh request for each registered device. In the existing HTTPS heartbeat response it may send `refresh_after_utc` (UTC epoch seconds). This is trusted only after TLS validation and a matching `200` heartbeat ACK. The worker waits for an energy sample with `liveCapturedUTC > refresh_after_utc`, within a 120-second request lifetime, then uses the standard frozen telemetry payload and receipt validation. It does not execute arbitrary commands or alter Modbus registers. The browser never directly GETs the ESP32.
- The website owns the **10-second per-device cooldown and one-pending-request rule**. Firmware also remembers the last served request in RAM and ignores expired or replayed command timestamps for that boot. Existing HTTPS backoff, Wi-Fi/RTC gates, checkpoint quiet windows and noncritical processing still apply. A real request can take roughly a heartbeat interval plus a meter poll; a button tap is not a guaranteed reading.
- Website rollout is feature-gated with `IOT_MANUAL_REFRESH_ENABLED=1` after explicitly applying migration 010 and restricted grants to the development database. Older servers without this flag omit the optional command, and firmware continues periodic telemetry normally. No production database writes, migrations or device tests are implied by this change.

## Provision a development unit using the deployed website

The authenticated operator page is [Power Meter Reading (development)](https://midea-jre-iot-dev.vercel.app/iot-power-meter-reading). Its configured ingest endpoint is `https://midea-jre-iot-dev.vercel.app/api/iot-ingest`. The operator production site at `midea-jre.vercel.app` is **not** a destination for this development-only uploader. Website provisioning v0.11.47-alpha was merged in [website PR #119](https://github.com/gedxxe/Midea-JRE-UNILAND-Waste-Calculator/pull/119). The endpoint is provisioned in an **untracked private file**, not hardcoded into committed firmware.

1. Log in with an authorized **development admin** account and open **Add power meter**. Choose the physical factory, a unique stable device/source ID, a unique meter ID, and the correct **firmware profile key**. Supported selectable keys are `fort`, `habuer`, `acrel-adl400`, `acrel-acr`, and `acrel-apm`. Set the ESP32's saved meter profile, Modbus slave address and validated CT/PT semantics to the real hardware. The website IDs and profile are **exact, case-sensitive bindings**, independent of the ESP32 Wi-Fi/AP MAC identity. Never reuse a website source ID for a different physical meter.
2. While the one-time device key is displayed, choose **Download node config**. Keep the generated `iot_credentials.local.h` private. If it was lost, use the site's **Replace key** workflow; an old key becomes invalid. Do not submit it to chat, source control, serial logs, build artifacts, or issue screenshots. The generated header supplies `IOT_DEV_ENABLED`, `IOT_DEV_ENDPOINT`, `IOT_DEV_DEVICE_ID`, `IOT_DEV_METER_ID`, `IOT_DEV_METER_PROFILE`, and `IOT_DEV_BEARER`. This is the only normal way to enable the existing uploader; it is **off by default** without the file.
3. Save the downloaded file as `firmware/include/iot_credentials.local.h`. Keep it out of Git and source releases. The tracked `firmware/include/iot_credentials.example.h` is just a readable template; do not replace the downloaded header's exact registered values with its illustrative IDs.
4. Open the **`firmware/` folder** as the PlatformIO project, then run `pio run -e esp32s3` to compile and `pio run -e esp32s3 -t upload` to flash the connected ESP32-S3. This repository requires the pinned managed PlatformIO Core; if the system's `pio` executable reports a Core mismatch, run `python tools/setup-core.py` from the repository root and use that managed Core.
5. Configure firmware network mode **Client with recovery AP** or **Dual** on an approved 2.4 GHz internet-connected Wi-Fi. An ESP32 recovery AP alone cannot access Vercel. Leave the DS3231 correct in UTC; no cloud/NTP sync is written into the RTC by this uploader.
6. From the device's reachable local IP, open `http://<ESP32-IP>/api/status` and inspect `iot.configured`, `iot.enabled`, `iot.phase`, `iot.lastHttp`, `iot.accepted`, `iot.duplicates`, `iot.failures`, `iot.lastAckDay`, `iot.lastAckSlot`, and `iot.error`. `configured=true` is **not** proof of an accepted cloud upload. Compare the newest captured kWh and the optional current (A) on the **development** website card with the physical LCD, meter wiring and profile scaling, noting separate capture/receipt timestamps.

The uploader always uses the existing `POST /api/iot-ingest` schema v1, `Content-Type: application/json`, `X-Device-ID`, and `Authorization: Bearer`. It does not upload browser cookies, website credentials or database credentials. No direct LoRa gateway is necessary for this Wi-Fi path.

**Security:** HTTPS uses the ESP-IDF public CA bundle with hostname verification. Plain HTTP and redirects are not permitted. RTC-derived UTC anchors the **TLS system clock**; the task never writes the DS3231. The development token is compiled into the local device image and stored in plaintext flash: this is acceptable only on a controlled test unit, not a secure production provisioning design. Rotate credentials after testing and protect physical access. Review and replace any public recovery-AP default credentials before connecting a field unit.

## Recording and delivery

- `checkpoint` events encode only accepted **cumulative primary kWh**, with a stable `cp_<local-day>_<slot>` event ID, strict UTC seconds, source-profile check, and the actual started/captured timestamps. The server accepts an identical retry with status 200 and a new insert with 201.
- The worker reads the existing CRC-verified active/previous captures or archived `/c<day>.bin` and `/c<day>.bin.bak` files. It sends one observation at a time without touching UART or changing captures. If filesystem access is briefly busy, the worker retries rather than skipping that archive. If an existing archived day is corrupt and neither copy is valid, the uploader blocks for recovery; it cannot truthfully claim complete synchronization. A day for which **no archive exists** is skipped without inventing a reading (for example, when a node was powered off). A dual-copy, CRC-verified `iot_ack_*.bin` cursor tracks the latest confirmed checkpoint. Only 200/201 **with the matching returned event ID**, followed by verified local cursor persistence, advances it. A crash between server commit and local ACK yields a harmless duplicate retry.
- Initial backfill is bounded to the most recent 31 local dates; after an acknowledged cursor falls outside the 31-day window, upload stops with `archive_window_exceeded` rather than claiming lossless historical sync. Retain device backups and make a recovery plan before longer outages or production use. Archived data that is already absent or inconsistent cannot be recreated.
- HTTPS requests are intentionally paused from 15 seconds before each scheduled checkpoint until 15 seconds after the five-second acquisition window. This prevents a network handshake from competing with the time-critical Modbus read. A preexisting HTTPS request can still overlap a checkpoint when its network operation unexpectedly exceeds the bounded socket timeout; physical load testing remains necessary.
- `telemetry` is the latest valid primary cumulative kWh, with **optional `readings.current_a`** sourced from the existing `currentA` Modbus snapshot. Only one explicitly named current-A channel is reported: **not** three phases, a sum or an average. Fort, Acrel ACR/APM and the selected ADL400 profile define current registers; the existing Habuer profile does not, so Habuer sends kWh only. The current field is omitted when a successful current measurement is unavailable, more than 15 seconds old, negative or non-finite. A measured zero is transmitted as `"0.000000"`. The value is already decoded by the selected meter profile, with no extra Modbus read or CT/PT multiplication inside the uploader. Live kWh and current are nearby but potentially separate samples; `captured_at` remains the energy snapshot timestamp, not a fabricated synchronized current timestamp. Telemetry is uploaded no more frequently than once every 30 seconds after checkpoint backlog clears. Telemetry uses a fresh per-boot event ID and retries the same in-memory event until accepted. **Unlike checkpoints, live telemetry pending at power loss is not persisted**. Telemetry and checkpoints remain separate from website reports; no automatic input overwrite or false instantaneous power measurement is performed.
- Failed reading requests retain their existing bounded retry/backoff, frozen payload and cursor logic; 429 honors numeric `Retry-After` with a cap. Permanent 400/401/403/409 and mismatched 200/201 reading receipts halt the worker until reboot/reprovisioning. `/api/status` reports the error, HTTP status and last durable checkpoint cursor.
- **Authenticated presence without Modbus:** about every 60 seconds, outside the same capture quiet window, the HTTPS worker POSTs `{"schema_version":1,"event_type":"heartbeat","device_id":"<registered>","meter_id":"<registered>","meter_profile":"<registered>"}` to the configured private endpoint with the existing bearer and X-Device-ID. It contains **no readings, event_id or timestamp** and does not inspect the UART. The server returns HTTP 200, `event_type="heartbeat"` and the matching device ID; only that combination counts as acknowledged contact. Invalid JSON/overflow, 201 and mismatching identity **do not** count. Heartbeat success updates only the local heartbeat contact diagnostics, never `accepted`, `duplicates`, `lastAcceptedUTC`, `lastAckDay`, `lastAckSlot`, frozen telemetry retries or a checkpoint cursor.
- A heartbeat has its **own bounded exponential retry/backoff** and `Retry-After` handling; attempts do not postpone pending reading retries. Reading backlog always has priority. The minimum 2.1-second send spacing, verified TLS, no-redirect policy and ±15-second checkpoint quiet window still apply to both kinds. A permanent authentication, binding or ACK failure blocks the worker. No fake reading or cloud-connected state is generated from Wi-Fi or a credential-free probe.
- The presence heartbeat can operate with **LittleFS unavailable**. In that mode `iot.archiveReady=false`, `iot.archiveError="filesystem_unavailable"` and `storageRecovery.mounted=false`, and the worker does **not** open, upload, acknowledge or advance any archive or send live telemetry. Recovery requires a normal reboot after safe storage inspection. This exception enables cloud presence only; it does not claim record durability or repair a corrupt filesystem.

Do not create an HTTP endpoint inside the meter or route browser credentials to Vercel. Do not send multiple meter identities through a single registered source. If a physical meter, ratio semantics or profile changes, register a **new** device source ID with the website before uploading new data.

## Testing and limitations

The native `firmware/test/test_iot_contract.cpp` validates deterministic JSON shape, timestamp conversion, profile matching, invalid/late captures, nonnegative decimal serialization, fresh/expired/missing current, measured zero, buffer limits, frozen retries, archive filenames and the HTTPS quiet interval. The CI compiles the ESP32 firmware **disabled** first and uploads only that image as its build artifact. It then performs a separate **enabled** PlatformIO build using a generated, ignored **synthetic** 43-character device key and a non-routable test domain. The synthetic image must never be flashed. Hardware tests using the separately provisioned real private header and physical end-to-end acceptance remain outstanding.

Required field tests before rollout: TLS certificate failure; a revoked and a mismatched device; invalid/late checkpoint rejection; server timeout followed by identical replay; 429 and 503; restart before and after ACK; recovery from a corrupt ACK bank; 31-day retention limit; Wi-Fi reconnect and simultaneous LoRa/UI load; clock changes; no missed scheduled readings; no changes to website report or autosave tables. No such hardware or cloud tests are claimed to be completed by this branch.

## Provisioning diagnosis (safe to share)

If `iot.phase=0`, the uploaded image was built with the IoT development flag disabled. If `iot.phase=4` and `iot.configured=false`, the development flag was enabled **but one of the compile-time header checks failed**. A registered source on the website, or previously saved Wi-Fi/network settings, does not automatically populate `firmware/include/iot_credentials.local.h`. The header is compiled into the image; uploading the wrong build also keeps old values.

Current status error codes distinguish:
- `iot_binding_invalid`: device ID, meter ID or profile ID is empty/contains unsupported characters.
- `iot_bearer_length_invalid`: bearer is not the required 43-character URL-safe key (do not paste it into logs).
- `iot_bearer_format_invalid`: bearer contains a character outside `A-Za-z0-9_-`.
- `iot_endpoint_https_required`: endpoint is not HTTPS.
- `iot_endpoint_path_invalid`: endpoint must end with exactly `/api/iot-ingest`.
- `iot_endpoint_too_long` / `iot_endpoint_url_invalid`: endpoint length or URI syntax rejected.

Never troubleshoot by printing full build flags, uploading headers, displaying the bearer in a screenshot or posting `-v` compiler output containing it. The error class alone is enough for next steps. Correct the local ignored header, rebuild **and upload**, then recheck `/api/status`. A `configured=true` status still requires a verified heartbeat HTTP 200 ACK to claim recent cloud contact, and separately a matching 200/201 event receipt and valid meter sample to claim reading delivery.

## Connection diagnostics and build verification

The PlatformIO pre-build hook checks a present private header locally (without printing its contents), and switches a nonsecret compiler define when the header appears or disappears. This avoids `__has_include` negative-dependency build-cache surprises. If PlatformIO discovers the header but the compiler cannot include it, the build stops with an explicit diagnostic rather than silently disabling authentication. The private header remains ignored by Git.

The uploader now includes `iot.clientRevision: 3` and `iot.localHeaderPresent` in the local `/api/status` payload. These values prove the flashed app exposes this diagnostic revision and whether `firmware/src/iot_uplink.cpp` included the local header at **build time**. A file merely existing in VS Code is not proof the flashed image saw it. `iot.configured` is the compile-time header format check, *not* an internet or backend connectivity check. `iot.enabled` means a worker was successfully created, not that HTTPS worked.

For failed requests inspect `lastHttp`, `transportError` (ESP-IDF error code), `socketErrno`, `tlsError`, `tlsFlags` and `freeHeapBeforePost`. These fields contain numerical diagnostic codes, not hostnames, headers or secrets. `lastHttp=0` with nonzero transport/TLS errors points to DNS/TCP/TLS before HTTP; 401 means credentials/registration, 403 means source binding, 404 means wrong endpoint, and 503 with `IOT_NOT_CONFIGURED` means website IoT is disabled. A redirect is blocked, never followed with the bearer. Retry status keeps the root-cause category instead of replacing it with generic `retry_scheduled`.

With no valid Modbus sample, the worker instead sends an **authenticated HTTPS heartbeat POST** at about 60-second intervals. `iot.heartbeatAttempts`, `heartbeatAccepted`, `heartbeatFailures`, `heartbeatHttp`, `lastHeartbeatUTC`, `heartbeatTransportError`, `heartbeatTlsError`, `heartbeatTlsFlags` and `heartbeatError` report its outcome without exposing credentials. `lastHeartbeatUTC` is local RTC time on a **verified** heartbeat ACK, not a fake meter capture time. The old `iot.probe*` fields remain for backward API compatibility, but the credential-free GET probe is not issued by this uploader revision. HTTP 405 from a manual GET only proves endpoint routing; it is not authenticated presence.

The PlatformIO configuration explicitly enables the verified x509 certificate bundle and HTTPS support. If either is missing in the effective ESP-IDF SDK configuration while IoT is enabled, compilation fails closed. After updating firmware, perform a clean build and flash the *same* `firmware/` project. Neither clean nor upload requires erasing NVS/LittleFS. Test a real HTTPS POST and matching server ACK before treating the link as operational. Never paste the private header, bearer, or verbose compiler command lines into logs/issues.

## Headless LittleFS recovery without an OLED

When `iot.configured=true` but `iot.error=filesystem_unavailable`, the HTTPS header is valid; no HTTP request has been attempted. Wi-Fi association is **not** cloud connectivity. A failed mount does not prove a blank or irrecoverable filesystem.

The firmware exposes `storageRecovery` within `/api/status`: mount state, partition presence and size, boot action, retry attempts/result, OLED presence, and the recovery URL. On a mount failure, the root URL redirects to `/storage-recovery`. The complete existing dashboard remains available at `/dashboard`. Visit `http://192.168.4.1/storage-recovery` from the ESP32 recovery AP, or use the live client IP plus `/storage-recovery`. This page is embedded in the firmware image and does not depend on a connected OLED or on LittleFS. Serial Monitor (USB, 115200 baud) reports mount/partition status and physical erase progress without printing secrets.

**Retry mount — no formatting** queues a bounded command to the core-1 owner. It closes the failed mount, tries `LittleFS.begin(false)`, and unmounts if successful. It does not erase, write archives or change Wi-Fi, RTC, Modbus or LoRa settings. If it succeeds, restart normally to perform the standard NVS/FS recovery and initialize the HTTPS worker; it intentionally does not start uploading against a partially restored filesystem. A failed retry is evidence for inspecting partition compatibility and backups, not permission to destroy archives. The action is denied during active diagnostics/scans and near a scheduled checkpoint window.

**Formatting is destructive and physical only.** After exporting or backing up recoverable records, power off; hold the physical OK button (GPIO6) LOW as power is applied; keep holding for five seconds, releasing early to cancel. The firmware will format only when the original mount failed and the storage partition exists. The OLED is optional. The Serial output shows progress and the final result. Formatting removes LittleFS archives and other filesystem material (including custom network CA), but does not erase NVS. There is no network format endpoint. If GPIO6 is not wired, arrange supervised physical maintenance rather than shorting arbitrary pins or bypassing the safeguard.

When storage cannot mount, the firmware continues authenticated heartbeat-only transport after internet-connected Wi-Fi and a valid RTC are available. `archiveReady=false` and `archiveError` remain explicit, and **no** archive or live telemetry is uploaded until normal boot restores storage. Once a heartbeat is accepted, the website's v0.11.48-alpha device card can show **Connected** even though `last accepted reading` is still empty. Presence expires after 180 seconds without authenticated contact. This is not a persistent TCP connection and is not evidence that Modbus, RTC checkpoint capture, or LittleFS works.

## Status values

`iot.phase`: 0 disabled, 1 idle/waiting, 2 upload in progress, 3 retry/backoff, 4 blocked. `accepted` is newly created **readings**, `duplicates` is confirmed idempotent reading replay, and `heartbeatAccepted` counts accepted **contacts** separately. The `lastAckDay` and `lastAckSlot` fields identify the last durably acknowledged checkpoint and are not changed by heartbeat. `archiveReady=false` means checkpoint upload is disabled, even if `heartbeatAccepted>0`. A unit running old firmware will not expose the `iot` status field.

## TLS memory exhaustion on ESP32-S3 N16R8 (5 November 2026)

A physical development node reported `heartbeatTransportError=28674`
(ESP_ERR_HTTP_CONNECT) and `heartbeatTlsError=32512` (Mbed TLS -0x7F00,
SSL_ALLOC_FAILED) with no HTTP response. Its aggregate heap was about 8.4 MB
(including PSRAM), while free internal DRAM reported by the local API was
about 36.5 KB. This is evidence of an allocation failure *before* HTTP;
it is not evidence of an invalid URL, bearer, server registration or
certificate chain. Never disable certificate verification to hide this error.

The pinned N16R8 custom ESP-IDF SDK now builds Mbed TLS with
`CONFIG_MBEDTLS_EXTERNAL_MEM_ALLOC=y` so its TLS allocations use the
installed 8 MB PSRAM rather than exhausting internal DRAM. The SDK bootstrap
checks the compiled header, not merely a PlatformIO build flag. The status
includes `tlsAllocatorPsram`, `internalFreeBeforePost`,
`internalLargestBeforePost`, `psramFreeBeforePost` and
`psramLargestBeforePost`. The existing `freeHeapBeforePost` remains for
backward compatibility but is an aggregate number. A subsequent 0x7F00
error now reads `heartbeat_tls_out_of_memory` or `tls_out_of_memory`
instead of an ambiguous handshake error.

**Development-only security:** PSRAM is external physical RAM, and the
observed test unit reported `flashEncrypted=false`. This does not provide
hardened at-rest secret protection. Keep credentials private, secure physical
access, rotate test credentials when appropriate, and do not deploy as
a production credential scheme. CA bundle, hostname and certificate-date
verification remain mandatory; HTTPS redirects are never followed.

After a **clean build from firmware/**, verify `tlsAllocatorPsram=true`,
review the internal/PSRAM largest blocks, and test `heartbeatHttp=200`,
`heartbeatAccepted>0`, `heartbeatError=""` against the actual dev
deployment. Any subsequent HTTP 401/403, Vercel deployment protection or
invalid server ACK is a separate follow-up to diagnose, not a reason
to weaken TLS. Modbus kWh remains unavailable until the meter is attached.

### 6 November 2026: target-aware SDK verification

The initial PR #15 CI passed all browser and logic/documentation checks.
The main ESP32-S3 application **also compiled successfully** with its
verified custom PSRAM-backed Mbed TLS SDK. The combined firmware job failed
at the later, independent `examples/FortSerialDebug` build, however:
`tools/sdk_bootstrap.py` required the external allocator for **every**
PlatformIO project. That USB-only diagnostic does not run the IoT uploader
and intentionally uses the SDK's internal TLS allocation choice.

The corrected bootstrap distinguishes the main firmware from the standalone
diagnostic. The main firmware remains **strictly gated** on external Mbed TLS
memory, PSRAM and CA/date/hostname verification, with a failure before the
SDK cache can be marked verified. The diagnostic keeps certificate-date,
full CA bundle and Wi-Fi key-usage checks, but does not inherit an
unnecessary IoT-specific allocator requirement. The prior Kconfig choices
remain explicitly pinned for the main firmware.

Neither the green application build nor the standalone build proves the
physical device has completed an HTTPS heartbeat handshake and received
a matching application ACK. Do not weaken TLS verification or format LittleFS
to troubleshoot a transport error.

## RTC held by future-dated records (6 November 2026)

A field device reported `rtc=false`, retained `dayDate=2033-11-06`,
`iot.error=waiting_for_wifi_rtc` and zero heartbeat attempts even though
Wi-Fi, LittleFS and external PSRAM TLS were available. That means no HTTPS
attempt has been made: do not diagnose TLS or bearer credentials yet.

New read-only `/api/status.rtcDiagnostic` fields distinguish:
- `hardwareReadable=true`, `state=before_retained_records`: the
  DS3231 time read succeeded, but its calendar precedes persisted
  checkpoint dates; normal phone RTC sync is blocked with `backward_date`.
- `state=i2c_read_failed` or `i2c_mutex_busy`: inspect DS3231 0x68
  power, GPIO8 SDA, GPIO9 SCL, I2C pull-ups, wiring and bus contention.
- `state=oscillator_stop_flag`: DS3231 time is untrusted; check
  backup power, then deliberate initialization after reviewing archives.
- `state=rtc_invalid_bcd`, `rtc_invalid_range`, `rtc_invalid_date`,
  `rtc_12h_mode`, or `rtc_century_flag`: unsupported or invalid calendar.
- `state=ok` and `rtc=true`: RTC and retained-date guard both passed.

`rawUTC`/`rawDate` are populated only after a valid DS3231 read.
`retainedDay`/`retainedDate` show the newest stored tracker or capture
date. `recordRollbackBlocked` identifies a valid RTC held by this guard.
USB Serial 115200 prints the reason on state transitions without OLED.
A Modbus timeout can overwrite generic `error_code`, so these dedicated
fields are essential. The change does NOT relax the no-backdating rule,
write a guessed RTC, clear NVS or format LittleFS. Archive and export any
misdated records before a separate deliberate reconciliation; fixing
the RTC alone cannot defeat the record-protection gate.

## Fix: browser RTC sync blocked by an empty 2033 checkpoint (6 November 2026)

The previous protection checked the **latest stored day** rather than
whether it held real readings. A DS3231 with a valid 2026 clock was marked
\`rtc=false\` when an old, empty 2033 tracker/capture date remained in
NVS/LittleFS; the browser's normal **Sync RTC** was then rejected with
\`backward_date\`. This is a firmware deadlock, not a reason to reset the
whole flash or disable archive safeguards.

The standard authenticated \`POST /api/time\` now supports a **narrow,
automatic empty-record repair when the operator presses Sync**:
- All in-memory future legacy intervals must be empty (no measured kWh).
- Every in-memory future checkpoint must be waiting/missed with no raw
  meter measurements; pending rollover backlog or future cloud ACK blocks.
- Every future \`/c<day>.bin[.bak]\` archive must decode with valid CRC and
  contain no recorded meter readings; damaged or real records block.
- The current NVS checkpoint is preserved under \`/qnvsc<day>.bin\`.
  Future empty archives are renamed into \`/qc<day>.bin[.bak]\`, and
  \`/capture.pending\` is quarantined if relevant. The /q names are
  intentionally excluded from the normal boot archive scanner.
- Only once backups are verified, the existing RTC write/readback runs.
  If it fails, active NVS is left unchanged. On success, correct day
  checkpoints are persisted with CRC as before. Never format LittleFS
  or clear the entire NVS.

The 2033 snapshots remain accessible as preserved flash files until
separately reviewed; this recovery does not claim they represent
measurements. Genuine measured future records are **not** reset by this
automatic path. Existing legitimate records for the requested 2026 day
must be preserved, not replaced by blank intervals/captures.

After flashing, press the normal Sync RTC button with a trusted device
clock. Expect \`rtcDiagnostic.state="ok"\`, \`rtc=true\`, and subsequently
at least one heartbeat attempt; HTTP 200 ACK still requires independent
real-device testing. A \`rtc_recovery_*\` command error means no automatic
destructive fallback has occurred. Never run \`erase_flash\`, clear all NVS,
or format LittleFS to bypass a protected record.

**Retention hardening:** the legacy/capture archivers now prune only the
date *exactly* 31 days before a successfully saved day. They no longer
erase every older date in one sweep after an accidental multi-year RTC
jump. Old records skipped by extended outages therefore remain until
explicit review/export; filesystem capacity must still be monitored.
The repair uses a bounded 72-entry static path list and fails closed if
there are unexpectedly more future files, rather than exhausting stack
or silently leaving an unexamined archive.

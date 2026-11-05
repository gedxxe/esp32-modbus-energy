# HTTPS IoT uplink: development-only companion to Energy Record

This optional node feature mirrors the **existing** Energy Record `iot-contract.js` schema v1 and the development endpoint `/api/iot-ingest`. The website repository is a reference only; this branch changes the client alone. Do **not** enable uploads to production. The original Modbus reader, DS3231 acquisition, local UI, ten daily checkpoints, LoRa paths, and archived report formats remain unchanged.

## Provision a development unit

1. Prepare a **separate** Vercel/Neon development deployment. Follow the website [IoT development instructions](https://github.com/gedxxe/Midea-JRE-UNILAND-Waste-Calculator/blob/main/docs/iot.md) for the explicit migration, owner-scoped device registration, and `IOT_ENABLED=1` on **development only**. Its runtime authentication uses a 256-bit bearer key, not browser cookies.
2. Copy `firmware/include/iot_credentials.example.h` to `firmware/include/iot_credentials.local.h`. Edit the copy with the exact HTTPS development endpoint, registered device ID, meter ID and matching meter-profile key (for example `fort`, `habuer`, or `acrel-adl400`). Enter the private 43-character base64url device bearer from the website's registration script. The local file is excluded from Git. Do not paste it into issues, docs, screenshots, serial logs, or assistant chats.
3. Set the firmware Wi-Fi mode to **Client with recovery AP** or **Dual** through the existing local UI. Use a permitted 2.4 GHz network with Internet access. The unit's local AP alone does not reach Vercel. The cloud uploader only starts when `IOT_DEV_ENABLED=1`; with no local provisioning header, it stays disabled.
4. Open `firmware/` in PlatformIO, build and flash `esp32s3`, then check `GET /api/status` under the `iot` object. It exposes only status/counters, never the device bearer. Use the website's `Power Meter Reading` development page to inspect accepted events.

**Security:** HTTPS uses the ESP-IDF public CA bundle with hostname verification. Plain HTTP and redirects are not permitted. RTC-derived UTC anchors the **TLS system clock**; the task never writes the DS3231. The development token is compiled into the local device image and stored in plaintext flash: this is acceptable only on a controlled test unit, not a secure production provisioning design. Rotate credentials after testing and protect physical access. Review and replace any public recovery-AP default credentials before connecting a field unit.

## Recording and delivery

- `checkpoint` events encode only accepted **cumulative primary kWh**, with a stable `cp_<local-day>_<slot>` event ID, strict UTC seconds, source-profile check, and the actual started/captured timestamps. The server accepts an identical retry with status 200 and a new insert with 201.
- The worker reads the existing CRC-verified active/previous captures or archived `/c<day>.bin` files. It sends one observation at a time without touching UART or changing captures. A dual-copy, CRC-verified `iot_ack_*.bin` cursor tracks the latest confirmed checkpoint. Only 200/201 **with the matching returned event ID**, followed by verified local cursor persistence, advances it. A crash between server commit and local ACK yields a harmless duplicate retry.
- Initial backfill is bounded to the most recent 31 local dates; after an acknowledged cursor falls outside the 31-day window, upload stops with `archive_window_exceeded` rather than claiming lossless historical sync. Retain device backups and make a recovery plan before longer outages or production use. Archived data that is already absent or inconsistent cannot be recreated.
- `telemetry` is the most recent valid normalized kWh, uploaded no more frequently than once every 30 seconds after checkpoint backlog clears. Telemetry uses a fresh per-boot event ID and retries the same in-memory event until accepted. **Unlike checkpoints, live telemetry pending at power loss is not persisted**. Telemetry and checkpoints remain separate from website reports; no automatic input overwrite or false instantaneous power measurement is performed.
- Failed requests use bounded backoff; 429 honors numeric `Retry-After` with a cap. Permanent 400/401/403/409 and mismatched 200/201 receipts halt the worker until reboot/reprovisioning. `/api/status` reports the error, HTTP status and last durable checkpoint cursor.

Do not create an HTTP endpoint inside the meter or route browser credentials to Vercel. Do not send multiple meter identities through a single registered source. If a physical meter, ratio semantics or profile changes, register a **new** device source ID with the website before uploading new data.

## Testing and limitations

The native `firmware/test/test_iot_contract.cpp` validates deterministic JSON shape, timestamp conversion, profile matching, invalid/late captures, and nonnegative decimal serialization. The CI also compiles the ESP32 firmware with the feature **disabled** and validates that the original acquisition/UI still builds. The optional cloud-on configuration requires a second PlatformIO build on the development unit and physical end-to-end verification.

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

Never troubleshoot by printing full build flags, uploading headers, displaying the bearer in a screenshot or posting `-v` compiler output containing it. The error class alone is enough for next steps. Correct the local ignored header, rebuild **and upload**, then recheck `/api/status`. A `configured=true` status still requires an actual 200/201 receipt and a live meter reading before claiming end-to-end success.

## Status values

`iot.phase`: 0 disabled, 1 idle/waiting, 2 upload in progress, 3 retry/backoff, 4 blocked. `accepted` is newly created events and `duplicates` is confirmed idempotent replays. The `lastAckDay` and `lastAckSlot` fields identify the last durably acknowledged checkpoint. A unit running old firmware will not expose the `iot` status field.

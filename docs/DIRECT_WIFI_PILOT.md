# Direct Wi-Fi industrial prototype: ESP32-S3 → HTTPS

**English** · [Bahasa Indonesia](id/DIRECT_WIFI_PILOT.md) · [HTTPS internals](IOT_HTTPS.md) · [Power-loss recovery](POWER_LOSS.md) · [Fleet readiness](FLEET_READINESS.md)

This is a supervised industrial **development pilot**, not certification for unattended factory deployment. The ESP32-S3 reads its installed RS485/Modbus meter and independently sends authenticated HTTPS events to the **development** Energy Record website over an approved 2.4 GHz Wi-Fi network. **No ESP32-C3 gateway or LoRa transmitter is required**; leave LoRa disabled. Do not modify production website data, LoRa settings, meter registers, NVS identity, partitions, or existing captures as part of a connectivity trial.

## Before flashing an installed node

1. Export the existing cumulative-kWh checkpoints and note their raw counts, profile, CT/PT settings, and device identity. Record the current installed firmware, reset reason and known-good Wi-Fi mode. Never flash a merged factory image over a commissioned device: use the normal application upload without partition erasure.
2. Provision one unique **development** website source with a profile key exactly matching the saved firmware meter profile. Download its generated `iot_credentials.local.h`, save it only under `firmware/include/`, keep it untracked and secret, then build **your own** ESP32-S3 application image. The synthetic GitHub Actions artifact is **not** a usable device image.
3. On the local dashboard choose **Client with recovery AP** or **Dual** and an approved 2.4 GHz network with outbound HTTPS. AP-only has no internet uplink. The factory AP recovery credential must not be deployed unchanged on an untrusted network; use an isolated test VLAN/SSID and control physical access.
4. Verify DS3231 UTC, WIB local schedule, RS485 response CRC, Modbus slave/baud/parity, selected profile and live cumulative kWh versus the meter LCD. Check that the device has a mounted filesystem and a valid meter config before testing upload. The HTTPS worker never adjusts DS3231 or meter registers.

## Expected cadence and independent evidence

| Indicator | Expected during a healthy pilot |
| --- | --- |
| Wi-Fi status | Associated on the intended SSID, with DHCP/IP; **not proof** of website acceptance |
| `iot.configured` / `iot.enabled` | Local provisioning accepted / HTTPS worker started; neither proves a successful cloud request |
| `iot.heartbeatAccepted`, `iot.lastHeartbeatUTC` | Authenticated matching HTTP 200 presence receipt, about every 60 seconds when due and not in a quiet window |
| `iot.accepted` or `iot.duplicates` | Separate matching 201/200 **reading** receipt; observe the cumulative kWh on the **development** website |
| Live readings | Once at start and about every 10 minutes **after successful telemetry acceptance**, or after an authorized fresh manual request |
| Checkpoints | Original ten WIB times, 5-second acquisition window; immutable, actual timestamps and source values, not reconstructed missing readings |
| `iot.lastAckDay` / `iot.lastAckSlot` | Only advances after server receipt **and** verified local cursor write; never advance merely to clear a backlog |
| `iot.archiveError` | Independent historical problem requiring inspection; presence and live data can continue without claiming historical completeness |

A historical backfill pause (for example `archive_window_exceeded`, `archive_corrupt_needs_review` or `archive_lock_busy`) **does not certify complete history**. New live telemetry and heartbeat remain independent. The upload worker fairly interleaves at most three historical checkpoint transmissions before serving due fresh telemetry/presence. An HTTPS `401`, binding mismatch, invalid accepted-event receipt, or other security-critical terminal failure may still intentionally block the uploader until reviewed.

## Failure-injection matrix (supervised test hardware only)

- Disconnect the approved Wi-Fi, change SSID or disable DHCP temporarily. Modbus and local checkpoint persistence must continue while the network is unavailable, then HTTPS should recover with a valid matching server receipt after reconnection.
- Simulate network/server timeout, 429, 503, TLS validation failure and corrupt/mismatched ACK in a controlled development environment. Confirm no bearer disclosure, no HTTP downgrade, no silent zero or fabricated kWh.
- Reboot **outside** and **near** a capture window. Confirm retained NVS/file generations, correct RTC, no duplicate created reading in the website and idempotent retry of the same checkpoint event ID when necessary.
- Place a malformed `/capture.pending` on a sacrificial test device and verify it is quarantined as `.bad` rather than overwritten. Once the nine bounded names are occupied or rename fails, checkpoint filesystem saving must fail visibly without data deletion.
- Test more than 256 miscellaneous recovery directory entries on a test filesystem. The recent valid daily archive must still be found. More than 4,096 inspected entries must produce an explicit recovery-scan error and **block capture-day advancement**, not silently assume recovery was complete.
- Test an old server ACK cursor beyond the retained 30-day backfill window **without manipulating the real production RTC**. Confirm historical upload pauses, `iot.archiveError` remains visible, while a new valid live reading and authenticated heartbeat can still be received. Export/reconcile history manually; never wipe an ACK cursor to conceal the gap.
- Run a supervised 24–72-hour soak with physical meter and network. Record minimum heap/largest internal block, task stack high-watermarks, WDT/reset reasons, RTC health, missed captures, NVS/LittleFS flags, HTTP/TLS errors, heartbeat acceptance and long reconnect cycles. Extend the duration to match the actual factory reliability target.

## Release and rollback safeguards

**Never** run partition erase, NVS reset, LittleFS format, full-device flash clone, synthetic CI credential flash, or unchecked `iot_ack_*.bin` deletion during pilot rollback. Save source exports and original image hashes before deployment. An older firmware cannot necessarily interpret newer two-bank config records, so re-verify meter settings after any deliberate downgrade. Handle damaged history with backups, a controlled review and an explicit service procedure, not destructive automatic rotation.

Software CI verifies native policies and binary compilation, not electrical supply, Wi-Fi coverage, 802.1X policy, real TLS authorization or end-to-end factory durability.

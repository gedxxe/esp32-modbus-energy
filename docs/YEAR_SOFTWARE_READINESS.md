# One-year firmware software reliability review

**English** · [Bahasa Indonesia](id/YEAR_SOFTWARE_READINESS.md) · [Fleet readiness](FLEET_READINESS.md) · [Enterprise Wi-Fi](ENTERPRISE_WIFI.md)

## Scope

Software engineering and source-level validation for continuous operation over 365 days. Physical power integrity, electrical/RF timing, meter-LCD agreement and onsite acceptance belong to the device/hardware owner. A one-year simulated timeline checks arithmetic and policy rules; it is **not** an elapsed 365-day endurance measurement.

## Implemented safeguards and current checks

| Subsystem | Software behavior | Verification |
| --- | --- | --- |
| Enterprise WPA2/802.1X | Explicit `STA.begin(false)`, clean station configuration (no stale BSSID/channel/RSSI lock), verify readback and EAP-PEAP/MSCHAPv2 before the only `esp_wifi_connect()`; built-in public CA bundle when no custom CA is selected | Actual production adapter compiled with fault-injection stubs; one-connection call-order assertions |
| Millisecond uptime | Network retry, preparing, connection and DHCP deadlines use modular unsigned subtraction; HTTPS long-term backoffs use 64-bit monotonic uptime | Synthetic 365-day Wi-Fi retry timeline and existing 64-bit HTTPS native regression |
| File retention | Exact canonical current-day and 30 previous days; preserve quarantine/forensic names; never mass-delete on RTC jump | Native 365-day daily-rollover test, bounded 31-day canonical set |
| Flash write frequency | Verified unchanged NVS/CSV snapshots skip identical writes, but corrupt/short/missing copies are rewritten through the recovery path | PR #22 native fault tests and merged CI |
| Durable captures | Ten immutable scheduled WIB points, atomic per-record schema/CRC, staging and retained backups | Existing corruption, restart and generation tests |
| HTTPS | Backoff, token binding and frozen immutable checkpoint replay; cloud ACK cursor advance only after verified server receipt and persisted cursor | Existing IoT native tests and separate draft PR #23 for transient live retry expiry |
| LoRa | Optional protocol v1, disabled by default, no radio/Modbus changes in Enterprise PR | Unmodified native radio tests and protocol fixtures |

## Remaining software limitations requiring separate changes

1. **Management surface authentication:** the local HTTP control API returns its write-authorizing token through an unauthenticated status endpoint. The fixed recovery AP password is part of public firmware source. These need a migration-safe authenticated control channel and unique provisioned AP secrets before unrestricted corporate LAN exposure.
2. **Private RADIUS trust:** public bundled roots cannot validate an unknown private corporate CA. Windows devices may receive enterprise CA roots and server DNS policies from MDM/Group Policy. The ESP32 needs an IT-approved trust anchor; neither automatic acceptance of an arbitrary certificate nor disabled date/chain checks is acceptable. The supported method is PEAP/MSCHAPv2, not every 802.1X/EAP profile.
3. **Password rejection policy:** disconnect reasons 23/202 currently stop automatic Enterprise retries until credentials are saved again. This protects against RADIUS account lockout, but a temporary backend rejection can require operator recovery. A secure bounded retry policy requires a distinct design from transient radio reconnects.
4. **Local history horizon:** the logger retains 31 days of canonical local archives, **not 365 days**. A year of historical data requires independently durable cloud ingestion, external backups and monitored gap reconciliation. A missed day with no archive is not automatically provable as “powered off.”
5. **Calendar gaps and filesystem growth:** retention retires one exact day per rollover. Long offline jumps intentionally avoid bulk deletion when the RTC may be wrong; old canonical records and forensic quarantines may accumulate, eventually reaching the bounded recovery-scan limits. Provide authenticated and audited operator reconciliation instead of automatic sweeping.
6. **Maintenance and credentials:** the public CA bundle and provisioned keys do not rotate automatically; plan a signed/trusted firmware update and private CA/key rotation before expiry. Stock NVS does not by itself guarantee encryption.
7. **Resource observability:** compile-time heap/flash percentages do not prove a full year of free heap, stack and memory-fragmentation health. Preserve existing task watchdog and HTTPS heap diagnostics and define software-defined alert thresholds in a dedicated future change.

## Software exit criteria for a one-year deployment candidate

- CI passes from the **exact commit** on default and HTTPS-enabled ESP32-S3 builds, standalone read-only diagnostic, native Modbus/RTC/LoRa/storage/network tests, browser regressions and documentation checks.
- Host-based fault tests demonstrate authentication failure/unknown CA refusal, no pre-EAP association, clock correction boundaries, 32-bit rollover, replay idempotency, archive corruption, and long reconnect cycles. Do not equate mocks with physical acceptance.
- Administrative control plane is isolated until authenticated access is deployed. An operator can distinguish Wi-Fi association, 802.1X authentication, DHCP, heartbeat and actual accepted kWh checkpoints; a green heartbeat is not a reading guarantee.
- Ensure 365-day archival integrity through the cloud's backup/reconciliation policy. The local retention window is a recovery cache, not a full-year archive.

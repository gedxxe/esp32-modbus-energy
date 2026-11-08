# Reliability remediation phase 2 — 2026-11-08

## Baseline and scope

Baseline is merged PR #25 (main commit `3d16f9ee827cd1fcbd28a7d019f5f198392ceb36`). Scope is **software/firmware only**: no changes to electronics, wiring, Modbus register map, calibration, 10-checkpoint calendar, LoRa frame layout, HTTPS ingest JSON or the website repository.

This is a compatibility-preserving **staged remediation**, not evidence of enterprise-production readiness or a five-year uptime certification.

## Implemented in this draft

| Roadmap | Change | Operational benefit | Compatibility constraint |
| --- | --- | --- | --- |
| P1: WLAN retry/fleet storms | `NetworkPolicy` now uses capped exponential backoff after consecutive attempts, with seed-based 0–10-second jitter. Base reconnect interval remains 30 seconds for Personal and 60 seconds for PEAP; upper bound is 300 seconds. On verified ONLINE state the retry streak resets, and a new outage starts a new base interval. The API exposes current streak/wait/remaining time. | Avoid synchronized attempts after large-scale SSID/AP outage and reduce repeated auth/association load while retaining self-healing. | Existing AP recovery fallback and `millis()` rollover-safe unsigned deadlines remain; failed authentication remains paused for separate credential recovery. |
| P0 staged: PEAP identity | Detect an empty RADIUS server certificate name for an active or staged PEAP configuration; expose `radiusNameCheckConfigured`, `activeRadiusNameCheckConfigured` and `radiusIdentityWarning`, plus a boot/network-change diagnostic log. | Makes the previously silent missing server-name check explicitly observable. | **No silent guessed domain, no default trust bypass, no migration that disconnects existing clients.** This patch **does not fix** missing RADIUS name binding; that remains a separate P0 hardening step requiring IT-approved name and UI/server provisioning migration. |
| P1: HTTPS worker progress | Publish worker monotonic progress every 5 seconds through its existing one-slot status queue. Surface `workerPulseSeen` and `workerPulseAgeMs` in `/api/status`. | Detect a stalled HTTPS worker separately from a network outage or rejected API heartbeat. | No added network traffic, no heartbeat retry side effects, no change to capture ACK cursor or upload schedule. |
| P1: event queue observability | Add an aggregate `eventQueueOverflowCount` for WiFi callbacks. | Diagnose event floods without altering existing overflow-recovery policy. | No change to WiFi queue capacity, credentials, or site provisioning. |
| P1: delayed WiFi callbacks | Keep the SSID from `STA_DISCONNECTED` and ignore a delayed event explicitly naming a **different** SSID. Discard `LOST_IP` before the current attempt has reached `ONLINE`, while preserving the bounded DHCP timeout. | Avoid a known class of late cross-network callbacks aborting a replacement connection attempt. | Does not solve same-SSID/stale-generation ambiguity; a deterministic event-sequence harness is still needed. |

## Verification obligations

- Run native `test_network.cpp` regressions: seeded device jitter differs, bounded backoff, withheld retry during scan, success resets streak, immediate first budget on new outage, and time wrap.
- Run standard CI: native tests, documentation checks, UI tests, full ESP32-S3 firmware, standalone debug sketch and opt-in HTTPS synthetic-credential build.
- On a **staging** network, validate SSID disappearance, AP roam, 802.1X RADIUS outage and recovery, DHCP renewal, DNS and gateway loss, portal/proxy response hijack, temporary website 503 and server 429; confirm no unverified HTTP body is treated as ACK.
- Observe `workerPulseAgeMs` under normal HTTPS timeout (6s) and deliberate injected network stall. The liveness field reports task scheduling, **not** successful authenticated server contact.
- Verify staging website receives unchanged schema v1 `active_energy_kwh` and optional `current_a`, 600-second periodic telemetry, 60-second heartbeat, immutable checkpoint event IDs, and 10-second manual refresh cooldown.
- Soak 7 and 30 days and capture internal-heap largest-free-block, task stack high-water, reset reasons and oldest unacknowledged archive day. No unplanned reset, loss of durable checkpoint or permanently stopped uplink.

## Pending prioritized roadmap

1. **P0—Enterprise server identity enforcement:** determine certified RADIUS DNS name(s) and organizational CA policy with IT; update provisioning UX/validation and introduce explicit safe migration of pre-existing PEAP profiles. Never silently invalidate existing saved credentials. `radiusIdentityWarning` remains set until corrected.
2. **P0—Website staged integration:** use valid test credentials and verify signed HTTPS receipts; no public CI can prove access to production secrets.
3. **P1—Reordered WiFi callbacks:** the different-SSID and premature LOST_IP cases have a conservative filter, but **same-SSID and same-AP stale-generation callbacks remain unproven**. Build a deterministic mocked event-loop harness with association-generation linkage and replay scenarios. Timestamp-only filtering is not a sufficient proof of absence of races.
4. **P1—HTTP server watchdog and memory:** inject slow clients, bounded TLS timeouts, repeated failed DNS lookups and long network outages while monitoring request latency and heap fragmentation.
5. **P1—Archive reconciliation:** alert on >30-day gap without forging or silently advancing the durable ACK cursor.
6. **P2—LoRa IRQ/task synchronization:** replace unqualified volatile flag coordination with a proven ISR-safe notification strategy, after protocol/ACK regression.
7. **P3—Incremental modularization:** isolate HTTP network/UI policy, recorder, and persistence while preserving existing queue ownership.

## Change classification

- **Source-code regression tested:** retry policy and name-check predicate via native `test_network.cpp`.
- **Needs firmware build:** new status fields and worker pulse.
- **Needs deployment tests:** managed WLAN operation, WPA3/EAP-TLS, DHCP/DNS recovery, HTTPS website acceptance, memory and watchdog soak.

Website repository `gedxxe/Midea-JRE-UNILAND-Waste-Calculator` is unchanged.

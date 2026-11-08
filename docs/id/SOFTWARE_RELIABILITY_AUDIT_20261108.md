# Software / firmware reliability audit — 2026-11-08

Scope: **software and firmware only**. Audit target: `gedxxe/esp32-modbus-energy` main at the branch point, plus the **read-only** ingest contract of `gedxxe/Midea-JRE-UNILAND-Waste-Calculator`. No hardware analysis is included. Static review and native unit/CI tests cannot certify multi-year uptime or real enterprise WLAN behavior.

## Executive Summary

**Provisional production-readiness score: 58/100** (static evidence-based assessment; **not** an operational uptime certification). The most concrete confirmed software failure mode was an HTTPS uploader that entered terminal phase 4 on ambiguous HTTP/JSON responses. Because `worker()` skips all further HTTPS traffic in phase 4, a temporary intermediary response could require a reboot to recover. This draft patch changes *only the recovery classification* for non-confirmed application responses. Authenticated server rejections remain terminal; TLS validation, redirect refusal, immutable event IDs, ACK cursor semantics and ingest JSON remain intact.

The main development model is Arduino on the pinned ESP-IDF-based platform, **not** an application fully migrated to native ESP-IDF. Therefore native ESP-IDF practices must be assessed as design goals rather than claimed compliance.

## Critical Findings

| ID | Severity | Evidence and root cause | Operational impact | Proposed remediation / disposition |
| --- | --- | --- | --- | --- |
| F-01 | **Critical** | `firmware/src/iot_uplink.cpp`: `transmitHeartbeat()` treated an HTTP 200 with a wrong/HTML body, intermediary 401/403, redirects/404 and the legitimate 503 feature-off response as terminal `phase=4`. `transmit()` similarly blocked on mismatched 200/201 receipts and some 4xx/redirects. `worker()` (near lines 595–650 of the base) skips work on `phase==4`. | Transient captive portal, reverse proxy, WAF or deployment cutover can suppress heartbeat, telemetry and archive uploads indefinitely until restart. | **Patched in this draft:** classify documented website ingest rejection bodies as terminal, otherwise back off with jitter and retry the same immutable event. Never follow 3xx redirects or accept malformed ACKs. Native regression covers the website's exact codes. |
| F-02 | **High / validation gap** | `main.cpp`: synchronous `server.handleClient()` runs before `tickNetwork()` inside an 8-second watchdog loop; bounded socket *write* helps, but no whole-request latency/fault-injection acceptance test is present. | Under slow/hostile HTTP requests, network event consumption and UI may be delayed. A watchdog reset is plausible but **not proven** from static inspection. | Add bounded request-read tests and on-device latency/stack/high-water telemetry; force slow/half-open clients and show WDT never trips. Do not change the server without reproducing the stall. |
| F-03 | **High / deployment limitation** | `enterprise_station.cpp` implements `NETWORK_PEAP` with `ESP_EAP_TYPE_PEAP` and minimum `WIFI_AUTH_WPA2_ENTERPRISE`; no native EAP-TLS or WPA3-Enterprise mode is exposed. | Not a drop-in client for every large corporate WLAN configuration. | Implement and test additional EAP methods only after obtaining site policy and pinned-SDK support; verify certificate name, CA chain, renewal and AP handover. Do not silently relax EAP verification. |

## WiFi & Enterprise Network Findings

| ID | Severity | Root cause / code location | Operational effect and remedy |
| --- | --- | --- | --- |
| W-01 | High | `main.cpp` lines ~168–216 use a 16-event `networkEventQueue`, attempt timestamps and recoverable overflow; no attempt identifier is attached to asynchronous driver events. | During rapid reconfiguration, queued events associated with old attempts may be misclassified if timestamps alone cannot disambiguate them; this is a **hypothesis requiring event-sequence fault injection**. Prefer explicit connection generation semantics and tests that replay delayed callbacks. |
| W-02 | Medium | `network_settings.h` `NetworkPolicy` uses fixed 30/60-second retry cadence; there is no per-device exponential backoff/jitter for WLAN association. | Many synchronized clients can retry together after an outage. Add capped exponential jitter without changing device identity or AP fallback, with rollover tests. |
| W-03 | Medium | `main.cpp` only decides readiness from local association and nonzero IPv4; the uplink separately discovers DNS/HTTPS reachability on each request. | A station can appear online with broken DNS, gateway routing or web access. Surface *distinct* link/IP/DNS/HTTPS/API receipt status; maintain existing heartbeat semantics. |
| W-04 | Medium | `EnterpriseWiFi::disconnected` pauses retries on auth reasons 23/202. This is a deliberate response to suspected credential rejection, not evidence of a software deadlock. | Credential or RADIUS policy changes require a managed recovery/reprovisioning path. Keep paused diagnostics visible; avoid infinite brute-force retries. |
| W-05 | Medium / unverified | WPA3-Enterprise and roaming-specific tests are absent. AP switching depends on underlying SDK behavior and event callbacks; no packet-loss/DHCP-renewal/SSID-disappearance soak result was available. | Validate WPA2-Enterprise PEAP on actual managed WLAN; test roam, DHCP lease renewal, temporary DNS failure, captive portal and gateway outage without accepting spoofed HTTP success. |

## Reliability Findings

| ID | Severity | Mechanism / impact | Mitigation |
| --- | --- | --- | --- |
| R-01 | High | `iot_uplink.cpp` archives a checkpoint only after a verified HTTP receipt and durable dual-bank cursor; a corrupted cursor pauses archive replay. The `BACKFILL_DAYS=30` guard may require manual reconciliation after a prolonged outage. This is data-protective by design, but can leave backlog unreconciled. | Provide explicit authenticated archive reconciliation workflow, metric for oldest unsent day and an operator alarm. Never auto-increment ACK cursors. |
| R-02 | Medium | `worker()` coalesces its input with a one-slot FreeRTOS queue, and live snapshots expire after one telemetry period. Historical checkpoints are durable; **intermediate live samples are not**. | Document expected offline gaps on the dashboard. If lossless telemetry is required, add a separate bounded durable queue with quota/retention and server idempotency; do not confuse it with checkpoints. |
| R-03 | Medium | `lora_service.cpp` uses ISR-written `volatile bool irqPending` read/cleared by a task. Volatile alone is not a formal cross-context synchronization primitive. | Prefer ISR-safe task notification or demonstrably lock-free atomics with tests. Confirm that no pending interrupt is lost when clearing the flag. **Not modified in this patch.** |
| R-04 | Medium | Many fixed-size queues and large `Snapshot`, diagnostic and archive buffers are allocated at boot; allocation failures cause a controlled reboot. There is no documented fleet-level evidence for worst-case internal heap, largest contiguous block, task stack or TLS churn over months. | Add minimum-free, minimum-largest-block and high-water instrumentation, plus soak with reconnect and repeated HTTP sessions. Separate PSRAM from internal-heap budget. |
| R-05 | Medium | WiFi, LoRa, RTC, capture and HTTPS have separate recovery loops and error states. End-to-end progress health is inferred, not validated by a periodic assertion across components. | Track last successful Modbus energy sample, last durable checkpoint, last HTTPS application ACK, oldest unacked day and reconnect streak independently. Expose status without secrets. |
| R-06 | Low | The HTTPS feature is compile-time opt-in using private `iot_credentials.local.h`; default builds correctly may have no website uplink. | Production release gate must verify opt-in enabled, endpoint correct, provisioned device binding and real ingest ACK. Never interpret a successful generic firmware build as proof of website connectivity. |

## Maintainability Findings

- **High:** `firmware/src/main.cpp` is approximately 2,117 lines and contains multiple subsystems and HTTP handlers. `firmware/include/web_ui.h` and `firmware/src/oled_frames.cpp` are large generated assets. Keep generated assets separate from business logic, extract narrow network/command/HTTP interfaces, and enforce ownership of cross-core state.
- **Medium:** Error strings/status phases are currently spread across the transport worker, network state machine and UI. Replace ad-hoc interpretation with typed, testable result classification and an explicit retry/permanent-reject matrix. F-01 introduces a small, pure contract predicate rather than changing the wire format.
- **Medium:** Repeated `String`, `JsonDocument`, TLS requests and WebServer processing have no proven long-run allocation high-water envelope. Do not declare a leak without heap traces; add heap-fragmentation trend benchmarks.
- **Medium:** Production observability relies on Serial output, in-memory diagnostics and status snapshots; retained fault events/boot-reason trend and recovery counters need an explicit, bounded diagnostic store.

## Long-Term Stability Risks (1–5 years)

Reboots during mid-flight HTTPS, `millis()` wraps, multi-day offline replay, NVS/FS cursor corruption, repeated EAP auth failures, lost DHCP/DNS, wrong system time, 429 rate limiting, stale device credentials, proxy-generated HTTP 200 HTML and overloaded HTTP clients should all be exercised in a **fault matrix**. Existing native tests cover many pure clock, cursor, contract and recovery predicates; they do **not** establish memory leak absence or 24/7 reliability.

MQTT: **not part of the reviewed website uplink architecture**. This client uses ESP-IDF HTTPS. MQTT lockup/reconnect is therefore *not applicable* to the current ingest path; an independent MQTT integration would require a separate audit.

## Recommended Refactoring

1. Introduce a transport-state event reducer with typed outcomes: `ConfirmedAccepted`, `ConfirmedRejected`, `RetryableHTTP`, `RetryableTransport`, `RateLimited` and `Unavailable`. Keep the current immutable POST and idempotent event IDs.
2. Split network policy from Arduino station driver and test delayed/misordered STA/DHCP events, reconnect generation, roam, AP disappearance, rate limit and randomized retry scheduling.
3. Build a software-only chaos harness with HTTPS mock server, delayed/truncated acknowledgements, EOF during body, proxy redirects, 401/403 HTML, 503 maintenance, DNS timeout, malformed JSON and long disconnects.
4. Add low-overhead failure counters and worst-case heap/stack snapshots to existing API status; design bounded wear-aware persistence for fault history only if operationally required.
5. Extract `main.cpp` subsystems incrementally with identical observable behavior; preserve cross-core queue ownership and Modbus/profile code.

## Production Readiness Score (0–100)

| Dimension | Score | Maximum |
| --- | ---: | ---: |
| WiFi / enterprise resilience | 10 | 25 |
| Data integrity / recovery | 19 | 25 |
| Task, memory and concurrency confidence | 11 | 20 |
| Observability and maintainability | 11 | 20 |
| Test and operational validation | 7 | 10 |
| **Total (provisional)** | **58** | **100** |

Score is a risk-weighted engineering assessment, **not** an objective benchmark. Absence of on-device soak data caps confidence; completing F-01 alone does not imply certification.

## Prioritized Remediation Roadmap

| Priority | Task | Acceptance criteria |
| --- | --- | --- |
| P0 | F-01: transient HTTPS response recovery (**this draft**) | Retry ambiguous 200/201, 401/403, malformed-body 4xx, redirect/404 and 503 with bounded backoff; never follow redirect or ACK bad event IDs; known website rejections still stop. Pass native regressions and opt-in firmware build. |
| P0 | Contract deployment check against website | Server still accepts the exact schema-v1 heartbeat, `active_energy_kwh`, optional `current_a`, checkpoint event IDs; telemetry remains 600s, heartbeat 60s, web manual cooldown 10s. Verify in staging with valid device credentials; read-only code comparison is not live proof. |
| P1 | Managed WLAN fault-injection matrix | Recover from DNS loss, DHCP renewal, AP handover, gateway outage and SSID disappearance with bounded retries; zero credential leaks and no unverified TLS bypass. |
| P1 | HTTP/WDT/heap soak tests | 7-day pilot and 30-day soak with baseline and peak memory, stack high-water, resets, WDT logs, uptime and telemetry acceptance. No unplanned restart or unbounded retained allocations. |
| P1 | Archive reconciliation and gap alerts | Detect cursor backlog >30 days and alert; operator can safely export and reconcile without modifying validated history. |
| P2 | Event generation and fleet jitter | Reordered WiFi events cannot poison a new attempt; reconnect storms bounded across fleet. |
| P2 | FreeRTOS and LoRa IRQ coordination | ISR/task notification is race-safe; HMAC/replay/ACK semantics unchanged. |
| P3 | Incremental modularization and logs | Unit boundaries, diagnostic event taxonomy and regression coverage enforced by CI. |

## Verification and boundaries

- **Read-only website integration:** `iot-contract.js`, `server/iot.js`, `server/http.js` inspected; **no website source changed**.
- **Patch touches only:** `firmware/include/iot_contract.h`, `firmware/src/iot_uplink.cpp`, `firmware/test/test_iot_contract.cpp`, this audit report.
- **No changes** to Modbus register maps, meter-specific kWh scaling, LoRa wire protocol, device identity, local clocks, checkpoint schedule, archive schema, upload payload, bearer storage or website.
- **Not proven in this audit:** successful real ESP32 compilation until CI completes; on-device managed enterprise WLAN/roaming; long-duration memory stability; HTTPS staging acceptances; permanent absence of all bugs.

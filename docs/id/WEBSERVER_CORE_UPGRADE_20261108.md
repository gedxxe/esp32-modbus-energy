# P1 software reliability — hardened Arduino WebServer core

**Baseline:** main `2169021b6681abc16a8676ee0a9e73135086c25a` (after merged PR #26). **Change:** only the pinned PlatformIO framework platform in `firmware/platformio.ini`; website and firmware application source remain unchanged.

## Confirmed dependency versions

- Before: pioarduino `55.03.311` → Arduino-ESP32 **3.3.11**, ESP-IDF **5.5.5**.
- After: pioarduino `55.03.312-1` → Arduino-ESP32 **3.3.12**, ESP-IDF **5.5.5**.
- The `-1` tag fixes SCons 4.11 compatibility compared with `55.03.312` without other release changes.
- Arduino-ESP32 3.3.12 release explicitly incorporates upstream `fix(webserver): Harden security and fix hangs` (espressif/arduino-esp32#12794), including bounded request/header reads rather than indefinite per-byte stream waits.

References:
- https://github.com/espressif/arduino-esp32/releases/tag/3.3.12
- https://github.com/espressif/arduino-esp32/pull/12794
- https://github.com/espressif/arduino-esp32/issues/12788
- https://github.com/pioarduino/platform-espressif32/releases/tag/55.03.312-1

## Why isolated?

The existing `server.handleClient()` is synchronous and runs in the same `loopTask` as `tickNetwork()`. A partial HTTP header streamed one byte at a time can keep old `WebServer::_parseRequest()` inside a read loop, delaying WiFi retries and tripping the task watchdog. Adding `millis()` measurements *after* `handleClient()` cannot preempt a stuck parser. Upgrading to a version with a bounded parser is the primary upstream fix.

A runtime upgrade is operationally more risky than pure reconnect/IoT contract fixes; therefore the core pin is isolated in its own draft PR for independent rollback.

## CI acceptance gates

- Native unit tests (network, IoT v1, checkpoint/archive, LoRa, diagnostics) unchanged.
- Full ESP32-S3 N16R8 PlatformIO build and opt-in synthetic HTTPS variant **must pass** on the upgraded platform.
- No changes in partition layout, flash image generation, NVS record schema or committed UI bundle; generated HTML must remain byte-identical.
- Browser regression unchanged and passing.
- Manual ESP32-S3 staging tests: HTTP GET/POST with clean headers, valid multipart CA upload, bad/malformed headers, partial/slow request and multi-header timeout; verify `loopTask` remains responsive and no WDT reset.
- Manual WPA2-Personal/PEAP and multi-AP reconnect; compare event callbacks, DHCP, cert-chain/name matching, 10-minute telemetry, 60-second heartbeat, immutable checkpoint ACK and archived retransmission.
- Heap/stack baselines and 7-day staging soak must remain comparable; no production release based solely on successful compile.

## Rollback

If regressions are observed in staging, revert the *single* `platform` URL to `55.03.311` **only as a temporary operational rollback** while preserving the open WebServer vulnerability status and restricted exposure. Do not silently declare 3.3.11 secure.

## Remaining roadmap

- IT-approved RADIUS certificate name and staged provisioning UI rollout (separate PEAP PR #27).
- Full no-loss website staging fault injection, DNS/DHCP/HTTP failures, same-SSID stale-event harness, ISR/task LoRa sync.
- Production release only after field/soak evidence and security review.

No merge performed.

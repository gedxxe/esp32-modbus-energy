# Implementation review for version 2.3.3

**English** · [Bahasa Indonesia](id/AUDIT.md)

The user reported a successful ModbusMaster sketch using a manual transceiver with DIR16. The previous installation default selected automatic direction, and its custom receiver depended on task-observed gaps to finish a frame. V2 changes both the circuit default and transaction engine to follow the working reference. This is a design revision based on that evidence, not an isolated measurement proving that only one difference caused every earlier failure.

## Transport and shared source

Normal acquisition, scanner probes and the standalone INO now include the same FortMeter library. ModbusMaster constructs the request and collects its expected response length, with manual pre/post callbacks matching the reference. The adapter records actual I/O, enforces capture/drain limits and independently checks the complete FC03 shape plus CRC, including exceptions. Echo and extra bytes are not silently removed to fit a desired result.

The upstream source is pinned at v2.0.1 and the small service-hook patch is retained alongside source hashes and licenses. Native tests compile that real source with the actual standalone INO or shared diagnostic class, so they do not substitute a mock implementation of ModbusMaster. Legacy include/generator paths remain as checked compatibility entry points without another active parser.

## Acquisition, migration and interface

The first contact query is the manual's 0x0058/two-word current request. A valid exception preserves protocol contact while leaving that value missing, allowing other profile reads to continue. The secondary counter is available without valid ratios, whereas production intervals still require validated CT/PT applied once and accepted time/continuity. Primary float ordering stays an explicit comparison rather than an automatic fallback.

CRC-valid v1 connection records migrate to manual direction while preserving serial settings. Day and LED records keep their formats, completed intervals remain protected, and filesystem formatting still requires physical confirmation. The web exposes direction and direct register readings, keeps stop/resume independent of ordinary command waits and validates rows before rendering them. Wiring and technical prose have been rewritten around the confirmed manual installation.

## Web integration and Wi-Fi

The local application now exposes session settings, evidence counters, structured raw reads, trial-setting controls and the primary-float command alongside the existing scanner, trace, flow and process views. AP/client/dual Wi-Fi configuration follows a separate core 1 persistence and core 0 driver pipeline, with credential validation, deferred application and recovery AP behavior. Header/OLED addresses now follow the actual interface rather than a fixed AP address. Capture comparisons also reject changed direction and settling/gap timing. The [web guide](WEB.md) maps every registered API route to its interface and owner.

## Limits that remain

UART timestamps represent software observations rather than measured t1.5/t3.5 or A/B propagation. A Modbus response has no transaction ID or register address, so every delayed reply cannot be attributed conclusively from bytes alone. The interface exposes no module-presence/power-good input, and silence, CRC failures or repeated local RX cannot certify a physical component's health.

Interval interpolation is an estimate limited to accepted 15-second gaps, and counter resolution depends on CT/PT. Backup/checkpoint tests cover defined failure paths rather than every flash or power-cut outcome. AP/HTTP has no separate login/TLS, cloud service, NTP or OTA path, and its upstream request parser still needs on-device load evaluation. [VALIDATION](VALIDATION.md) records actual software results, while [COMMISSIONING](COMMISSIONING.md) identifies the separate integrated hardware checks.

The 2.2 integration also adds asynchronous Wi-Fi scanning, OLED network editors, a migrated custom-IP record and a persisted idle-animation policy. The reference animation is converted losslessly into a bounded decoder and no longer initializes its own display or enters a separate loop. Wake input is consumed before ordinary button dispatch. Clock animation is isolated from RTC mutation, and API pages retain their sequence/quality validation with reduced event response size. Radio timing, physical menu operation and sustained animation under concurrent clients still require on-device checks.

The 2.2.1 correction replaces the Wi-Fi wrapper start/completion policy, serializes browser API bodies, bounds speculative-socket waiting and adds driver reason/code visibility. The communication view now retains engineering milestones and raw frames with event-driven directional labels. Browser regressions cover lost acknowledgements, delayed reads, radio failures, CRC and timeout without fabricated RX, while the actual scheduler is tested for body completion, cancellation and priority Stop. The radio and HTTP changes still require on-device acceptance.

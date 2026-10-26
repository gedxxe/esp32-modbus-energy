# Implementation review notes

**English** · [Bahasa Indonesia](id/AUDIT.md)

This is a record of specific changes and remaining limits, not an assertion that the installation is production-qualified. Current checks and build sizes are in [VALIDATION](VALIDATION.md).

## 1.2.0 — browser and documentation

- Replaced the inline dashboard with maintainable React sources and genuine pinned React Bits FadeContent/SpotlightCard dependencies.
- Added English-first UI, persisted Indonesian selection, translated action/error codes, and language-neutral quality codes. Kept the legacy status quality field for compatibility.
- Embedded local gzip HTML/JS/CSS/notices in flash, enforced a 160 KiB payload budget, and added source-hash checks before firmware builds.
- Sent assets through bounded nonblocking lwIP chunks with scheduler yields/watchdog feeds, avoiding a whole-bundle heap String.
- Rewrote the main README and English technical guides, linked Indonesian editions, and documented boot order, task ownership, queues, lock bounds and RS485 timing.
- Retained GPIO assignments, meter registers/scales, NVS record layouts, schedules and the physical-only format procedure.

## 1.1.0 — meter scan

Added manual range-limited scanning, progress/cancel/apply commands and separate diagnostics. Scan reads energy and CT/PT only, pauses acquisition with interrupted interval quality, restores the previous settings and requires explicit application of a result. Enumeration/state changes are tested without a UART; physical detection/turnaround still needs commissioning.

## 1.0.1 — recovery and documentation

The initial review added the confirmed Fort terminal map, pin-level wiring/netlist, AGENTS.md, locked library versions, local SVGs, link checks and CI. It also fixed backup restoration so a failed restore cannot delete the recoverable backup, limited failed CT/PT retry rate to five seconds, and fed the watchdog per Modbus attempt. Fault-injection/native tests cover those logic paths.

The scanned manual's request CRC is correct; the printed response CRC is not. Firmware still rejects invalid CRC. No register address, scaling or protocol exception was changed to accept the typo.

## Remaining implementation limits

- UART byte timestamps come from draining a driver buffer; exact t1.5 validation is not claimed.
- Boundary interpolation across at most 15 seconds is an estimate. Counter resolution depends on CT/PT, not displayed decimal places.
- Counter reset and rollover are not distinguished; both break continuity.
- NVS CRC/checkpoints and CSV backups cover defined recovery paths, not every physical flash/power-loss failure. There is no second app-level checkpoint journal or CSV-to-tracker reconstruction.
- AP/HTTP access has no separate login/TLS, cloud, captive DNS, NTP or OTA path. Phone time sync is explicit.
- Synchronous HTTP can delay UI drawing, including during a bounded asset transfer. Meter acquisition remains in its own core-1 task; target periods are not hard deadlines.
- No software test establishes isolation, cable/noise tolerance, actual Fort scaling or full hardware endurance. Use [COMMISSIONING](COMMISSIONING.md).

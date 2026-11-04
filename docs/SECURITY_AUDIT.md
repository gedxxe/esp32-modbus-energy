# Repository security and ADL400 evidence audit

**Date:** 2026-11-05 · **Scope:** tracked Git source, documentation, generated web assets, stored sketches, tests and binary-file bytes at the audited tree. A repository scan is not a hardware test or an incident-response forensic review.

## Confirmed finding and remediation

The original factory recovery AP password was a shared compile-time value in `firmware/include/board.h` and repeated in nine tracked source/documentation files. Assume it was publicly exposed; do not reuse it on any system. Firmware now generates 128 random bits on each device's first boot, renders them as a 32-character WPA2 password and persists them in a separate `apauth` NVS namespace. If NVS is unavailable, the current boot uses a unique ephemeral password rather than a shared default. The password is displayed through the physical **OLED → NETWORK / WIFI → AP password** menu, not logged or returned by the status API. Upgrading a unit that has only the old common password necessarily changes its recovery AP password. Reconnect with the OLED value.

A password that once appeared in Git history **remains in old commits, clones, caches and forks** even after removing it from the current tree. This patch does not rewrite history; that would require a separate coordinated decision. Rotate all potentially reused credentials. Devices on older firmware remain exposed until upgraded.

## Automated safeguards

`python tools/security_audit.py` scans every Git-tracked file (including readable byte sequences in binaries) for recognized provider token formats, private-key headers, basic/bearer literals, embedded AP/PSK/password assignments and obvious passwords in Markdown. It rejects sensitive file names such as private keys and tracked `.env` files. `.gitignore` excludes common local keys, .env files, build outputs, logs, temporary data and provisioning files. CI runs the scan and unit tests for unique AP password generation, reboot persistence, invalid-record rotation and NVS-write failure.

Run locally:

```sh
python tools/security_audit.py --self-test
python tools/security_audit.py
```

**Limits:** A regex scanner cannot certify the absence of every secret. Binary screenshots, compressed PDF streams, embedded metadata, old Git objects, GitHub Actions logs, branch protection, local untracked files and deployed ESP32 NVS contents require separate inspection. The scanner deliberately never echoes matching secret values.

## Remaining security constraints

The ESP32 dashboard uses plain HTTP on the local AP and client network, without TLS or per-user login. Its per-boot mutation token is not a replacement for strong network access control. Do not publish or port-forward the device's HTTP endpoint to the internet; isolate the network appropriately. Client Wi-Fi and PEAP passwords are runtime configuration saved on the device, not source-code constants. Device flash extraction and transport security are separate risks not solved by this repository cleanup.

## ADL400 read-path verification

The independent field sketch `docs/references/adl400-energy-field-reference.ino.txt` reads with Modbus FC03, slave ID 220, 9600 baud, 8N1, TX17/RX18/DIR16, and validates CRC. The normal production `acrel-adl400` profile uses the same physical register map:

| Meaning | FC03 address / words | Interpretation |
| --- | --- | --- |
| Total active energy | `0x0000 / 2` | UInt32 ABCD, divided by 100 |
| Forward active energy (comparison only) | `0x000A / 2` | UInt32 ABCD |
| Reverse active energy (comparison only) | `0x0014 / 2` | UInt32 ABCD |
| Total reactive energy (comparison only) | `0x001E / 2` | UInt32 ABCD |
| PT ratio | `0x008D / 1` | Independent unsigned register |
| CT ratio | `0x008E / 1` | Independent unsigned register |

Field-observed raw 9,798 with PT 1 and CT 80 gives **9,798 / 100 × 1 × 80 = 7,838.40 kWh**. The separately observed LCD was approximately 7,838.9 kWh, a difference of 0.5 kWh, smaller than one 0.8-kWh raw step at CT=80. That is consistent with quantization and sample timing, but does not alone prove calibration or that the compared LCD page showed the identical counter.

The production path uses `FortTransport::readRawFC03` (CRC-valid RTU frames, bounded timeout, echo/noise handling), `MeterReadings` and `profiles::primaryEnergy`, then acquisition filtering and checkpoints. PT/CT are read as separate requests and required before scaling; no Modbus write command is sent. Only energy is required for an ADL400 sample, so optional float electrical measurements cannot invalidate the cumulative energy sample. Host regression replays the supplied raw value and adverse frames; compiled ESP32 and browser tests give integration evidence but **cannot replace a real ADL400-on-firmware test**. The slave ID and serial format must match the installed meter.

## Maintenance boundary

Main acquisition, saved-device registry, RTU transport, meter profiles, web UI and diagnostics remain separate units. This audit adds the AP credential lifecycle and a tracked-file scanner without changing the Fort/Habuer meter protocol, the ADL400 register map, checkpoint storage schema or existing read-only diagnostics. Large generated OLED/web assets remain intentionally tracked for reproducible builds; they are not a place to store local settings or credentials.

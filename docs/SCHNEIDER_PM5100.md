# Schneider PM5110: D+R active-energy acquisition

[English](SCHNEIDER_PM5100.md) · [Bahasa Indonesia](id/SCHNEIDER_PM5100.md)

## Proven on the physical meter (6 November 2026)

The owner tested a **Schneider Electric PM5110**, product ID `15270` (`0x3BA6`), with an ESP32-S3 using manual RS485 direction (GPIO17 TX, GPIO18 RX, GPIO16 DE and /RE). Slave **1**, **19200 baud**, **8E1**, Modbus RTU **FC03** returned correctly framed, CRC-validated replies. No Modbus writes are needed.

The field requirement is **active energy D+R (Delivered + Received)** as the **primary recorded kWh**. This is *not* reactive energy (kvarh) and *not* the legacy Delivered-only (D) counter. A CRC-valid read returned:

| Counter | Schneider register (one-based) | PDU decimal / hex | Words / encoding | Hardware result |
| --- | ---: | --- | --- | ---: |
| **Active energy D+R: production kWh** | **2704** | **2703 / `0x0A8F`** | **2 / IEEE754 Float32 ABCD** | **412691.00000 kWh** |
| Active energy Delivered-only: archived source | 2700 | 2699 / `0x0A8B` | 2 / Float32 ABCD | 411196.75000 kWh |

Verified D+R Modbus frame: TX `01 03 0A 8F 00 02 F6 38`; RX `01 03 04 48 C9 82 60 5D 25` (CRC OK). The Float32 payload `48 C9 82 60` decodes to **412691.0 kWh**. The Delivered-only payload `48 C8 C7 98` decodes to **411196.75 kWh**. The difference of **1494.25 kWh** reflects a different cumulative basis and **must never** be treated as an interval consumption.

The workbook `PM51xx_PM53xx_PMC Register List_v2011_v2021_R01.xls` uses one-based register numbers. Subtract one to construct the zero-based Modbus PDU address. There is **no second CT/VT multiplier** and **no MWh conversion**; PM5110 reports both readings directly in kWh. Float32 precision around 412691 kWh is 0.03125 kWh per ULP. Displaying additional decimals does not improve measurement resolution.

## Immutable profile IDs and archives

| ID | Key | Source | Use |
| --- | --- | --- | --- |
| 6 | `schneider-pm5100` | 2700 / Delivered-only kWh | **Archived legacy data only**, hidden from new meter setup and rejected as a new saved-device activation |
| **7** | **`schneider-pm5100-dr`** | **2704 / D+R kWh** | **Normal Schneider PM5110 recording** |

A saved ID-6 meter configuration **decodes to ID 7** on firmware upgrade; its old saved-device binding is detached (ID cleared) because it verified the wrong counter. Old checkpoints remain ID 6 and are **not rewritten or backfilled**. A new ID-7 sample causes source-change continuity interruption; no cross-source daily or interval delta is allowed. Migration changes the runtime reading profile, but existing NVS bytes are preserved until the operator saves Meter setup again.

Checkpoint persistence continues using the original 32-bit Float32 payload with the existing schema-3 archive layout. The independent 64-bit Wh counter (register 3212 / PDU 3211) is diagnostic only; it is **not** truncated into the 32-bit checkpoint field.

## Measurement and diagnostic map

| Field | Workbook register | PDU / hex | Encoding | Unit |
| --- | ---: | --- | --- | --- |
| Active D+R energy (required) | 2704 | 2703 / `0x0A8F` | Float32 ABCD, 2 words | kWh |
| Active D energy (legacy only) | 2700 | 2699 / `0x0A8B` | Float32 ABCD, 2 words | kWh |
| Current A (optional) | 3000 | 2999 / `0x0BB7` | Float32 ABCD, 2 words | A |
| Voltage A–N (optional) | 3028 | 3027 / `0x0BD3` | Float32 ABCD, 2 words | V |
| Total active power (optional) | 3060 | 3059 / `0x0BF3` | Float32 ABCD, 2 words | kW |
| Total power factor (optional) | 3084 | 3083 / `0x0C0B` | 4Q Float32, 2 words | cos φ |
| Frequency (optional) | 3110 | 3109 / `0x0C25` | Float32 ABCD, 2 words | Hz |
| Reactive **Delivered** (optional, distinct from D+R) | 2708 | 2707 / `0x0A93` | Float32 ABCD, 2 words | kvarh |

The physical validation sketch used Current Average at 3010 and Voltage L-L Average at 3026, whereas the optional production descriptors above use phase-A current and phase A-N voltage. These are different measurements; do not compare their values as though they were the same signal. The required kWh read does **not** depend on optional fields being available.

PF uses the four-quadrant encoding from workbook worksheet *Data Types*: for a finite raw float >1, use `2 - raw`; for raw <-1, use `-2 - raw`; reject out-of-range values. The existing API stores decoded cos φ without a separate lead/lag flag.

## Restoring the known-good acquisition path (9 November 2026)

The standalone ESP32-S3 scanner confirmed ID **1**, **19200 8E1**, manual GPIO16 direction, FC03 PDU **0x0A8F** ×2. Latest field capture: `48C9 EE18` = **413552.750 kWh** (D+R Float32) and independent Int64 Wh counter = **413552.798 kWh**. The two reads are sequential; Float32 quantization and intervening energy usage explain small differences. Neither D-only register 2700 nor reactive kvarh should ever be forwarded as active D+R kWh.

The production firmware now selects the existing **bounded CRC-validated raw FC03 reader** for Schneider profiles, matching the successful scanner's RTU transaction style. It preserves the shared strict ID/function/count/CRC checks and keeps Fort, Habuer, ACR and APM on their pre-existing ModbusMaster path. Schneider D+R continues using immutable profile **7**, `schneider-pm5100-dr`, without changing the checkpoint/archive or website/LoRa payload contracts. A native regression replays the verified D+R frame, asserts successful production decoding, and rejects CRC corruption, wrong slave and TX echo. The full firmware still needs on-device acceptance; a native test cannot prove physical UART timing.

**Windows build blocker is separate from meter communication:** firmware platform `55.03.311` is pinned to **pioarduino Core 6.1.19**. If the upload terminal says `6.2.0 is running`, run `python tools/setup-core.py` **from the repository root**, restart VS Code, and repeat the build/upload from `firmware`. This reinstalls the pinned Core in `%USERPROFILE%\\.platformio\\penv` without upgrading Arduino/ESP-IDF. Do not delete NVS, change CT/PT, or modify power-meter wiring to address a host-side build-version error.

## Commissioning and website integration

1. Open ESP32 **Meter setup**. Choose **Schneider PM5110 (D+R kWh)** and set slave **1**, baud **19200**, parity **Even**, one stop bit, and manual GPIO16 direction according to the validated installation. Save the configuration.
2. In **Meter setup → Diagnostic console**, type each of these commands separately in **web Command input**, click **Run command** after each (or use Arduino IDE Serial Monitor INPUT at 115200 baud, *Newline*, then Enter/Send):

   ```text
   profile schneider-pm5100-dr
   id 1
   baud 19200
   parity E
   status
   intermediate
   read
   deeper
   ```

   The `read` command should show D+R cumulative kWh, not 411196.75 Delivered-only kWh. The actual counter increases with time; **412691.0** is a field snapshot, not an expected constant. Resume measurement after active diagnostics.

3. Verify the local dashboard, scheduled checkpoints, LoRa telemetry and IoT uploader all carry **profile ID 7** and D+R kWh. The firmware reports the external IoT key **`schneider-pm5100-dr`**. If a separate website/device registration still expects `schneider-pm5100`, update that integration explicitly. Do **not** disguise D+R events as Delivered-only events or bypass profile matching.

The production scheme remains read-only FC03 and checks Modbus CRC/length/function/ID, finite nonnegative kWh, source continuity, RTC validity and physical jump limits. The field serial logs establish meter interoperability; final acceptance of the full flashed firmware, gateway and website still requires hardware/end-to-end tests.

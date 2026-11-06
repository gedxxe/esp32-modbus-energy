# Schneider PM5100 reading profile

[English](SCHNEIDER_PM5100.md) · [Bahasa Indonesia](id/SCHNEIDER_PM5100.md)

The `schneider-pm5100` profile has immutable ID **6**. IDs 0–5 retain their existing definitions, and checkpoint schema 3 remains 446 bytes. The source is the owner-supplied `PM51xx_PM53xx_PMC Register List_v2011_v2021_R01.xls`, read without modifying it. The relevant worksheet is **Register List**, with **Data Types** defining the four-quadrant power-factor conversion. The workbook contains PM5100 model/product entries and a PM5110_11 support column. This profile does not claim compatibility with every Schneider PM5xxx model. The [source fingerprint and extracted rows](references/pm5100-registers.json) record the exact workbook used.

## Wire requests and units

Schneider register numbers are decimal and one-based. The firmware descriptors below contain the zero-based PDU address, so register 2700 becomes address 2699 (`0x0A8B`). Each read uses **FC03 and two complete 16-bit registers**. Schneider confirms most-significant-register-first storage for PM5000 meters in [FAQ FA275749](https://www.se.com/us/en/faqs/FA275749/). Its [PM5300 addressing example](https://www.se.com/it/it/faqs/FAQ000268374/) uses the same supplied PM5100/PM5300 register-list family and explicitly converts register 3000 to address 2999. Do not subtract one again in the diagnostic command input, which accepts PDU addresses already.

| Measurement | Workbook row | Register number | PDU decimal / hex | Type | Unit / scale |
| --- | --- | --- | --- | --- | --- |
| Active energy delivered into the load | 206 | 2700 | 2699 / `0x0A8B` | Float32 ABCD | kWh, ×1 |
| Reactive energy delivered | 210 | 2708 | 2707 / `0x0A93` | Float32 ABCD | kvarh, ×1 |
| Phase A current | 221 | 3000 | 2999 / `0x0BB7` | Float32 ABCD | A, ×1 |
| Phase A–neutral voltage | 237 | 3028 | 3027 / `0x0BD3` | Float32 ABCD | V, ×1 |
| Total active power | 255 | 3060 | 3059 / `0x0BF3` | Float32 ABCD | kW, ×1 |
| Total power factor | 268 | 3084 | 3083 / `0x0C0B` | 4Q_FP_PF | decoded cos φ |
| Frequency | 274 | 3110 | 3109 / `0x0C25` | Float32 ABCD | Hz, ×1 |

The selected energy is **delivered/import energy**, not export, the sum of import and export, or net energy. Register 2704 contains the sum and is deliberately not substituted. CT/VT settings already belong to the meter's measurement calculation, so the firmware uses `NO_RATIO` and a multiplier of one. It does not issue CT/VT setup writes or multiply the primary readings again.

Power factor is transmitted as an IEEE754 float with a special quadrant encoding. As specified on Data Types row 15, values above +1 become `2 − raw`, and values below −1 become `−2 − raw`. Thus +1.2 becomes +0.8, and −1.2 becomes −0.8. Values outside −2…+2 and non-finite payloads are rejected. Leading/lagging information is not represented separately in the existing dashboard PF field.

## Persistence, resolution and missing readings

The workbook also exposes an INT64 delivered-energy counter at register 3204 in **Wh**. That counter requires four registers and is not read or truncated by this profile. Instead, the documented Float32 kWh representation is stored losslessly as its original 32-bit IEEE754 payload in the existing checkpoint raw slot. This preserves the format used by ACR/APM without migrating archives or altering the Modbus transport.

Float32 still limits the meter-reported resolution. Around 8,000 kWh the next representable value is about 0.000488 kWh higher, around 1,000,000 kWh it is 0.0625 kWh higher, and at 16,777,216 kWh the upward step is 2 kWh. Showing three decimal places does not recover precision absent from the float. A future exact-Wh INT64 profile would require a distinct immutable ID and a versioned persistence change. Existing resolution-aware jump checks remain active.

Only accepted cumulative kWh is required for checkpoint recording. PF, frequency, voltage, current, power and reactive energy are additional readings, not prerequisites for that counter. This matters when the meter returns its NaN unavailable sentinel, PF is unavailable at no load, or A–N voltage is unavailable in a wiring configuration without neutral. Invalid optional values remain unavailable instead of becoming zero. A failed or non-finite energy response never replaces the previous accepted snapshot. Acquisition is sequential, so additional fields do not imply a simultaneous electrical snapshot.

## Select and commission

In the ESP32 dashboard, open **Meter setup**, select **Schneider PM5100**, set the address/baud/parity from the meter's communication menu and save. The documented serial baud choices are 9600, 19200 and 38400. Do not assume a slave ID. Existing UART and manual-direction wiring remain TX17 to DI, RX18 from RO, and GPIO16 to tied DE and /RE. Connect RS485 using the Schneider meter's own polarity labels, not Fort terminal numbers.

Compare delivered/import kWh against the matching meter display page, including any displayed multiplier or unit prefix. Then check current, voltage, frequency and power. The scanner uses the energy request at PDU 2699/2 and requires its existing repeated CRC-valid evidence. A response proves communication, not the manufacturer's identity.

For either the **web Command input + Run command** or **Arduino IDE Serial Monitor INPUT + Enter/Send at 115200 baud with Newline**, enter the following commands one line at a time:

```text
profile schneider-pm5100
status
intermediate
read
deeper
```

These commands select the profile in diagnostic RAM. `intermediate` probes the energy address, `read` decodes kWh and `deeper` audits the profile's register plan. Use the normal existing `id`, `baud` and `parity` controls when the saved link differs from the meter. Copy resulting **OUTPUT** to the support chat when needed. Resume production after active diagnostics, which deliberately interrupt interval continuity.

The existing browser picker, saved-device registry, checkpoint logging and IoT encoder use the new profile descriptor. An external website device binding must use `schneider-pm5100`. A separate LoRa gateway that validates known profile IDs must recognize ID 6 before consuming these records. Neither integration should reinterpret old profile IDs. No physical PM5100 acceptance test has been performed for this change.

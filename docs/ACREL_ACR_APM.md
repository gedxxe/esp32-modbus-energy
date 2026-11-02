# Acrel ACR10R-DxxTE and APM800/801/810

**English** · [Bahasa Indonesia](id/ACREL_ACR_APM.md) · [Meter profiles](METER_PROFILES.md)

Version 3.2.0 adds profile IDs 3 (`acrel-acr`) and 4 (`acrel-apm`). Fort, ADL400 and Habuer retain IDs 0, 1 and 2 and their existing conversions. These profiles use the existing FC03 transport, meter selection, discovery, saved-device verification and checkpoint schedule. They do not configure the meter or send writes.

## What the supplied manuals establish

The [ACR manual](../Acrel%20ACR%20Modbus.pdf) concerns ACR10R-DxxTE, not every ACR model. PDF pages 14–15 (printed 11–12) specify the hexadecimal map below. The energy table calls its floats primary energy but omits the energy unit and Float32 word order. This profile therefore proposes ABCD Float32 in kWh/kvarh as a **commissioning candidate**, not a verified statement from that table. Production requires an explicit saved binding after comparison with the installed LCD. If units or order differ, retain it as an observation and do not activate this profile.

The [APM manual](../Acrel%20APM%20Modbus.pdf) uses **decimal PDU addresses**, with no subtraction of one. PDF pages 55–57 establish the primary electrical map, page 63 (printed 60) gives primary Float32 import energy at **3000–3001 in Wh**, and page 74 (printed 71) describes high-word-first IEEE754 values. That example instead mentions **3050–3051**, contradicting the table. This release follows the table at decimal 3000 (`0x0BB8`). It never silently switches addresses or selects whichever value looks plausible. Compare the installed model/revision with the LCD before activating the binding. The illustrated 52140-to-kWh arithmetic on page 74 is also inconsistent with Wh. Conversion follows the explicit 1Wh table unit and the same page's 589000Wh = 589kWh example. The initial scope is APM800, APM801 and APM810.

## Production read descriptors

Every listed request is read-only FC03. A 32-bit value uses two words with the high word first (ABCD), while 16-bit values use one word. Scale converts the decoded value to the output unit shown. Both energy profiles are primary totals, so the firmware applies no additional CT/PT multiplier. Physical transformers need not be 1:1.

| Profile | Value | PDU address | Words | Decoder | Scale | Output / basis |
| --- | --- | --- | --- | --- | --- | --- |
| ACR | Current / contact probe | `0x0064` | 1 | UInt16 | 0.01 | A, manual measurement |
| ACR | Voltage | `0x0061` | 1 | UInt16 | 0.1 | V, manual measurement |
| ACR | Active power | `0x0067` | 1 | Int16 | 0.001 | kW, signed |
| ACR | PF | `0x0073` | 1 | UInt16 | 0.01 | dimensionless |
| ACR | Frequency | `0x0077` | 1 | UInt16 | 0.01 | Hz |
| ACR | Import energy | `0x0047` | 2 | Float32 | 1 | candidate primary kWh |
| ACR | Reactive energy | `0x004B` | 2 | Float32 | 1 | candidate primary kvarh |
| APM | Current | 1100 (`0x044C`) | 2 | Int32 | 0.001 | A, primary |
| APM | Voltage | 1120 (`0x0460`) | 2 | Int32 | 0.1 | V, primary |
| APM | Active power | 1156 (`0x0484`) | 2 | Float32 | 0.00001 | kW, primary (raw × 0.01 W / 1000) |
| APM | PF | 1183 (`0x049F`) | 1 | Int16 | 0.001 | dimensionless, primary map |
| APM | Frequency / contact probe | 252 (`0x00FC`) | 1 | Int16 | 0.01 | Hz, manual labels nominal frequency |
| APM | Import energy | 3000 (`0x0BB8`) | 2 | Float32 | 0.001 | kWh, primary Wh / 1000 |
| APM | Inductive reactive energy | 3004 (`0x0BBC`) | 2 | Float32 | 0.001 | kvarh, primary varh / 1000 |

For each new profile PF, frequency, import energy and reactive energy must pass as one accepted snapshot. Transport checks remain ID, function, exact length and CRC. Values must be finite, PF within −1.05…1.05, frequency within the shared 40…70 Hz runtime guard, and energy nonnegative with no Float32 sign bit set. The runtime guard is not a claimed vendor operating range. Optional electrical comparisons retain their existing polling cadence. The main counter and checkpoint remain unavailable before commissioning. A failed group preserves the last accepted readings and cannot create a checkpoint.

The live catalog exposes function, address, words, encoding, ABCD order, scale, output unit, basis and validation status for its descriptors. `commissioningRequired` records the profile policy, while `profileReady` reports whether the active saved binding satisfies it. A static manual-derived descriptor remains labeled for LCD verification even after an individual installation is confirmed.

## Commission from the local website

1. In **Meter setup → Meter model**, choose the exact ACR or APM profile, match the installed ID/baud/parity and save. Manual direction remains TX17→DI, RX18←RO and GPIO16→tied DE and /RE. Both supplied manuals list up to 38400 baud. No meter settings are written.
2. Open **Meter discovery and saved devices**. For these profiles its initial plan selects the profile probe and import energy. ACR uses `0064:1,0047:2`, while APM uses `00FC:1,0BB8:2`. Addresses in this plan editor are **hexadecimal**, including those converted from the APM decimal manual. Start a bounded scan matching known serial settings.
3. Inspect both confirmed responses and the raw UInt32/Float32 ABCD/CDAB candidates. Use diagnostics below to compare active and reactive energy against the corresponding LCD pages. A valid CRC alone does not establish units, manufacturer identity or word order. If the ACR values are Wh or CDAB rather than this candidate's kWh/ABCD, or APM energy is only at 3050, do not verify this binding.
4. Name the installation, enter its actual model/revision, select the production profile and record the compared LCD/raw values in Development notes or a downloaded report. Check the existing verification box **only after** address, word order, units and conversion agree, then save. Choose **Use for measurement** on that saved record. This is an on-device operator action, not a request for a software administrator or another firmware upload.
5. Overview now reports the saved binding as verified and accepts production energy. Selection survives restart through the existing CRC records. Editing connection settings directly clears that binding, so these profiles require selecting the verified saved record again. Unknown or incompatible units remain observations.

The existing universal default plan remains bounded to eight probes and preserves its prior Fort/ADL400/Habuer signatures. It cannot hold every signature from all five profiles simultaneously. The new profile-specific plan includes the relevant new addresses without expanding the persisted evidence record. Custom FC03 plans remain editable.

## Read-only diagnostic comparisons

Enter each command into **web Command input + Run command** after entering diagnostics. For the standalone INO use **Arduino IDE Serial Monitor INPUT, 115200 baud, Newline, then Enter/Send** for each line. These are device-console commands, not computer-terminal commands. Trial values are RAM-only and cannot authorize production.

```text
profile acrel-acr
id 1
baud 9600
parity N
read
reg 0047 2
reg 004B 2
report
resume
```

For APM, enter `profile acrel-apm` in the same input, set the actual link, then `read`. Compare `reg 0BB8 2` (decimal 3000) with `reg 0BEA 2` (decimal 3050) explicitly if investigating the manual conflict. The `order normal` / `order swap` and `scale` commands are diagnostic trials, not saved production changes. Copy resulting **OUTPUT** to support only when sharing observations.

Additional manual entries remain available through explicit raw FC03 diagnostics rather than new dashboard cards:

| Profile | Parameter | Starting address | Words / type | Unit or status |
| --- | --- | --- | --- | --- |
| ACR | Export energy | `0x0049` | 2 / Float32 | primary, unit/order require verification |
| ACR | Capacitive energy | `0x004D` | 2 / Float32 | primary, unit/order require verification |
| ACR | Reactive power | `0x006B` | 1 / Int16 | × 0.001 kvar |
| ACR | Apparent power | `0x006F` | 1 / UInt16 | × 0.001 kVA |
| APM | Export primary energy | 3002 (`0x0BBA`) | 2 / Float32 | Wh × 0.001 to kWh |
| APM | Capacitive primary energy | 3006 (`0x0BBE`) | 2 / Float32 | varh × 0.001 to kvarh |
| APM | Primary reactive power | 1164 (`0x048C`) | 2 / Float32 | × 0.01 var / 1000 to kvar |
| APM | Secondary voltage/current | 243 / 249 (`0x00F3` / `0x00F9`) | 1 / Int16 | × 0.1 V / × 0.001 A |
| APM | Secondary PF | 280 (`0x0118`) | 1 / Int16 | × 0.001 |
| APM | Secondary active power | 259 (`0x0103`) | 2 / Int32 | × 0.01 W |
| APM | Secondary reactive power | 267 (`0x010B`) | 2 / Int32 | × 0.01 var |
| APM | Secondary import/export energy | 300 / 302 (`0x012C` / `0x012E`) | 2 / Int32 | Wh, not used for final energy |
| APM | Secondary reactive in/out | 304 / 306 (`0x0130` / `0x0132`) | 2 / Int32 | table prints 1WH, reactive unit must be verified |

## Storage and acceptance limits

The existing schema-3 checkpoint remains 446 bytes, with the 32-bit raw slot holding the exact IEEE754 payload for Float32 profiles. The immutable profile ID determines how it decodes to the separately retained double kWh value. Integer profiles preserve their original arithmetic. CRC-valid but non-finite, negative or inconsistent reconstructed Float32 records are rejected. Historical schema-1/2 readers and the registry bank format remain unchanged. Counter reset checks compare nonnegative Float32 payload order, while jump checks use decoded energy and Float32 resolution rather than treating bit differences as energy increments.

Only active kWh is persisted in the ten scheduled checkpoints. Reactive energy remains live telemetry. These software checks do not establish hardware commissioning or prove that a different ACR/APM revision uses the same map. No Wi-Fi, LoRa, RTC scheduling, partition or Modbus library changes are part of this integration.

The `primary` command also reads the selected primary energy pair and prints both Float32 word-order candidates. Its raw APM Float32 value is Wh, while `read` applies the profile conversion to kWh.

Use at least two nonzero readings that change with consumption. A zero counter or unchanged/palindromic word pair cannot establish word order or unit conversion.

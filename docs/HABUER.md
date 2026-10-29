# Habuer field-reference profile

**English** · [Bahasa Indonesia](id/HABUER.md) · [All meter profiles](METER_PROFILES.md)

The Habuer option implements the working ESP32-S3 sketch supplied by the project owner. It uses four separate FC03 requests and the same shared ModbusMaster transport as Fort and Acrel. The original [reference sketch](references/habuer-field-reference.ino.txt) is retained outside deployable firmware, with SHA256 `6ad372465827f8636a0dd2644bd205f86be4e954c68fb18da6454d642ee1447b`. This is a field-reference map. The supplied material does not identify an exact Habuer model or provide a vendor manual, so the profile does not imply that every Habuer model has this map.

## Read plan and conversion

| Order | Parameter | FC03 starting address | Words | Decoder | Unit |
| --- | --- | --- | --- | --- | --- |
| 1 | Power factor | `0x0030` | 2 | IEEE754 Float32, ABCD | cos φ |
| 2 | Frequency | `0x0032` | 2 | IEEE754 Float32, ABCD | Hz |
| 3 | Total active energy | `0x0034` | 2 | Unsigned 32-bit integer × 0.1 | kWh |
| 4 | Total reactive energy | `0x0035` | 2 | Unsigned 32-bit integer × 0.1 | kvarh |

The first returned word contains the high 16 bits. For example, active-energy words `0001 E240` represent raw 123456 and produce 12345.6 kWh. These are meter-reported cumulative totals. No additional CT/PT multiplier is applied. The identity ratio stored with a checkpoint describes the firmware conversion and does not claim that the physical transformers have a 1:1 ratio.

The overlapping starting addresses `0x0034` and `0x0035` are intentional references to the supplied working transactions. The reader does not merge them into a contiguous register request or infer a new address from the width of the previous parameter. This distinction also applies to diagnostic audit plans. Neither current, voltage nor instantaneous active power has a verified address in the supplied code, so those registers are not queried and their cards are not presented as measured values.

A new snapshot requires all four responses to pass ID, function, count, length and CRC validation. Power factor must be finite and within −1.05 to 1.05, and frequency must be finite and within 40–70 Hz, following the reference sketch. A failed or invalid parameter rejects the whole candidate. It does not replace a missing measurement with zero or combine a new counter with an older PF/frequency reading. The last accepted snapshot remains identifiable by its age, while UART frames and the rejection quality show what failed.

The profile descriptor declares required fields using a bit mask rather than a manufacturer branch. `readingQuality` exposes `required`, `transportFailed`, `invalid` and `complete`. Its bits are PF=1, frequency=2, active energy=4 and reactive energy=8. A successful last UART transaction can coexist with an incomplete snapshot if an earlier parameter failed, so inspect this group result as well as the individual Modbus status.

## Select Habuer from the website

1. Connect to the station and open **Meter setup → Meter model**. Choose **Habuer**, set the installed serial settings, select **Manual direction** and choose **Save settings**. The supplied reference uses slave ID 1, 9600 baud and 8N1. Changing the model preserves the operator's serial choices instead of silently reprogramming the meter.
2. Open **Overview**. The main counter shows accepted active kWh. The additional measurements show PF, frequency and reactive energy, while the conversion status says **Meter total**. The live acquisition uses the existing approximately two-second schedule, rather than copying the reference sketch's 15-second print loop.
3. If contact is missing, the production scanner starts with `0x0030`, two words. A confirmed response establishes transport evidence. Normal acquisition must still pass the complete required snapshot. The bounded production baud matrix extends through 38400 as a search policy, while only 9600/8N1 is established by the supplied reference. Other candidates are not claimed as manufacturer specifications.
4. **Meter discovery and saved devices** also includes the Habuer probe and energy signature in its generated default plan. A named finding can be bound to Habuer after checking the installed unit. The saved evidence is not automatic manufacturer identification, and production still performs the four-read validation after activation.

## Diagnose without changing production settings

In **Meter setup → UART diagnostics**, enter diagnostics, type each line below into the **web Command input and choose Run command**. In the standalone sketch, type the same lines into **Arduino IDE Serial Monitor INPUT at 115200 baud with Newline**, then press **Enter/Send** after each line. These are device commands, not computer-terminal commands or messages to a support chat.

```text
profile habuer
id 1
baud 9600
parity N
direction manual
read
report
```

`read` runs the complete required snapshot. `deeper` prints the four independent raw queries and the conversion result. To investigate a specific parameter, use `reg 0030 2`, `reg 0032 2`, `reg 0034 2` or `reg 0035 2` in that same command input. Trial settings remain in RAM. Choose **Resume measurement** or enter `resume` to restore the saved production configuration. Copy resulting **OUTPUT** to a support chat only when sharing evidence.

## Checkpoints and wiring

The ten daily active-kWh checkpoints use the existing DS3231 UTC clock and WIB schedule. The timestamps bracket the actual energy request and response, even though PF and frequency are read earlier in the snapshot. The existing five-second admission window and bounded cycle checks remain in force. A rejected snapshot cannot create a checkpoint or advance the active-energy continuity baseline. Fort and Acrel historical IDs and conversion semantics remain unchanged, with Habuer assigned immutable profile ID 2. Reactive energy is live telemetry in this release and is not a second persisted checkpoint series.

![Complete Habuer wiring schematic](assets/wiring-habuer.svg)

The ESP32 connections stay TX17 to DI, RX18 from RO and GPIO16 to tied DE and /RE. The shared direction adapter settles for 100 µs before TX and after returning to RX. Connect the module A/B pair to the labels on the installed meter. Numeric Habuer terminal assignments were not provided, so the diagram does not borrow Fort or Acrel terminal numbers. The figure is a logical wiring representation, not a photograph of package pin positions.

Native tests execute the real ModbusMaster source with the exact four-request sequence, missing replies, invalid floats, complete-snapshot retention, direct conversion and checkpoint/registry round trips. Browser tests exercise the shipped gzip UI. The owner's working sketch is hardware evidence for the reference map, while this integrated release still requires comparison against the installed meter's LCD and physical timing checks.

The live API exposes `meterEnergy` and `meterAge` as the generic register-value fields. The older `secondaryEnergy` and `secondaryAge` names remain compatibility aliases. Read `meter.energyBasis` to distinguish a meter-reported total from a secondary counter that needs transformer conversion.

![Representative Habuer dashboard with simulated measurements](assets/ui-habuer-desktop.png)

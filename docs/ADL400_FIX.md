# ADL400 correction based on the owner's field scan

**English** · [Bahasa Indonesia](id/ADL400_FIX.md) · [Meter profiles](METER_PROFILES.md)

Version 3.3.0 follows the owner's successful ADL400 scan. There is still exactly one visible **Acrel ADL400** choice. Production now reads total active energy at `0x0000`, two holding registers, as an unsigned high-word-first integer. The forward `0x000A` and reverse `0x0014` counters remain diagnostic comparisons, not additional source choices or automatic fallbacks.

## Findings and cause

The earlier reader used forward energy `0x000A`, while the independently confirmed raw value 9798 came from total energy `0x0000`. The prior configuration could therefore query a different quantity from the owner's successful scan. Its secondary energy scale and PT/CT formula were already correct. There is no evidence of a CRC bug or a reason to add a fitted correction factor. The old power descriptor at `0x016A` is documented in the supplied V1.3 manual, but the field scan exercised the lower integer power register `0x006A` and the separate `0x53xx` float block. The corrected reader follows those observed addresses.

The field configuration is slave **220**, **9600**, **8N1**, with TX17, RX18 and tied DE-/RE on GPIO16. ID 220 is an installation setting, not an ADL400 default. Save the actual address in Meter setup. A scan restricted to IDs 1–16 cannot find 220, and there is no need to run that scan when the address is already known.

## Final production descriptors

All requests use FC03, with hexadecimal PDU addresses and no 40001 offset. Each Float32 uses two registers in **ABCD** order, based on the owner's verified voltage/frequency readings and the supplied manual's float block. No CT/PT multiplier is applied to electrical measurements.

| Quantity | Address | Registers | Decode and output |
| --- | --- | --- | --- |
| Contact probe | `0x0064` | 1 | UInt16 × 0.01 A |
| Total active energy | `0x0000` | 2 | UInt32 ABCD × 0.01 kWh, then PT × CT once |
| PT / CT | `0x008D` / `0x008E` | 2 in one request | UInt16 PT, UInt16 CT; both 1–9999 |
| Phase-A voltage | `0x5300` | 2 | Float32 ABCD, V |
| Phase-A current | `0x530C` | 2 | Float32 ABCD, A |
| Total active power | `0x006A` | 1 | Signed Int16 × 0.001 kW |
| Total PF | `0x5330` | 2 | Float32 ABCD, dimensionless |
| Frequency | `0x5332` | 2 | Float32 ABCD, Hz |

The [supplied manual](../Acrel%20ADL400%20Manual.pdf) was visually checked at PDF pages 16–19 and 22–23. Its length column is bytes: length 4 means two 16-bit registers. Page 17 prints kWh beside active *power*, which is dimensionally inconsistent. The low power descriptor uses 0.001 kW, consistent with the quantity and the separately documented kW power block. The field report does not include a calibrated power value, so exact power agreement still needs a matching LCD reading. Float power at `0x5318`, reactive power at `0x5320` and apparent power at `0x5328` remain raw diagnostic comparisons because their engineering units were not established by the supplied scan values. Their mere availability does not justify inventing a scale.

Optional electrical samples retain the existing comparison cadence. They do not become prerequisites for accepting a valid energy sample. Missing or rejected values remain missing rather than zero. PF age is not presented as if it were sampled with every energy request.

## Energy calculation and the LCD difference

```text
raw = (uint32_t(highWord) << 16) | lowWord
secondary_kWh = double(raw) / 100.0
energy_kWh = secondary_kWh × PT × CT
9798 / 100 × 1 × 80 = 7838.40 kWh
```

The reference LCD value is 7838.9 kWh. At PT 1 and CT 80, one raw count represents **0.8 kWh**, so the 0.5 kWh difference is smaller than one counter increment. Quantization, sequential sample timing or comparing different counter pages can account for a difference of this size, but the evidence does not isolate one cause. No offset, gain adjustment or forced 7838.9 result is implemented. Forward/reverse readings are logged rather than presumed equal to the total, and reverse energy is never assumed to be zero.

The main path combines the words before scaling. `state.meterEnergy` contains the decoded secondary value, while `profiles::primaryEnergy` computes the final double kWh once. Checkpoints receive raw UInt32 plus PT/CT metadata and independently serialize the same final result, rather than multiplying an already scaled API value. The API and UI receive engineering values. CT/PT is not applied again by the browser.

## Use the existing firmware website

1. Choose **Acrel ADL400** in **Meter setup**, set the installed slave address and serial settings, then save. For the reported installation use 220, 9600 and N parity. The firmware does not change the meter settings.
2. Overview shows **Total active energy**. The register panel identifies `0x0000` and the secondary value, while the main counter includes PT/CT. Voltage/frequency use the verified Float32 map.
3. If you want the full field comparison, open **Diagnostic console**, choose the **ADL400 energy comparison** preset, then choose **Run command**. This is an active read-only investigation, so scheduled recording pauses until **Resume measurement**. It is not required to select the energy source.

Alternatively, enter these lines into **web Command input + Run command**, one line per click after entering diagnostics. On the standalone INO, type each into **Arduino IDE Serial Monitor INPUT, 115200 baud, Newline, then Enter/Send**. They are device-console commands, not computer-terminal commands.

```text
profile acrel-adl400
id 220
baud 9600
parity N
read
adl400
resume
```

`read` follows the actual production energy path. `adl400` performs a single bounded report: PT/CT before and after the reads, total/forward/reverse/reactive raw UInt32 values, secondary and scaled energy, resolution, observed read times, reverse-zero/nonzero status and seven ABCD/CDAB float comparisons. If ratios fail or change, the report withholds scaled comparisons. Missing registers print unavailable, not zero. The measurements are sequential and the report says so. Stop interrupts through the existing transport service hooks. Normal polling does not repeatedly print this report, which keeps the serial/web log bounded.

The reactive counter at `0x001E` is reported in kvarh using the same ratio interpretation. It remains diagnostic telemetry rather than a new persisted checkpoint series. For isolated comparisons, `reg 0000 2`, `reg 000A 2`, `reg 0014 2`, `reg 001E 2`, `reg 008D 2` or `reg 5300 2` go into the same device command input. Copy resulting **OUTPUT** to support only when sharing logs.

## Compatibility and evidence

Historical profile ID 1 retains the earlier forward-energy meaning and conversion. It is decoded for archived records but hidden from selectors and cannot be activated as a saved production binding. Current ADL400 uses immutable internal ID 5 with the same public `acrel-adl400` key. Old active configurations upgrade automatically while retaining slave ID, baud, parity and direction. The old named binding is detached rather than falsely claiming it verified the new counter. Its registry record and captured data are preserved. Source changes break continuity, and completed records are not rewritten. No extra ADL400 source option is exposed.

Fort and Habuer descriptors and their transaction behavior remain unchanged. No packet parser, Wi-Fi settings, checkpoint schedule or flash-record schema was replaced. Host tests replay valid frames addressed to 220 through the real ModbusMaster source and verify 7838.40 kWh, float decoding, PT/CT stability, reverse-zero/nonzero cases, unavailable energy without fallback, config migration and checkpoint round trips. Browser tests exercise the actual generated gzip assets and verify that only one ADL400 option is offered.

The owner's successful physical scan is accepted as the reference evidence. The integrated release has not been connected to that physical meter here. Remaining acceptance is a new firmware run compared with the same LCD page at matching times, including observing total, forward and reverse together. It is not a request to repeat the wiring investigation or to change the verified scan findings.

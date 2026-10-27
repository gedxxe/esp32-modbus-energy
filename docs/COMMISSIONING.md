# Commissioning and acceptance

**English** · [Bahasa Indonesia](id/COMMISSIONING.md)

The user reported a successful standalone ModbusMaster read with a manual transceiver and DIR16. Use that known connection as the baseline for firmware 2.0.0, then record integrated readings rather than treating a software test or screenshot as hardware acceptance.

## Bench checks

Before going to the meter, verify that the board boots without a Serial Monitor, exposes Midea Engineering AP at 192.168.4.1 and shows Firmware 2.0.0. Check the OLED's installed 180° orientation, all three debounced buttons, the optional buzzer and RGB settings across a restart. OLED absence must not prevent the AP from starting, and a failed filesystem must require the physical five-second OK confirmation before formatting.

With A/B disconnected, run a bounded diagnostic and retain its output. Any RX in this condition is local circuitry activity, not a Fort response. A temporary UART-only loopback, with module TTL leads removed and GPIO17 joined to GPIO18, should produce an exact TX echo that the validator rejects as energy. Remove the jumper and restore the manual module before the meter test.

## Fort connection and scaling

Connect TX17→DI, RX18←RO and DIR16→DE+/RE, with A→58 and B→59, then match CONi ID/baud/parity. In web Command input, run `status` and `intermediate` separately, choosing Run command after each. Save raw TX/RX and the 0x00 result for the manual 0x0058 request. Use `reg 0042 2`, `reg 006A 2` and `reg 0080 1` to compare voltage, power and frequency with the correct meter pages.

Compare `reg 0082 2` with the secondary import counter and inspect the PT numerator/denominator and CT values before accepting the primary calculation. The `primary` command prints float-order candidates for a separate comparison. Do not choose an order merely because the value is finite. A CRC-valid exception is protocol contact and does not pass data/scaling acceptance.

## Scan checks

Test a known ID at the saved format before expanding the range. A result should require two valid transactions, remain unapplied until selected and preserve direction when saved. Cancellation must interrupt a receive wait and restore the saved connection. Keep the output for invalid ID/CRC/timeout cases without interpreting those counters as component-presence sensors.

## Scheduled intervals and recovery

Set phone time after verifying its date/timezone, then compare the displayed WIB clock with the RTC and a restart. Check a complete morning/evening boundary, a deliberately interrupted interval and preserved completed rows. Invalid time, changed CT/PT, gaps and counter reset must not create complete consumption. Verify CSV exports and backup-preserving recovery after a deliberate, recorded power-cut test.

## Acceptance record

Retain firmware version/commit, physical module and wiring mode, meter menu values, raw words, display comparisons, RTC checks, interval/CSV samples and observed heap/stack headroom. The core monitor is a tick-sampling aid rather than an instruction profiler, and multi-client AP latency still needs an on-target run. [VALIDATION](VALIDATION.md) distinguishes these checks from local compiler/native/browser results.

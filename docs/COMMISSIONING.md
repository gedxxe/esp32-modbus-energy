# Commissioning and acceptance

**English** · [Bahasa Indonesia](id/COMMISSIONING.md)

Record the board/firmware version, Fort communication settings, CT/PT values and test date. Use test intervals before relying on this device for a production record. No test below is claimed to have passed on physical hardware merely because it exists in this guide.

## Bench checks

1. Wire the OLED/DS3231 at 3.3 V and check GPIO labels against [WIRING](WIRING.md). Add the three buttons; the buzzer may remain disconnected.
2. Upload environment `esp32s3`. Verify startup without a Serial host, AP availability and the OLED IP when installed. A missing OLED must not prevent the AP/reader starting.
3. If the archive partition is uninitialized and fails mounting, hold OK during boot for five seconds. Confirm the deliberate format, then check storage status after RTC/time become valid. Do not format existing records as the first response to an archive fault.
4. Connect a phone to the AP without mobile/internet dependence. Confirm English default, Indonesian switching, persistence after reload, and functioning local CSS/JS. Check the displayed firmware version.
5. Set the RTC from a correctly timed phone. Confirm WIB, turn main power off for several minutes, then confirm DS3231 battery-backed time after restart.
6. Check button bounce: a press/release generates one page or interval action; a 900 ms OK hold toggles mute once. Test alarm behavior on RTC/meter/storage faults if the buzzer is fitted.

## Fort connection and scaling

1. Connect the reader to the dedicated Fort RS485 port: A → 58/A, B → 59/B. Photograph or record the actual slave ID, baud and parity on the meter menu.
2. Enter those reader settings manually, or follow the scan procedure below. Defaults are not evidence of the installed unit's settings.
3. Keep the confirmed automatic-module TXD→GPIO18/RXD←GPIO17 wiring, GPIO16 unused. No response alone does not locate a fault: assess UART evidence, serial settings and register support independently. RO/DI/DE-/RE applies only to the manual alternative.
4. Observe CRC/timeout counters under steady load. Isolated errors may occur; persistent increments need investigation.
5. Compare **primary imported positive active energy** with the matching Fort display. Do not compare against export, net, reactive energy or a sum of both energy directions.
6. Compare CT/PT ratios. A fixed scale-factor mismatch requires checking the register interpretation and meter settings before accepting readings.
7. Compare active kW while load is steady. Over 10–30 minutes, compare counter changes against expected energy. For example, an average 12 kW over 0.5 h implies about 6 kWh, subject to meter accuracy and counter resolution.

## Scan checks

1. Perform the initial scan outside a production interval. Start with a range containing the known ID so the test is short.
2. Verify progress, candidate ID/baud/parity and responsive navigation. Reload the page and switch language during the scan; the active range/progress must recover from the device snapshot.
3. Cancel while a request is pending. After the candidate finishes, verify the previous reader settings return. Scan counters must stay separate from normal polling errors.
4. Run again and obtain a result. Compare it with the meter menu; do not treat a compatible response as model authentication. Confirm the current config is unchanged until **Apply result** is chosen.
5. Apply the result, wait for normal readings and check storage/error status. Reboot to verify the saved reader settings survive. No meter menu/register should have changed.
6. Test an absent meter or a wrong ID range: the scan must end without a match and restore previous settings. A full 1–247 search has 4,446 candidates and can take about 70 minutes without a response.
7. Run scan during a test interval. That interval must become partial; previously completed intervals must remain. Reboot must reset scan progress/results and must not automatically restart scan.

## Scheduled intervals and recovery

1. Start before 05:55 WIB for morning and 16:25 WIB for evening. RTC, ratios and communication must be valid before the first boundary.
2. Record Fort counter values near 06:00, 06:30 and 07:00. Compare CSV differences while accounting for photo timing, poll spacing and counter resolution.
3. Check all six morning and three evening windows, including shared boundaries.
4. Disconnect RS485 for more than 15 seconds mid-interval. Reconnect and confirm partial quality rather than a complete-looking result.
5. Repeat with a reboot, explicit RTC sync and an observed counter/ratio change on a controlled test setup. Completed records must survive; interrupted windows must not be interpolated across the gap.
6. Test the WIB date boundary using test data, then export current and prior dates. Version 1.2 writes English CSV quality/shift codes while preserving older archive files.
7. Test power interruption around checkpoint/archive replacement. Compare exports before and after. A backup file must remain available after a failed replacement/restore. Simulated filesystem tests do not substitute for physical flash power-cut tests.

## Acceptance record

Accept the installation only after scaling/direction/word order match the actual meter; all fitted I/O works; RTC battery backup and AP remain usable; interval, gap and recovery tests pass; and the cable/supply/panel arrangement is approved by the responsible facilities engineer. Retain the firmware version, test evidence and known limitations with the installation record.

## OLED mounting and RGB acceptance

1. With OLED installed upside down, check boot animation, normal pages and the recovery prompt read upright. Set OLED_ROTATE_180=false only for normal mounting. Do not format archives merely to test the prompt.
2. Confirm the board's physical RGB pin (48 confirmed for this installation; 38 on DevKitC v1.1 revisions). Check Hue starts at brightness 128 with an 8-second cycle.
3. In Device & records, apply Off/Solid/Hue/Blink/Breathing, primary colors, brightness 0/128/255 and 0.5/30-second cycles. Check physical output as well as the applied status.
4. Reload the phone page, then restart the board. Verify saved settings return after boot; boot lighting still uses compile-time defaults. Confirm a storage warning is absent before relying on persistence.
5. Repeat while the meter is polling and while scanning/serving assets. Observe watchdog resets, missed UART responses and animation jitter; no on-target timing measurements are claimed by the software tests.

## Raw capture and standalone diagnosis

1. On the main firmware, open Meter setup / Raw TX/RX diagnostics, start capture, then run a one-ID scan. Confirm captures identify each attempt's baud/parity/ID/register and freeze after twelve attempts, or earlier on Stop. Download the raw text.
2. Repeat with A/B disconnected at the ESP32 module, keeping TTL/power connected. RX traffic in this test is not a Fort reply. Exact request echo, malformed bytes and no-byte timeout must remain distinct observations; do not infer a valid connection from error counters.
3. The supplied module has automatic direction and four TTL pins. Leave GPIO16 unconnected; verify functional TTL input/output directions before changing wiring. The reported installation crosses module TXD to GPIO18 and RXD to GPIO17.
4. For Arduino-only diagnostics follow the [field guide](../examples/FortSerialDebug/README.md). In Arduino IDE **Tools → Serial Monitor**, select **115200/Newline**. Type `x`, Enter; `status`, Enter; `diagnose`, Enter. Wait for `[REPORT_END]`. Copy scrolling **OUTPUT** (settings and TX/RX/CRC/report) into the support chat. Commands go to Serial Monitor INPUT; logs go to the chat. Optional paired tests require physically changing only A/B at the module between `check connected` and `check isolated`; those states are operator labels. This sketch replaces the main application temporarily.
5. Restore the main firmware after testing. Verify retained records/time/settings before resuming interval acceptance. A successful compile or mock capture does not prove the user's meter was found.

## Main firmware diagnostic acceptance — 1.4.0

Follow [the phone procedure](DIAGNOSTICS.md) first. Check current-format and18-format matrix scope, editable probe/count and a matching CRC-valid exception found without pretending energy works. Exercise no RX, echo then separate reply, malformed/oversized frames and raw map failure; preserve output. Stop during a wait, close/reopen panels, then resume and verify saved UART pins/settings, production readings and partial interrupted interval. Test settings/values must not enter NVS or energy records. Verify event sequence gaps, no animation with idle traffic, reduced motion, task high-water/heap and core samples under scan/network load. Software tests do not establish physical module health, A/B waveforms or meter scaling.

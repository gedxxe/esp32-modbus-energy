# Diagnostics in the main firmware

**English** · [Bahasa Indonesia](id/DIAGNOSTICS.md) · [Overview](../README.md)

Firmware 2.3.0 uses the same FortMeter/ModbusMaster source for normal reads, scanner probes and the standalone sketch. The diagnostic console therefore tests the installed reading path rather than a separate parser. Its default connection is manual direction on TX17/RX18/DIR16, with a 2000 ms response timeout, 100 µs driver/receiver settling and 200 ms between requests.


The web console now also shows the current session settings and evidence counters, with structured register and trial-setting controls. The primary-float preset prepares the same `primary` command available to the standalone sketch. Use the [complete web guide](WEB.md) for the feature-to-API map and AP/client connection steps.


## First run at the panel

1. Join **Midea Engineering AP**, password **betahdijre12345**, open **http://192.168.4.1**, and confirm Firmware 2.3.0 in the footer. In Meter setup, match the Fort CONi ID/baud/parity and select Manual direction for the confirmed DE-/RE installation.
2. Open **Diagnostic console**. Type `status` into the web **Command input**, then choose **Run command**. Read the printed ID, format, direction and timings before starting a test. These command lines are device input, not messages for a support chat.
3. Enter `intermediate` as a new command and choose Run command. This sends the manual's phase-A current request at `0x0058`, two registers, and prints TX/RX plus the ModbusMaster result. At ID 1 the request bytes are `01 03 00 58 00 02 45 D8`.
4. Enter `diagnose` separately when a wider record is needed. It first observes RX without TX for three seconds, then tries the selected probe and the configured raw register plan, including energy/ratio decoding when enabled. A silent first address does not prevent the later plan entries from running.
5. Wait for `[REPORT_END]` in **Device diagnostic output**, then choose **Download output**. Keep that TXT file. If requesting analysis, copy the resulting OUTPUT or file to the support chat rather than pasting log labels back into Command input.
6. Choose **Resume measurement** after testing. Confirm that the maintenance warning disappears and the saved production connection returns before leaving the panel.

An active UART test enters maintenance, pauses RTC/interval sampling and interrupts continuity for the current interval, while preserving completed intervals. Its settings and values remain in RAM. Closing a panel or browser does not resume recording, so reopening the dashboard is the way to check whether a session remains active. Read-only `status`, `report`, `help`, `compare` and `plan show` can inspect retained information without entering maintenance.

## Read the transaction result

| Output | Meaning and next action |
| --- | --- |
| `0x00 OK` and `[VALID_RESPONSE]` | ID, FC03, count, captured length and CRC passed, compare the returned value with the correct Fort display. |
| `0x01–0x04` with `[COMM_VALID_EXCEPTION]` | A matching CRC-verified exception arrived, the requested data was not read, so inspect the address/count and meter support. |
| `0xE0 WRONG_SLAVE_ID` | The received prefix belongs to another ID or is malformed, inspect HEX rather than treating it as a Fort reply. |
| `0xE1 WRONG_FUNCTION` | The received function differs from FC03 or its exception form. |
| `0xE2 RESPONSE_TIMEOUT` | No complete response arrived before the deadline, partial RX can also end in this status. |
| `0xE3 CRC_ERROR` | The captured response failed integrity, including exception CRCs. |
| `0xE4 ABORTED` | Stop/resume interrupted the wait and no reading was accepted. |
| `0xE5 LOCAL_UART_ERROR` | The local driver or byte write failed, so the transaction cannot assess the external path. |
| `0xE6 INVALID_FRAME_LENGTH_OR_ECHO` | The requested shape did not match, or the complete request was echoed back, nothing is stripped. |
| `0xE7 RX_OVERFLOW` | Input exceeded the bounded capture/receive limit and was rejected. |

RX bytes, CRC errors and BREAK events are observations of the local UART path. None alone proves that the Fort replied or that a module is powered and healthy. With A/B disconnected, received activity is local to the remaining circuitry. Conversely, a slave may remain silent until requested, so passive silence does not prove a failed component. The output preserves that uncertainty instead of converting it into a hardware verdict.

## Choose a bounded test

Enter each command below **one line at a time in web Command input and choose Run command for that line**. Address arguments are hexadecimal PDU addresses and quantities are 16-bit words. No 40001 offset is added.

| Purpose | Command |
| --- | --- |
| Repeat the manual current example | `probe 0058 2`, then `intermediate` |
| Read voltage, power or frequency directly | `reg 0042 2`, `reg 006A 2`, `reg 0080 1` |
| Inspect the secondary energy counter | `reg 0082 2` |
| Compare primary float word orders | `primary` |
| Observe RX without sending a request | `listen 5` |
| Inspect weak-pull response at the TTL input | `rxcheck` |
| Print retained evidence without another request | `report` |
| Stop a running test and older queued work | `x` or Stop test |
| Restore saved production settings and recording | `resume` or Resume measurement |

Runtime configuration includes `id 1`, `baud 9600`, `parity N`, `pins 18 17`, `direction manual`, `settle 100`, `gap 200`, `timeout 2000` and `interval 2000`. `direction auto` is available for an actual automatic-direction circuit. It is not a way to operate a manual transceiver without its DE-/RE wiring. The diagnostic pin guard excludes the direction pin while manual control is selected.

The raw plan accepts `plan clear`, up to eight `plan add ADDRESS WORDS` entries, and `plan show`. The energy view accepts `energy 0082`, `ratios 002C`, `scale 0.001`, `order normal` or `order swap`, and `decode on/off`. Those overrides affect only the test session. The manual's energy-length inconsistency can be investigated with one/two/three-word raw reads, but a primary float must not be reinterpreted as a secondary integer simply because one value looks plausible.

## Search connection settings

The production scanner starts with the saved baud/parity/direction and the selected ID range, using `0x0058/2` by default. A valid data/exception result is repeated before being offered. Apply result saves the reader's connection and leaves energy/ratio checks to normal acquisition. Discovery does not authenticate a meter model or validate the physical energy scale.

At the default two-second timeout, 16 silent IDs take roughly 35 seconds including request spacing. A full 18-format matrix over 1–247 takes about three hours when silent, so start with the actual CONi settings or a narrow range. Console lists can be restricted with `range 1 16`, `scanmode current`, `bauds 9600 19200`, `parities NE` and `scan`. Issue `x` to cancel during a wait. A production scan and active console test never own UART concurrently.

## Live communication and process monitor

The communication view follows the device's queued/completed TX, first RX byte and completed/rejected transaction events. Its 32-event ring exposes sequence gaps, while the browser replays timestamp spacing at 1× with polling/network delay and no decorative movement during silence. These events record software observations, not measured propagation through A/B or confirmation that a remote device received the request.

Device & records contains the process monitor, which samples non-idle core state and displays heap/PSRAM, task priority/state and stack high-water values. It polls only while open, as do the detailed log panels. Stack minima are historical headroom, and tick samples are not instruction-time accounting. Use them to find pressure before performing on-device load and endurance tests.

## Serial Monitor alternative in the main firmware

Open **Arduino IDE/PlatformIO Serial Monitor at 115200 baud with Newline**. Type `debug` into **Serial Monitor INPUT** and press Enter/Send to enter maintenance, then type `status` and `intermediate` as separate input lines. `x` stops a test and `resume` restores normal measurement. The same resulting OUTPUT may be copied to the support chat, and the web log can be downloaded even when USB is not attached.

## Paired physical observations

`check connected`, `check isolated` and `compare` retain user-labeled captures with matching settings and completion guards. Only use those labels when you actually change A/B at the module between captures. Do not run both on an unchanged physical connection and interpret them as automatic cable detection. The [eight field recipes](../examples/FortSerialDebug/README.md#choose-the-test-by-the-observation) provide bounded variations for passive RX, format search, raw-map and word-order checks.

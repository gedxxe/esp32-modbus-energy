# FortSerialDebug 2.1.0 / field diagnostic guide

**English** · [Bahasa Indonesia](README.id.md)

This sketch runs the same FortMeter/ModbusMaster source used by firmware 2.1.0, with USB Serial instead of the web application. Its default is the confirmed manual transceiver: TX17→DI, RX18←RO and DIR16→DE+/RE, with 100 µs settling, 200 ms request spacing and a two-second response timeout. It sends FC03 reads only and keeps test configuration in RAM.

## 1. Upload once

In Arduino IDE, install the supplied **FortMeter-library-v2.1.0.zip** through **Sketch → Include Library → Add .ZIP Library**, then open `FortSerialDebug.ino`. The ZIP includes the documented ModbusMaster 2.0.1+fort.1 source. Installing only the stock ModbusMaster library does not supply its runtime service hooks. From a source checkout, copying the entire [FortMeter folder](../../firmware/lib/FortMeter) into your configured Sketchbook `libraries` folder is the equivalent installation.

Select an ESP32-S3 board with 16 MB flash, OPI PSRAM and USB CDC enabled, then compile/upload with Arduino ESP32 2.0.17. PlatformIO is an alternative: from the repository-root **computer terminal**, run `pio run -d examples/FortSerialDebug`, or add `-t upload`. It resolves the same local library automatically. Neither route requires the React build to run this standalone sketch.

## 2. Keep the installed wiring

![Manual transceiver direction sequence](../../docs/assets/rtu-timing.svg)

TX17 connects to DI, RX18 receives from RO, and GPIO16 connects to DE and /RE joined together. The compatible module shares the ESP32's low-voltage supply/ground, with A going to Fort terminal 58/A and B to 59/B. Terminal 70/COM is a switch-input common rather than RS485 ground. Use `direction auto` only for a physical automatic-direction module, with no GPIO16 connection.

## 3. Type commands into Serial Monitor INPUT

Open **Arduino IDE Serial Monitor at 115200 baud and choose Newline**. Type `status` into **Serial Monitor INPUT**, press Enter/Send, and read the resulting **OUTPUT** before continuing. Then type `intermediate`, press Enter/Send and inspect the manual's current request at 0x0058/two words. These lines are typed into the device console, not sent to a support chat.

For a wider test, type `diagnose` as another input line and wait for `[REPORT_END]`. Copy the resulting OUTPUT, beginning at `[REPORT_BEGIN]` together with preceding settings/TX/RX, to the support chat if requesting analysis. Never type those output labels back into Serial Monitor INPUT. On the main firmware, the corresponding destination is **web Command input + Run command**, one line per click, after opening Diagnostic console.

## Choose the test by the observation

| Observation or purpose | Recipe |
| --- | --- |
| RX activity without sending requests | [01 passive RX](cases/01-passive-rx.txt) |
| Exact echo or no valid response | [02 echo/reply](cases/02-echo-or-no-reply.txt) |
| Search a bounded ID/format set | [03 serial matrix](cases/03-serial-matrix.txt) |
| Separate a valid exception from readable data | [04 register exception](cases/04-register-exception.txt) |
| Audit ambiguous register width without assuming energy | [05 raw map](cases/05-raw-map-audit.txt) |
| Decode the secondary counter with explicit CT/PT | [06 configured kWh](cases/06-configured-kwh.txt) |
| Compare configured timeout budgets | [07 timeout comparison](cases/07-timeout-comparison.txt) |
| Compare integer word order explicitly | [08 word order](cases/08-word-order-comparison.txt) |

The recipe comments name INPUT and OUTPUT destinations. For Serial Monitor, type/paste non-comment lines and allow each test to finish. The bounded console can retain up to sixteen lines, while `x` interrupts a wait and clears older queued commands. For the web console, enter each non-comment line separately and choose Run command. Multi-line web paste is rejected to prevent ambiguous execution.

In the standalone sketch, x stops work while retaining trial settings and resume restores startup defaults. In the main firmware, Resume measurement restores the saved production connection.

## Runtime reference

| Setting or action | Example and meaning |
| --- | --- |
| Link | `id 1`, `baud 9600`, `parity N`, `pins 18 17` (RX then TX) |
| Direction and settling | `direction manual`, `settle 100`, `gap 200`, times are µs then ms |
| Response and poll timing | `timeout 2000`, `interval 2000`, milliseconds |
| Contact probe | `probe 0058 2`, then `intermediate` |
| Raw words | `reg 0082 2`, addresses are hexadecimal PDU values |
| Primary float comparison | `primary`, prints ABCD/CDAB candidates without selecting either |
| Raw plan | `plan clear`, `plan add 0080 1`, `plan show`, at most eight queries |
| Energy view | `energy 0082`, `ratios 002C`, `scale 0.001`, `order normal/swap`, `decode on/off` |
| Discovery | `range 1 16`, `scanmode current/matrix`, `bauds 9600 19200`, `parities NE`, `scan` |
| Passive evidence | `listen 5`, `rxcheck`, `report`, `stats clear` |
| User-labeled comparison | `check connected`, physically change A/B, `check isolated`, `compare` |
| Repeated measurement and stop | `read`, `poll`, `x`, `resume` |

Invalid parameters leave the existing configuration intact. The manual direction pin is excluded from UART trial pins. A full 4,446-candidate matrix at two seconds can take about three hours, whereas recipe 03 deliberately uses a smaller timeout/range. Start with the actual meter menu settings rather than a full search.

## Read the result, then choose an action

`0x00` requires matching ID/function/count/length/CRC, while an exception is separately CRC-verified and does not produce kWh. `0xE0`, `0xE1`, `0xE2` and `0xE3` retain ModbusMaster's wrong-ID, wrong-function, timeout and CRC meanings. The integration adds cancellation/local-I/O/shape/overflow results at 0xE4–0xE7. [The main guide](../../docs/DIAGNOSTICS.md#read-the-transaction-result) explains next actions for each result.

Passive silence cannot identify a missing, unpowered or defective module. When A/B is physically disconnected, local RX activity is not a Fort reply, even when repeated request bytes produce repeated noise. `rxcheck` only observes weak-pull behavior, and paired labels are operator assertions that require a real physical change between matching completed captures. A valid transport result must still be compared with the correct meter page before accepting scaling or float word order.

## Boundaries and verification

The host suite compiles this actual INO together with the vendored ModbusMaster source and runs every field recipe. PlatformIO also compiles the same library for the physical N16R8 target. Those checks establish software behavior, not the installed module's signal levels, DE-/RE waveform or meter-display agreement. Record those separately using [COMMISSIONING](../../docs/COMMISSIONING.md).

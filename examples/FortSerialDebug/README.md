# Fort serial diagnostic — ESP32-S3

**English** · [Bahasa Indonesia](README.id.md) · [Main firmware](../../README.md)

Open [FortSerialDebug.ino](FortSerialDebug.ino) in Arduino IDE. This standalone sketch uses only the Arduino ESP32 core: no SSD1306, RTC, JSON, Wi-Fi or Modbus library. It replaces the running dashboard application while installed. Use the main firmware again after the diagnostic session.

## Wiring

- GPIO17: ESP32 UART1 TX → the module's TTL **input**.
- GPIO18: ESP32 UART1 RX ← the module's TTL **output**.
- ESP32 3V3 → module VCC; ESP32 GND → module TTL GND.
- Module A+ → Fort 58/A; B− → Fort 59/B. Fort 70/COM is not RS485 ground.
- The pictured four-pin module controls direction automatically. GPIO16 is unused by this sketch. The user's current reported wiring is module RXD → GPIO17 and module TXD → GPIO18. That reports the installation, not independent verification of the module label directions.

OLED, RTC, switches and buzzer are not needed. No supply is taken from the Fort communication terminals.

## Arduino IDE

1. Install **esp32 by Espressif Systems** in Boards Manager. Local compilation was checked with Arduino ESP32 **2.0.17** through PlatformIO; the Arduino IDE GUI and other core versions were not tested.
2. Open the sketch folder. Keep its [partitions.csv](partitions.csv) beside the INO to match the main firmware's 16 MB partition layout. The sketch does not read/write NVS or mount/format a filesystem. Keep **Erase All Flash Before Sketch Upload → Disabled**; export important records before diagnostic reflashing.
3. Select **ESP32S3 Dev Module**, flash **16MB**, PSRAM **OPI PSRAM**. For the native USB connector choose **USB Mode: Hardware CDC and JTAG**, **USB CDC On Boot: Enabled**. If using a USB-UART bridge, select that port and the serial routing appropriate to it.
4. Verify and upload. If automatic upload does not enter download mode, hold BOOT while tapping RESET, then release BOOT and retry upload.
5. Open Serial Monitor at **115200 baud**, line ending **Newline**. Send `help` if needed. The sketch never waits indefinitely for a USB terminal to attach.

PlatformIO users can run `pio run -d examples/FortSerialDebug` from the repository root; this uses the included optional configuration with the same core version and no external libraries.

## First diagnosis: A/B disconnected at the ESP32 module

Send each line separately:

```text
range 1 1
scan
```

This tests ID 1 at six baud rates × N/E/O parity (18 combinations). Keep the TTL/power wiring connected. A quiet, disconnected receive path should show no valid meter reply. Repeated `EXACT_TX_ECHO` proves an 8-byte received frame equals that attempt's request; it does not locate the physical loopback. `CRC MISMATCH`, short frames or other RX activity remain untrusted data. Save the complete Serial output, including TX and RX, for comparison.

Next reconnect the Fort A/B pair and repeat the same test. Change one variable at a time. If the actual slave ID is unknown, expand the range:

```text
range 1 247
scan
```

The full search has **4,446 candidates** at 1200/2400/4800/9600/19200/38400, each with 8N1/8E1/8O1. It can take about an hour or more without replies; it is not a search of every possible UART format. Send **x** to abort, including during an 800 ms response wait. During busy waits only x is processed; send other commands after stopping.

`FOUND` requires valid CRC/ID/function/length energy and ratio responses, plus nonzero PT/CT words within the documented range. It indicates protocol compatibility, not authentication of the meter model. The sketch selects the matching reader settings in RAM; it never reconfigures the meter.

## Read once or continuously

Use the communication settings from the Fort menu when available:

```text
id 1
baud 9600
parity N
read
poll
```

`read` requests the energy counter and CT/PT once. `poll` toggles repeated reads with a 2-second target; retries/timeouts and printing may lengthen the interval. `status` reports reader settings. `x` stops polling/scan. No communication settings are persisted.

The secondary unsigned 32-bit counter is read from **0x0082–0x0083**, high word/byte first. PT numerator/denominator/CT come from **0x002C–0x002E**. Primary imported kWh is `raw × 0.001 × PT numerator / PT denominator × CT`, calculated in double with the ratio applied once. These match the main firmware and the manual references in [DESIGN](../../docs/DESIGN.md#fort-register-reference).

## What the logs mean

Each request prints uptime, slave, baud/parity, register, count and TX hex. Each observed RX frame prints hex, calculated/received CRC and validation outcome. A response must pass all checks before its data is used. A valid exception reports its code but is not accepted as an energy reading.

An exact standalone request echo is logged and rejected; the receiver continues waiting within the original deadline for a separate valid response. Concatenated/ambiguous data is not blindly stripped or accepted. A no-byte timeout is distinguished from RX activity without any valid response. RX storage is capped at **64 bytes per frame**, the receive wait at **800 ms**, and detailed frame logging at **four frames per request**. Extra frames are still validated; a valid result/exception is always reported. RX observations come from the UART buffer, not per-byte oscilloscope timestamps. An aborted/incomplete frame is not accepted. Serial output changes timing, so this is a diagnostic tool rather than a timing acceptance test.

Only Modbus **function 03** is transmitted. There are no register writes, broadcast probes, resets, NVS writes or automatic filesystem formatting. Compilation is not evidence of successful operation on the user's Fort installation.

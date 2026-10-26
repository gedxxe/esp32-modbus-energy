# Firmware design

**English** · [Bahasa Indonesia](id/DESIGN.md)

This guide describes the implementation in `firmware/src/main.cpp`, `energy.h`, `scan.h` and `recovery.h`. Timeouts and scheduling intervals are bounds or targets in the code, not measurements of the physical installation.

## Fort register reference

The primary reference is the scanned `Fort Power Meter_User Manual_compressed.pdf` at the repository root. Addresses below match the manual's hexadecimal PDU addresses; there is no 40001 translation.

| PDF page / printed page | Reference |
| --- | --- |
| 7, table 3; 9, figure 5 | RS485 58/A and 59/B. Terminal 70/COM belongs to switch inputs. |
| 18 / 16, §7.1 | Half-duplex RTU, 1200–38400 baud, default 9600, N81/E81/O81; default N81 |
| 26 / 24, tables 13–14 | Function 03 example; high word/high byte first for the integer data |
| 27 / 25, table 17 | PT numerator 0x002C, denominator 0x002D, CT multiplier 0x002E |
| 29 / 27, table 17 | Active power 0x006A–0x006B, signed 32-bit, 0.0001 kW |
| 30 / 28, table 17 | Imported secondary energy 0x0082–0x0083, unsigned 32-bit, 0.001 kWh; primary float at 0x008A–0x008B |

The application uses only **function 03**. Configuration changes select the reader's UART settings and slave ID; there is no meter register-write path.

The request `01 03 00 58 00 02 45 D8` has a correct CRC. The manual's response `01 03 04 00 00 C3 17 72 E5` does not: the calculated CRC bytes are **EA CD**. Tests reject the printed response and accept the corrected one. The secondary-energy table says length “3” while giving an unsigned long and two register addresses. The reader requests two words/four bytes. These discrepancies are recorded rather than copied into protocol exceptions.

## Startup order

![Startup after the ROM and bootloader](assets/boot-flow.svg)

1. Start Serial without waiting for a host. Set GPIO16 LOW and buzzer LOW, configure button pull-ups, allocate mutexes and queues.
2. Start hardware I2C at 100 kHz on GPIO8/9 with a 40 ms timeout. Probe the OLED before allocating its display buffer. Run the roughly 1.5 s boot animation/buzzer pattern; missing OLED skips drawing.
3. Mount LittleFS with `begin(false)`. On failure, a physically held OK button can authorize a five-second format. Existing archives are not automatically destroyed.
4. Start the AP at 192.168.4.1/24, channel 6, up to four clients. Generate a new random POST token, publish an initial snapshot, register HTTP routes and start the watchdog.
5. Create the core-1 acquisition task. That task opens Preferences, validates its settings/checkpoint and starts UART1. The Arduino loop handles UI/HTTP on core 0.

Allocation/task-creation failures are fatal and trigger restart. Missing meter, RTC, OLED or filesystem are reported as degraded operation rather than blocking startup indefinitely.

## Task and lock model

![Execution contexts](assets/architecture.svg)

`ARDUINO_RUNNING_CORE=0` and `ARDUINO_EVENT_RUNNING_CORE=0` are build flags. The acquisition task is explicitly created on core 1 at priority 2 with a **12,288-byte ESP-IDF task stack argument**. The Arduino loop runs at priority 1. Wi-Fi/TCP/IP tasks remain under the framework's scheduler; the application does not assign every system task to a core.

| Resource | Access rule | Wait bound in the caller |
| --- | --- | --- |
| UART1, Preferences, tracker, scanner | Acquisition task only | No cross-task lock needed |
| Command queue | Core 0 sends, core 1 drains before each loop | Six values; enqueue wait 0; HTTP 503 if full |
| Snapshot queue | Core 1 overwrites, core 0 peeks | One `Snapshot` value; no pointer sharing |
| I2C mutex | RTC transactions and OLED rendering | 80 ms read / 100 ms write / 30 ms OLED |
| Filesystem mutex | Acquisition archive replacement; UI history/export | 200 ms archive write / 100 ms HTTP |

The snapshot contains a copy of the day, active settings, scan progress, diagnostics and latest values. Only acquisition mutates its live state. Core 0 cannot update the tracker indirectly through a pointer. HTTP config/time/mute/scan requests are parsed and queued; NVS writes and UART reconfiguration happen on core 1.

The normal acquisition loop checks commands, validates RTC, handles day/clock changes, refreshes ratios when due, and polls energy/power every two seconds. It then publishes and yields 100 ms. A scan substitutes one candidate probe for normal meter polling, publishes, and yields 10 ms. RTC/day transitions continue during scan.

Timing uses unsigned subtraction on `millis()`/`micros()` to tolerate rollover. Polling is soft scheduled: one unanswered register can consume three 800 ms response windows plus wire time/gaps/backoff. It is not a fixed-frequency sampling interrupt. Each receive wait yields a tick and each attempt feeds the eight-second watchdog. UI drawing is requested no more often than every 300 ms, while the loop yields 5 ms.

The SSD1306 buffer is 128×64/8 = 1024 pixel bytes. I2C transfer and rendering occupy the UI task while drawing; mutex ownership prevents RTC traffic from interleaving. This is bounded cooperative access, not simultaneous bus transactions.

## UART and transceiver state

![Request bytes and DE timing](assets/rtu-timing.svg)

`HardwareSerial bus(1)` selects UART1. GPIO17 is TX, GPIO18 is RX. GPIO16 connects to both DE and active-low /RE: HIGH enables the driver and disables the receiver; LOW releases the driver and enables receiving. A module with automatic direction can leave GPIO16 unused, but turnaround/local-echo behavior must be checked on the actual module.

A transaction drains at most 128 stale RX bytes, waits the silent gap, sets direction HIGH, waits 30 µs, sends eight bytes and waits for `flush()` to finish transmission before returning LOW. UART hardware generates the start/data/parity/stop bits. The software does not toggle TX for each bit.

For normal 8N1, a character is 10 bits; E/O parity uses 11. The gap is `ceil(3.5 × bits / baud)` seconds, or 1.75 ms above 19200 baud. The pre-request delay rounds upward to RTOS milliseconds plus one millisecond. Received bytes are observed through the driver buffer, so this does not provide accurate t1.5 timestamps for every wire byte.

The driver RX buffer is 128 bytes. A fixed 16-byte local array covers the maximum 11-byte response used by this reader. Overflow, empty response, CRC mismatch, wrong ID/function/length/count and valid exception 0x83 are rejected. CRC16 is computed in software with initial 0xFFFF and reflected polynomial 0xA001; low CRC byte is transmitted first. The normal path uses up to three attempts and backoff, while scan uses one attempt per register. Scan counters are separate from normal polling counters.

## RTC, counter conversion and continuity

DS3231 stores UTC. The driver checks ACK/length, oscillator-stop flag, valid BCD/calendar, 24-hour mode and century range before accepting a timestamp. An explicit phone sync writes UTC, clears OSF and verifies the resulting time. The day/interval key uses `UTC + 7 hours`, preserving WIB calendar boundaries.

Energy is `unsigned_raw × 0.001 × PT_numerator / PT_denominator × CT`. Power is `signed_raw × 0.0001 × ratio`. Ratio words must be nonzero and no higher than 9999. Ratios refresh after 60 seconds with at least five seconds between retries; sustained stale ratios stop conversion. A ratio change invalidates the baseline instead of mixing scales.

Energy polling is bracketed by RTC reads. Timestamp windows above two seconds or transactions above 2.5 seconds do not feed the tracker. The tracker rejects backward/duplicate timestamps, NaN/negative values and the wrong day. Boundary interpolation is allowed only across monotonic samples no more than 15 seconds apart. Counter decreases, including possible rollover, are treated as discontinuities because a reset cannot reliably be distinguished from wrap.

The jump check uses a compile-time 100,000 kW limit plus two counter ticks of tolerance. It is an installation sanity check, not a metrology guarantee. Update the limit for the plant's actual capacity during commissioning.

## Scan controller

`scan.h` is an allocation-free enumerator/state machine. States are `idle`, `running`, `found`, `cancelled`, `exhausted`. Within an inclusive ID range, all six baud rates and three parities are tried exactly once: `(last − first + 1) × 18` candidates. The configured ID gets all modes first if it belongs to the range. UART is restarted only when baud/parity changes, not for each slave ID.

A candidate must return valid energy words at 0x0082 and valid ratio words at 0x002C. No broadcast or write requests are used. Scan stops at the first compatible response. Detection does not cryptographically identify a Fort model.

Starting scan interrupts and checkpoints the tracker, clears the energy baseline and pauses normal polling. Completed intervals remain. Cancellation is processed between candidates, each with at most two 800 ms response windows. End/cancel restores the saved serial settings and invalidates ratio/baseline state. Apply-result goes through the same checked NVS config path as a manual setting. Manual settings are rejected during scan at both HTTP and task layers; duplicate start cannot reset an active scan. Reboot forgets progress/results.

## Persistence and export

Preferences records include schema magic and an application CRC. A checkpoint stores `Day`; recovery calls `Tracker::interrupt()` so reboot cannot bridge an unobserved gap. NVS provides its own underlying commit behavior, but the app has no second checkpoint journal and does not reconstruct the tracker from CSV.

CSV archive replacement writes `/pending.csv`, rotates the old file to `.bak`, installs the replacement, then removes the backup. `installArchive` preserves recoverable data when a rename/restore fails. Export falls back to `.bak` if the main file is absent. Retention removes dates older than 31 stored calendar days only after a successful current write. There is no automatic formatting after mount failure.

HTTP copies a bounded CSV under the filesystem mutex and sends it after unlocking. A full/corrupt filesystem is reported, not silently replaced. RTC battery backup maintains time; it does not keep ESP32 sampling while power is off. Power-cut/endurance behavior needs hardware testing.

## HTTP and browser delivery

`GET /api/status`, `/api/history`, `/api/export[?day=epoch_day]`; `POST /api/config`, `/api/time`, `/api/mute`, `/api/scan/start`, `/api/scan/cancel`, `/api/scan/apply`. POST uses a per-boot token and bounded numeric/range parsing. Commands return 202 for queue acceptance, not a claim of completed NVS/RTC work. Status must be checked afterwards. A backwards calendar date is rejected while the current RTC/day is valid.

The AP password and HTTP token define the current access boundary. There is no separate login/TLS, OTA, cloud, captive DNS or NTP. Whoever has AP access can read/export and change reader settings. Phone time sync is explicit.

The React app uses local gzip assets generated into PROGMEM arrays. HTML, JS, CSS and third-party notices have separate endpoints and correct MIME/Content-Encoding headers. Asset payloads are sent from flash in at most 1 KiB nonblocking lwIP sends, yielding/feed-watchdog per iteration, with a five-second transfer deadline. The synchronous UI task can be occupied during this transfer; core-1 acquisition does not depend on it. This avoids allocating a whole JS bundle in heap or sitting inside a long `WiFiClient::write` retry loop.

React/React DOM, GSAP and React Bits execute in the browser. Both language catalogs are bundled locally; language selection is stored in browser localStorage with an English fallback when storage is blocked. Live values are never animated through synthetic counter values. CSS and React Bits handle reduced motion. No CDN/font/API request is needed outside the AP.

## References

- [Modbus serial-line specification](https://www.modbus.org/file/secure/modbusoverserial.pdf)
- [Espressif FreeRTOS task API](https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/api-reference/system/freertos_idf.html)
- [DS3231 datasheet](https://www.analog.com/media/en/technical-documentation/data-sheets/DS3231.pdf)
- [React Bits provenance](../web/vendor/react-bits/README.md)

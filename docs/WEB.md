# Using the local dashboard

**English** · [Bahasa Indonesia](id/WEB.md)

Firmware 2.1.0 serves the complete application from the ESP32, including measurement, connection discovery, the shared ModbusMaster diagnostic console, raw traffic, process metrics and Wi-Fi settings. The standalone INO remains an optional USB-only diagnostic tool. It is not required to unlock features in the installed firmware, and changing Wi-Fi mode does not change meter settings or replace the firmware.

## Reach the application

On a fresh installation, connect the phone to **Midea Engineering AP** using **betahdijre12345** and open **http://192.168.4.1**. The AP password is the compile-time setting in [board.h](../firmware/include/board.h). English is selected initially, with Indonesian available from the language selector. Once joined as a client, open the IP assigned by the router instead. The OLED network page and the Wi-Fi panel show the current addresses.

Open **Device & records → Wi-Fi connection** to select a mode, enter a 2.4 GHz SSID and password, then choose **Save Wi-Fi settings**. The ESP32 uses DHCP for client addressing. A password must be 8–63 printable ASCII characters or a 64-digit hexadecimal WPA key. For an intentionally open network, select the explicit checkbox. Leaving the field blank retains a saved password only when the SSID is unchanged, and the saved password is never returned to the browser or put into browser storage. An enterprise identity/certificate workflow is not implemented by this form.

| Mode | What remains reachable |
| --- | --- |
| Access point only | Direct access through the Midea hotspot, with client credentials retained for a later mode change |
| Client with AP recovery | AP during connection attempts, then client IP after a successful DHCP connection and a 15-second AP grace period |
| Access point + client | Both interfaces, serving the same application and API |

Saving first goes through the core 1 command queue and CRC-protected NVS record. A failed save leaves the active connection intact. After a successful save, core 0 waits eight seconds before applying it, giving the browser time to receive the execution result. The client attempt never blocks the acquisition task. AP recovery remains available while disconnected, attempts repeat every 30 seconds, and a lost client link brings the AP back. The firmware does not format storage, erase intervals or reboot as part of this transition.

![Wi-Fi modes and recovery](assets/network-modes.svg)

The AP and client share one radio, so the AP follows the router's channel in combined mode. If the LAN overlaps the AP subnet, the firmware chooses a non-overlapping AP address from 192.168.4.1, 192.168.5.1, 10.42.0.1 and 172.23.42.1. Read the current address rather than assuming 192.168.4.1 after a network transition. A phone on the router must be allowed to reach other LAN clients. The same local HTTP interface becomes reachable on that LAN, with the existing per-boot mutation token and no separate account/TLS layer.

## Follow the meter pipeline

Start in **Overview** to check current, voltage, frequency, the secondary counter and primary imported energy. Primary energy requires validated CT/PT applied once, whereas a valid secondary counter remains visible if ratio validation is pending. The sample age and recording quality identify stale or interrupted intervals, and the live communication view follows observed UART events rather than producing decorative traffic during silence.

In **Meter setup**, match CONi ID/baud/parity and Manual direction for the installed GPIO16 circuit. Scan a known ID with the saved format before expanding the range. The initial probe is FC03 at 0x0058 with two words, and a candidate must reply twice with valid data or a CRC-verified exception. Apply result saves the reader connection only. An exception establishes protocol contact but does not validate the energy register or scale.

Open **Diagnostic console** when a connection needs investigation. Session settings show the UART owner's actual configuration, while Observed UART results separates valid data, exceptions, CRC, incomplete input, echoes, timeouts and local failures. Read-only `status`, `report` and `help` inspection does not pause production. A raw read, trial-setting change or active test enters maintenance and interrupts the current interval, keeping trial values outside recording.

Use **Read a register directly** to enter a hexadecimal PDU address and one/two/three words, then choose **Read raw words**. Use **Adjust a trial setting** to select ID, format, direction, timing, probe, profile or scan-list parameters and apply one change at a time. The next read uses those settings and the console output records rejection of invalid arguments. **Primary float** prepares the `primary` command, which prints ABCD/CDAB candidates after **Run command** without choosing one automatically.

For any other supported command, type one line into **Command input** and choose **Run command**. **Stop test** remains available while a test is waiting, while **Resume measurement** cancels work and restores production pins, direction and saved serial settings. Download output to retain raw frames, decoded words and evidence before leaving the panel. [DIAGNOSTICS](DIAGNOSTICS.md) explains the detailed commands and the eight recipes, including the distinction between UART observations and physical connection claims.

## Integration map

| Web feature | HTTP path | Device owner and result |
| --- | --- | --- |
| Live measurements, quality, saved connection and Wi-Fi state | `/api/status` | Copied acquisition snapshot plus core 0 network state |
| Meter connection and confirmed scan result | `/api/config`, `/api/scan/start`, `/api/scan/cancel`, `/api/scan/apply` | Core 1 command execution, shared FC03 reader |
| Trial settings, raw reads, float comparison and diagnostic recipes | `/api/diagnostic` GET/POST | Core 1 FortDiagnostics, same FortTransport as acquisition and INO |
| UART direction and frame replay | `/api/uart` | Bounded software event ring, including sequence gaps |
| Passive raw transaction capture and export | `/api/trace`, `/api/trace/start`, `/api/trace/stop` | Bounded copied trace, without changing CRC acceptance |
| Core/task/heap/PSRAM observations | `/api/system` | Tick sampling and owned/idle task metrics |
| Network modes and credentials | `/api/network` | Core 1 persistence, one-entry message to core 0 Wi-Fi state machine |
| LED, alarm mute and RTC synchronization | `/api/led`, `/api/mute`, `/api/time` | Queued controls with execution results |
| Completed records and CSV | `/api/history`, `/api/export` | Bounded filesystem access with existing recovery rules |

All these routes are registered by the production firmware. Ordinary controls use an execution ID rather than treating HTTP 202 as completion, and incompatible operations are checked again at the UART owner's transaction boundary. Wi-Fi changes are rejected during a scanner or maintenance session, while Stop/Resume has its own priority mailbox. The browser stops polling detailed logs, trace and process panels when they close, and malformed data cannot become accepted measurements.

## Verify the integrated installation

After uploading 2.1.0, check the footer version, perform the manual 0x0058 read from the web console and compare the displayed register values with the meter. Exercise Stop during a timeout, Resume back to production, a narrow scan, trace download, process metrics, CSV and a Wi-Fi mode change. For client mode, also disconnect the router deliberately and confirm that the AP returns without interrupting acquisition. These physical checks complement the compiled/native/browser checks recorded in [VALIDATION](VALIDATION.md), which use simulated APIs for browser tests.

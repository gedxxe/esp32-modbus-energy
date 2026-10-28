# Build and flash

**English** · [Bahasa Indonesia](id/FLASHING.md)

Firmware **2.4.0** targets the physical ESP32-S3 N16R8 with 16 MB flash and OPI PSRAM. Open `firmware/` in PlatformIO and select `esp32s3`. The local FortMeter library is part of the source checkout, so main polling and standalone diagnostics cannot silently select different registry versions.

For an installed logger, use PlatformIO upload or the app-only image at 0x10000 with the unchanged partition layout. The merged factory image spans the NVS range at 0x9000–0xDFFF through padding, so it is not the preferred update path when preserving current settings and NVS recovery records. Export available checkpoint CSVs before maintenance, and do not use whole-chip erase or filesystem formatting for a routine update.

## Physical board

Close Serial Monitor before upload and connect the board's USB port used for programming. From a **computer terminal in `firmware/`**, run `pio run -e esp32s3`, then `pio run -e esp32s3 -t upload`. Open `pio device monitor -e esp32s3` afterward. If automatic entry fails, hold BOOT while resetting/reconnecting and release it when the uploader begins communication.

A v1 CRC-valid connection record preserves ID/baud/parity and migrates to the confirmed manual direction on DIR16. Existing day and LED records keep their formats. A whole-chip erase is not part of an ordinary update and would remove stored records. Filesystem initialization remains the physical boot-confirmation procedure in the [README](../README.md#3-first-startup-and-meter-scan).

## Binary files

| File | Offset | Use |
| --- | --- | --- |
| `midea-fort-factory.bin` | 0x0 | Combined bootloader, partitions, boot_app0 and app |
| `midea-fort-app.bin` | 0x10000 | App-only update with the matching partition layout |
| `bootloader.bin` | 0x0 | Separate component flashing |
| `partitions.bin` | 0x8000 | Separate component flashing |
| `boot_app0.bin` | 0xE000 | Initial app-selection metadata |

An app-only image does not contain a bootloader and must not be flashed at 0x0. Factory images include initial selection metadata but do not include the CSV filesystem, so prefer PlatformIO for normal development updates and retain backups before deliberate erase operations.

## Arduino diagnostic library

Install `FortMeter-library-v2.4.0.zip` through Arduino IDE's Add .ZIP Library menu, then open the standalone INO. The same ZIP can be generated without a firmware build using `python tools/package-library.py` from the repository-root computer terminal, or the source FortMeter folder can be copied into the configured Sketchbook libraries directory. Follow the [standalone guide](../examples/FortSerialDebug/README.md) for board options and explicit Serial Monitor INPUT/OUTPUT instructions.

## Rebuild browser assets

A normal PlatformIO build uses committed assets and needs no Node. After editing browser inputs, run `npm ci`, `npm run build:web` and `npm run check:web` from the repository-root **computer terminal** with Node 24.22.1, then build firmware again. The header and manifest must be committed with the source changes. There is no separate filesystem upload for the dashboard.

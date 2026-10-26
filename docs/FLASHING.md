# Build and flash

**English** · [Bahasa Indonesia](id/FLASHING.md)

Firmware **1.3.0**, ESP32-S3 DevKitC-style **N16R8**: 16 MB flash, 8 MB OPI PSRAM. Open the `firmware/` directory in PlatformIO.

## Physical board

Close Serial Monitor, then run from `firmware` (replace COM10 with the actual port):

```powershell
pio run -e esp32s3 --target upload --upload-port COM10
pio device monitor -e esp32s3 --port COM10
```

PlatformIO chooses component files and offsets. If `pio` is absent from PATH, invoke `& "$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe"` instead. The committed React bundle is checked before compile; no npm installation is required unless web sources change.

## Binary files

| File | Offset | Use |
| --- | --- | --- |
| `midea-fort-factory.bin` | **0x0** | Combined bootloader, partition table, initial OTA metadata and application |
| `midea-fort-app.bin` | **0x10000** | Application only; requires this project's existing partition layout |
| `bootloader.bin` | 0x0 | Separate ESP32-S3 bootloader |
| `partitions.bin` | 0x8000 | Partition table |
| `boot_app0.bin` | 0xE000 | Initial OTA selection metadata |

Example from a directory containing the packaged BIN files:

```powershell
python -m esptool --chip esp32s3 --port COM10 --baud 460800 write_flash 0x0 midea-fort-factory.bin
```

Do not write an app-only image at 0x0. Factory flashing resets initial app-selection metadata; use PlatformIO for normal updates. The dashboard is stored in firmware flash, so there is no separate web filesystem upload. CSV filesystem contents are not included in the factory image. On a new board, initialize storage with the physical OK-at-boot procedure in the [README](../README.md#3-first-startup-and-meter-scan).

## Rebuild browser assets

From the repository root:

```sh
npm ci
npm run build:web
npm run check:web
```

Then compile/upload as above. The PlatformIO pre-build guard rejects source/hash mismatches instead of silently embedding an old dashboard. Keep the generated header and `web/bundle.json` with the source commit.

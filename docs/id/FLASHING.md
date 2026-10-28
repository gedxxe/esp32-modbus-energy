# Build dan flash

[English](../FLASHING.md) · **Bahasa Indonesia**

Firmware **2.3.3** memakai target fisik ESP32-S3 N16R8 dengan flash 16 MB dan OPI PSRAM. Buka `firmware/` pada PlatformIO dan pilih `esp32s3`. FortMeter lokal ikut source checkout agar polling dan diagnostik tidak memilih versi library yang berbeda.

Tutup Serial Monitor sebelum upload. Pada **terminal komputer yang berada di `firmware/`**, jalankan `pio run -e esp32s3`, kemudian `pio run -e esp32s3 -t upload`, dan buka monitor dengan `pio device monitor -e esp32s3`. Jika auto-reset gagal, tahan BOOT saat reset/reconnect dan lepas setelah uploader mulai berkomunikasi.

| Berkas | Offset | Penggunaan |
| --- | --- | --- |
| `midea-fort-factory.bin` | 0x0 | Gabungan bootloader, partisi, boot_app0 dan app |
| `midea-fort-app.bin` | 0x10000 | App saja dengan layout partisi yang cocok |
| `bootloader.bin` | 0x0 | Flash komponen terpisah |
| `partitions.bin` | 0x8000 | Flash komponen terpisah |
| `boot_app0.bin` | 0xE000 | Metadata pemilihan app awal |

App-only tidak memiliki bootloader dan tidak ditulis pada 0x0. Factory memuat metadata awal tetapi tidak memuat filesystem CSV, sehingga PlatformIO lebih sesuai untuk update biasa. Migrasi record koneksi v1 ber-CRC sah mempertahankan ID/baud/parity dan memilih direction manual DIR16, tanpa mengubah format record hari atau LED. Erase seluruh chip bukan bagian update biasa dan akan menghapus penyimpanan.

## Library diagnostik Arduino

Pasang `FortMeter-library-v2.3.3.zip` lewat Add .ZIP Library pada Arduino IDE, lalu buka INO mandiri. ZIP dapat dibuat tanpa build firmware melalui `python tools/package-library.py` pada **terminal komputer di root repository**, atau salin folder source FortMeter ke folder libraries Sketchbook. [Panduan mandiri](../../examples/FortSerialDebug/README.id.md) menjelaskan opsi board dan tujuan INPUT/OUTPUT Serial Monitor.

## Aset dashboard React

Build PlatformIO biasa tidak memerlukan Node. Setelah browser diubah, gunakan Node 24.22.1 dan jalankan `npm ci`, `npm run build:web`, serta `npm run check:web` dari **terminal komputer di root repository**, lalu compile firmware. Header serta manifest disimpan bersama perubahan source. Dashboard tidak memerlukan upload filesystem terpisah.

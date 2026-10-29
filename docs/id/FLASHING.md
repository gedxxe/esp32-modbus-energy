# Build dan flash

[English](../FLASHING.md) · **Bahasa Indonesia**

Firmware **3.0.0** memakai target fisik ESP32-S3 N16R8 dengan flash 16 MB dan OPI PSRAM. Buka `firmware/` pada PlatformIO dan pilih `esp32s3`. FortMeter lokal ikut source checkout agar polling dan diagnostik tidak memilih versi library yang berbeda.

Tutup Serial Monitor sebelum upload. Pada **terminal komputer yang berada di `firmware/`**, jalankan `pio run -e esp32s3`, kemudian `pio run -e esp32s3 -t upload`, dan buka monitor dengan `pio device monitor -e esp32s3`. Jika auto-reset gagal, tahan BOOT saat reset/reconnect dan lepas setelah uploader mulai berkomunikasi.

| Berkas | Offset | Penggunaan |
| --- | --- | --- |
| `midea-fort-factory.bin` | 0x0 | Gabungan bootloader, partisi, boot_app0 dan app |
| `midea-fort-app.bin` | 0x10000 | App saja dengan layout partisi yang cocok |
| `bootloader.bin` | 0x0 | Flash komponen terpisah |
| `partitions.bin` | 0x8000 | Flash komponen terpisah |
| `boot_app0.bin` | 0xE000 | Metadata pemilihan app awal |

App-only tidak memiliki bootloader dan tidak ditulis pada 0x0. Factory memuat metadata awal tetapi tidak memuat filesystem CSV, sehingga PlatformIO lebih sesuai untuk update biasa. Migrasi record koneksi v1 ber-CRC sah mempertahankan ID/baud/parity dan memilih direction manual DIR16, tanpa mengubah format record hari atau LED. Erase seluruh chip bukan bagian update biasa dan akan menghapus penyimpanan.

Untuk logger terpasang, gunakan upload PlatformIO atau image app-only pada 0x10000 dengan layout partisi yang sama. Image factory gabungan melintasi area NVS 0x9000–0xDFFF lewat padding, sehingga bukan pilihan utama untuk mempertahankan setting serta record pemulihan NVS. Unduh CSV checkpoint sebelum pemeliharaan, dan jangan memakai erase seluruh flash atau format filesystem untuk update rutin.


Versi 3 memerlukan integrasi pioarduino terpin dan Arduino-ESP32 3.3.11. Pada **terminal komputer di root repo**, jalankan `python tools/setup-core.py` sekali untuk memasang pioarduino Core 6.1.19 pada environment terkelola yang dipakai build turunan SDK. Buka ulang terminal PlatformIO sesudahnya, atau gunakan path interpreter yang dicetak alat setup. Build pertama mengunduh SDK/compiler dan membuat cache SDK bersama. Layout partisi tetap sama, tetapi simpan image sebelumnya serta ekspor record sebelum memperbarui runtime major.
## Library diagnostik Arduino

Pasang `FortMeter-library-v3.0.0.zip` lewat Add .ZIP Library pada Arduino IDE, lalu buka INO mandiri. ZIP dapat dibuat tanpa build firmware melalui `python tools/package-library.py` pada **terminal komputer di root repository**, atau salin folder source FortMeter ke folder libraries Sketchbook. [Panduan mandiri](../../examples/FortSerialDebug/README.id.md) menjelaskan opsi board dan tujuan INPUT/OUTPUT Serial Monitor.

## Aset dashboard React

Build PlatformIO biasa tidak memerlukan Node. Setelah browser diubah, gunakan Node 24.22.1 dan jalankan `npm ci`, `npm run build:web`, serta `npm run check:web` dari **terminal komputer di root repository**, lalu compile firmware. Header serta manifest disimpan bersama perubahan source. Dashboard tidak memerlukan upload filesystem terpisah.

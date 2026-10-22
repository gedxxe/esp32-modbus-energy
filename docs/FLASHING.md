# Firmware hasil build

Target: ESP32-S3 DevKitC-style N16R8 (flash 16 MB, PSRAM OPI 8 MB).

Untuk pemasangan pertama, file **midea-fort-factory.bin** berisi bootloader, partition table, metadata awal OTA, dan aplikasi. Tulis pada offset **0x0** memakai esptool / Espressif Flash Download Tool.

Contoh dari folder ini, ganti COM7 dengan port board:

```powershell
python -m esptool --chip esp32s3 --port COM7 --baud 460800 write_flash 0x0 midea-fort-factory.bin
```

Pilihan utama tetap `pio run --target upload` dari folder `firmware`, agar PlatformIO memilih file dan offset yang benar.

`midea-fort-app.bin` adalah aplikasi saja, offset **0x10000**, dan hanya cocok jika partition table project ini sudah terpasang. Jangan menulis app-only pada 0x0. Factory image mereset metadata pemilihan aplikasi; gunakan alur PlatformIO untuk update normal.

Filesystem arsip tidak disertakan dalam factory image. Lakukan inisialisasi fisik sesuai README pada instalasi pertama. Tidak diperlukan koneksi internet untuk dashboard setelah firmware terpasang.

Semua binary berhasil dibangun, tetapi belum diflash/diuji pada hardware pengguna.

[English](../FLASHING.md) · **Bahasa Indonesia**

# Firmware hasil build

Firmware: **1.2.0** (dengan Scan Fort). Setelah upload, buka Koneksi meter → Scan Fort pada dashboard.

Panduan langkah demi langkah ada pada [cara menjalankan Scan Fort](../../README.id.md#scan-fort).

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

## Cirkit Designer: import ZIP dengan profil 8 MB

Dialog **Choose firmware** menerima `.bin` dan `.zip` serta mendukung pemilihan beberapa file. Pilih bundle **midea-fort-cirkit-8mb.zip**, lalu Import. Isinya berupa `firmware.bin` (Application), `bootloader.bin` (Bootloader), dan `partitions.bin` (Partitions).

Bundle ini memakai environment `cirkit`, bukan image N16R8 hardware:

```powershell
cd firmware
pio run -e cirkit
cd ..
python tools/package_cirkit.py
```

Pada pengujian importer, satu app BIN saja menghasilkan `no bootloader .bin provided` / `no partitions .bin provided`. Bundle dikenali sebagai tiga role yang benar. Image hardware 16 MB juga melewati tahap bootloader setelah dibundel, tetapi emulator melaporkan flash 8 MB dan menolak header 16 MB; karena itu digunakan partisi/header 8 MB pada profil khusus ini.

Target default `esp32s3` tetap memakai flash 16 MB dan PSRAM OPI untuk board fisik N16R8. Jangan mengganti firmware hardware dengan bundle simulator untuk pemasangan produksi.

## Aset dashboard React

Build PlatformIO biasa memakai header gzip yang sudah ada dan memeriksa hash source, sehingga tidak perlu Node. Setelah mengubah source di `web/`, gunakan Node 24.22.1 dan jalankan `npm ci`, `npm run build:web`, lalu `npm run check:web` dari root sebelum compile. Dashboard tersimpan di firmware; tidak perlu upload filesystem web terpisah. Buka `http://192.168.4.1`, pastikan footer 1.2.0, lalu pilih Indonesia di kanan atas.

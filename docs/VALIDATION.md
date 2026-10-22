# Hasil validasi — 22 Oktober 2026

Versi firmware yang diaudit: **1.0.1**. Perubahan dan temuan tersaji di [AUDIT](AUDIT.md).

## Logika C++ — LULUS

Perintah: `tools/test.ps1`, compiler g++ dengan C++11 dan `-Wall -Wextra -Werror`.

Kode yang diuji adalah header yang sama dengan firmware, bukan salinan algoritma dalam bahasa lain:

- Debounce 35 ms menghadapi bouncing saat tekan/lepas.
- Long press 900 ms menghasilkan satu event tanpa event short press tambahan.
- Debounce tetap bekerja ketika `millis()` melewati maksimum uint32.
- CRC query resmi Fort; salah cetak CRC respons ditolak; respons yang diperbaiki diterima.
- Bit corruption pada setiap byte respons, ID salah, panjang salah dan jumlah register salah ditolak.
- Keenam interval pagi dan ketiga interval sore menghasilkan delta sesuai beban simulasi.
- Interpolasi batas, startup terlambat, kehilangan sampel di tengah/dekat batas, gap tepat 15 detik dan lebih dari 15 detik.
- Pemulihan checkpoint ke tracker setelah reboot mempertahankan interval selesai dan menandai interval aktif parsial.
- Timestamp mundur/duplikat, nilai NaN/negatif, counter reset, hari salah dan pergantian hari.
- Fault injection filesystem: normal replacement, backup-only recovery, gagal restore, gagal memasang file baru dan gagal merotasi file lama; data lama tetap tersedia pada jalur kegagalan yang diuji.
- Retry CT/PT tidak terjadi lebih sering dari lima detik, termasuk ketika penghitung millis rollover.

## Dashboard browser — LULUS

Perintah: `node tools/preview-test.cjs <lokasi-modul-playwright> msedge`.

HTML diambil langsung dari `firmware/include/web_ui.h`; HTTP API **disimulasikan** pada localhost. Browser Microsoft Edge headless, viewport desktop 1440×1150 dan HP 390×844.

- Enam baris pagi / tiga baris sore, pergantian tab.
- Pengiriman sinkronisasi waktu, form slave/baud/parity, dan token sesi pada POST.
- Meter offline menyembunyikan nilai daya; kegagalan API menampilkan bahwa data tidak diperbarui.
- Tidak ada JavaScript page error dan tidak ada overflow horizontal viewport.
- Screenshot desktop dan HP telah diperiksa visual; versi HP menampilkan kualitas setiap interval tanpa geser horizontal.
- Versi API tampil pada footer dan sesuai `version.h`.
- Sebanyak 15 SVG dirender; batas semua teks diperiksa agar tidak keluar viewBox. Diagram wiring, kontak tombol, arsitektur dan OLED juga diperiksa visual.

Preview memakai angka simulasi, **bukan hasil pembacaan meter pabrik**:

- [Preview desktop yang ditampilkan di README](assets/ui-desktop.png).
- [Preview HP yang ditampilkan di README](assets/ui-mobile.png).

## Dokumentasi dan kesiapan repository — LULUS LOKAL

`python tools/check_docs.py` memeriksa 10 file Markdown, link/anchor lokal, 16 SVG (termasuk badge lisensi), 31 sambungan netlist, pin firmware dan konsistensi versi. Jumlah link terakhir dicetak oleh tool; pemeriksaan mencakup nama file dengan case yang tepat untuk GitHub/Linux, anchor heading, XML SVG, tidak adanya resource/script eksternal, badge dan konfigurasi. Teks GPLv3 lengkap dan metadata `GPL-3.0-only` juga diperiksa.

Root `AGENTS.md`, README visual, panduan kontribusi, changelog, `.gitattributes`, `.editorconfig`, `.gitignore` dan lockfile development tools telah disertakan. Workflow CI menyediakan job logic/docs, build ESP32-S3 dan browser. Workflow belum dijalankan di GitHub pada sesi ini; tidak ada klaim status CI remote lulus.

## Build ESP32-S3 — LULUS

PlatformIO `pio run`, environment `esp32s3`, platform Espressif32 6.10.0 / Arduino ESP32 2.0.17. Build release menghasilkan `firmware.bin` dan `firmware.elf`, exit code 0. Build akhir tidak menghasilkan warning/error compiler.

- Target ESP32-S3, CPU 240 MHz, flash 16 MB, QIO/OPI PSRAM; loop dan Arduino event task di core 0, akuisisi dipin ke core 1.
- RAM statis: 46.904 dari 327.680 byte (14,3%). Angka ini belum mencakup seluruh alokasi heap/stack saat berjalan.
- Program flash: 864.373 dari partisi aplikasi 6.553.600 byte (13,2%).
- Firmware factory digabung memakai esptool 4.5.1: bootloader 0x0, partition table 0x8000, boot_app0 0xE000, aplikasi 0x10000.

## Belum diuji pada hardware

Belum ada pengujian flash/boot ESP32-S3, UART dengan Fort fisik, I2C dengan OLED/DS3231, suara buzzer, tombol fisik, RTC battery backup, AP radio, multi-client HTTP pada perangkat, gangguan noise kabel, ketahanan power-cut flash, dan endurance pabrik. Tes browser memakai respons simulasi; tes C++ tidak menjalankan task/driver ESP-IDF. Ikuti `docs/COMMISSIONING.md` untuk uji penerimaan perangkat.

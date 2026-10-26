# Catatan validasi — 27 Oktober 2026

[English](../VALIDATION.md) · **Bahasa Indonesia**

Firmware **1.4.1**, diagnostik mandiri **FortSerialDebug 1.3.1**. Ini hasil pemeriksaan software lokal. Penerimaan Fort terpasang tetap memerlukan [commissioning](COMMISSIONING.md) dan [diagnostik lapangan](DIAGNOSTICS.md). Detail ukuran aset dan batas teknis tercatat di [laporan English](../VALIDATION.md).

## Build firmware — lulus

espressif32 **6.10.0** / Arduino ESP32 **2.0.17**, target fisik ESP32-S3 N16R8 (flash 16 MB / OPI PSRAM). Output akhir tanpa warning/error compiler.

| Build | RAM statis | Program |
| --- | ---: | ---: |
| Firmware utama `esp32s3` | 79.328 byte | 1.094.969 byte |
| INO mandiri tanpa library eksternal | 21.476 byte | 332.101 byte |

RAM statis bukan total heap/stack runtime. Stack loop core 0 sebesar 16 KiB dan akuisisi 12 KiB; alokasi antrean/driver, high-water aktual dan latensi banyak klien perlu pengukuran fisik. GUI Arduino IDE dan versi core lain belum diuji.

## Regresi native — lulus

Tiga suite C++11 dengan `-Wall -Wextra -Werror` melalui `tools/test.ps1`. Tes INO mengeksekusi file sebenarnya; tes produksi mengeksekusi engine generated yang dipakai firmware utama.

- CRC/ID/fungsi/count/panjang salah, exception, echo, prefix gabungan, overflow, tail terlambat dan respons valid. Exception membuktikan transport saja, bukan kWh.
- Byte valid terlambat selama guard sebelum TX dibuang. RX terus aktif menghasilkan bus-busy terbatas tanpa TX. Echo diikuti respons byte-per-byte pada turnaround minimum 19200/8E1 diterima sebagai frame terpisah. Inisialisasi UART gagal dicoba lagi. Kasus baru dijalankan pada INO serta engine produksi.
- Fallback multi-alamat, plan raw, pin/ID/format/timing/profil runtime, scan current/matrix, observasi pull lemah, capture berpasangan dengan guard konfigurasi/abort, input antrean dan delapan resep lapangan. Semua request yang terkirim tetap FC03/CRC sah/nonbroadcast.
- Enumerator 4.446 kandidat matriks dan 247 kandidat current, prioritas ID/format, rentang satu ID, cancel/restart dan rollover millis.
- Sembilan interval, interpolasi, gap, data hilang, reboot/RTC/reset/CTPT, NaN/negatif dan pelestarian interval lengkap. Hari mundur ditolak walau RTC invalid. Waktu HP antre dikompensasi dan diperiksa ulang pada batas/rollover.
- Guard mode/revisi/umur perintah, batas 10 detik dan pergantian scan/maintenance. Tes helper tidak menjalankan antrean FreeRTOS secara paralel; integrasi utama ditinjau dan dikompilasi.
- Writer HTTP diuji dengan progres parsial, tanpa progres, error fatal dan rollover. Chunk maksimal 1 KiB, callback layanan aktif; progres tidak memperpanjang deadline. Ini injeksi fault socket tiruan, bukan saturasi Wi-Fi fisik.
- Pemulihan arsip saat operasi gagal, mode/NVS CRC RGB, debounce, raw capture serta urutan/wrap event UART.

Jeda frame tetap observasi buffer oleh task, bukan timestamp wire. Latensi RTOS dapat menyembunyikan gap. RTU tidak membawa transaction ID; asal setiap respons tertunda tidak selalu dapat dipastikan dari byte saja.

## Browser gzip — lulus

Playwright/Microsoft Edge menguji byte gzip dalam header firmware dengan API simulasi.

- EN/ID, penyimpanan bahasa/fallback, tabel interval, config, waktu, LED, mute, scan/probe/cancel/apply, CSV dan raw capture/export.
- HTTP 202 tidak melepas kontrol sebelum hasil eksekusi ID tepat. Tes mencakup completion tertunda/rejection. Sinkronisasi waktu diblokir saat maintenance; Run menunggu status konsol awal. Stop prioritas tetap tersedia saat kontrol biasa menunggu.
- Slot/status, baris arsip/proses/core, frame trace dan event UART rusak tidak menjatuhkan aplikasi; snapshot terakhir dipertahankan sebagai stale/unavailable.
- Konsol, paste satu baris, pause/resume/unduh log, polling panel tertutup, tabel proses/memori serta motion hanya saat event baru. Riwayat setelah reboot tidak dianimasikan sebagai paket baru.
- Lebar 320–2560 px, reduced motion, tanpa overflow dokumen/error JS/request eksternal. Hash komponen React Bits cocok upstream. Seluruh 19 SVG dirender tanpa teks keluar viewBox.

Semua screenshot berdata simulasi: [dashboard](../assets/ui-desktop.png), [HP](../assets/ui-mobile-id.png), [scan](../assets/ui-scan-desktop.png), [diagnostik HP](../assets/ui-diagnostic-mobile.png). Bukan bukti penerimaan hardware.

## Aset dan dokumentasi — lulus

Node 24.22.1 / esbuild 0.28.2 / React 19.3.1 / GSAP 3.15.0. Source, header dan manifest sinkron; total **150.594 byte gzip**, di bawah 160 KiB. Guard Python lolos dalam build PlatformIO; build firmware biasa tidak memerlukan Node. Tes negatif dengan input web tambahan benar-benar dijalankan melalui PlatformIO: guard menolak dengan petunjuk pemulihan tanpa traceback. Input sementara dihapus dan provenance kembali lolos.

Checker memvalidasi **25 Markdown, 221 link/anchor relatif, 19 SVG dan 31 koneksi alternatif manual**, GPIO/versi/lisensi dan provenance. Wiring terkonfirmasi tetap TXD modul → GPIO18, RXD ← GPIO17, GPIO16 tidak dipakai modul otomatis, Fort 58/A dan 59/B. Audit ini tidak mengubah alamat register/skala/terminal.

Packager memeriksa kesamaan app dengan build terakhir, komponen/offset factory, SHA256 seluruh file dan CRC ZIP. Source/docs/notices/tools/manual ikut paket; cache/dependency tidak ikut. Status CI remote harus dibaca dari commit yang benar; hasil lokal bukan klaim CI lulus.

## Batas pengujian

Pembacaan Fort nyata/kesesuaian display, turnaround/noise RS485, OLED/RTC/baterai, tombol/buzzer, LED GPIO48, pemulihan putus daya dan banyak klien AP belum diuji pada instalasi ini. Writer respons HTTP dibatasi, tetapi parser request upstream tidak diganti atau diklaim tahan semua request rusak/besar. Modul empat pin tidak menyediakan sinyal hadir/power-good; UART diam tidak cukup untuk menentukan komponen fisik mana yang hilang/rusak.

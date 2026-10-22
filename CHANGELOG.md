# Changelog

## Dokumentasi/lisensi — 2026-10-22

- Pemilik menetapkan GNU GPLv3 (`GPL-3.0-only`); tambah teks LICENSE, NOTICE, badge lokal dan metadata paket.
- Perjelas judul README dengan kata kunci ESP32, Modbus dan energy monitor.
- Firmware tetap versi 1.0.1; tidak ada perubahan perilaku firmware.

## 1.0.1 — 2026-10-22

- Tambah `AGENTS.md` berisi fakta hardware, invariants firmware, sumber kebenaran dan aturan verifikasi.
- Tulis ulang README dengan badge lokal, preview UI, ilustrasi OLED, diagram wiring SVG dan pemasangan per kabel.
- Verifikasi terminal meter dari scan manual: 58/A dan 59/B; 70/COM adalah switch-input common.
- Tambah generator SVG, netlist CSV, pemeriksaan link/versi/aset, panduan kontribusi, serta workflow CI.
- Perbaiki recovery arsip agar backup tidak dihapus jika restorasi gagal; tambah fault-injection test.
- Batasi percobaan ulang rasio CT/PT setiap minimal lima detik; tambah tes termasuk rollover `millis()`.
- Feed watchdog pada setiap percobaan Modbus agar satu siklus beberapa transaksi lambat tidak memicu reset yang tidak perlu.
- Sediakan versi firmware melalui API; samakan versi rilis, badge dan metadata paket.
- Perbarui paket rilis agar seluruh dokumentasi dan aset lokal ikut tersertakan.

## 1.0.0 — 2026-10-22

- Implementasi awal ESP32-S3 N16R8: AP lokal, dashboard web, OLED, tiga tombol, buzzer dan DS3231.
- Pembacaan Modbus RTU dengan CRC; counter energi sekunder dan rasio CT/PT.
- Enam interval pagi dan tiga interval sore dengan kualitas data.
- Eksekusi dua core, checkpoint NVS, CSV LittleFS, tes native dan simulasi browser.

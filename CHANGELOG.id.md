[English](CHANGELOG.md) · **Bahasa Indonesia**

# Changelog





## Diagnostik mandiri 1.2.0 — 2026-10-26

- Konfigurasi Serial tervalidasi untuk pin/timing UART, probe, daftar/mode scan, rencana raw register, alamat/scale/order dan toggle decode.
- Antrean terbatas mempertahankan perintah saat menunggu; stop membatalkan antrean, prefixHEX tidak memicu stop dan inputinvalid tidak mengubah setting.
- Rombak panduan bilingual menjadi delapan resep, arti hasil dan tindakan berikutnya. Semua resep dieksekusi melalui INO sebenarnya pada tes host.
- Fungsi03/integritas/storage tetap ketat. Firmware utama tetap1.3.0.

## Diagnostik mandiri 1.1.0 — 2026-10-26

- Pisahkan RX pasif tanpa TX, respons protokol dan validasi profil energi; tambahkan basic/intermediate/deeper/diagnose dan baca raw terbatas.
- Scan komunikasi mengenali exception sah tanpa menjadikannya data kWh; CRC/ID/fungsi/panjang tetap ketat.
- Counter atomik parity/framing/FIFO/buffer/break dan tes pada INO sebenarnya dengan Serial/UART tiruan.
- Periksa ulang gambar manual asli dan pertahankan catatan ambiguitas panjang energi. Firmware utama tetap1.3.0.

## 1.3.0 — 2026-10-26

- Capture pasif 12 percobaan Modbus berikutnya dalam RAM tetap, termasuk setting UART, TX/RX hex, overflow dan echo persis. Validasi respons/CRC tidak diubah.
- Kontrol web bilingual untuk mulai/henti/ekspor; polling trace hanya saat panel dibuka. Rekaman bertahan sampai diganti atau restart.
- Sketch Arduino IDE FortSerialDebug tanpa library eksternal: baca kWh/CT/PT, debug Serial terbatas dan scan ID/baud/parity yang bisa dibatalkan. Sertakan layout partisi 16 MB dan build PlatformIO opsional.
- Bedakan modul empat pin arah otomatis dari referensi DE/RE manual. Tambahkan tes native/browser dan kompilasi sketch di CI.

## 1.2.1 — 2026-10-26

- Koreksi default RGB pemasangan ke GPIO48 setelah dikonfirmasi pada board pengguna; GPIO38 tetap dicatat sebagai alternatif revisi board.
- Semua halaman OLED diputar 180° melalui satu variabel master sebelum frame boot pertama.
- Tambahkan LED RGB Mati/Warna tetap/Hue/Kedip/Breathing, brightness default sedang, dan pengiriman RMT asinkron di core 0.
- Tambahkan kontrol web bilingual untuk pola, warna, kecerahan dan durasi siklus. Record LED berversi dengan CRC disimpan core 1; status penyimpanannya terpisah dari pencatatan energi.
- Jelaskan variasi pin GPIO48/GPIO38 serta perbedaan RGB dan RGBW empat kanal. Tambahkan tes native/browser tanpa mengubah polling atau aturan interval meter.

## Pemeliharaan repository — 2026-10-26

- Pertahankan satu target hardware N16R8 dan masukkan hanya image flashing yang ditentukan ke paket.
- Hapus tooling uji lama, arsip rilis usang, cache build, file sementara dan banner yang tidak dipakai.

## 1.2.0 — 2026-10-26

- Dashboard React offline dengan React Bits FadeContent dan SpotlightCard asli yang dikunci commit upstream.
- English sebagai bahasa utama dan pilihan Indonesia persisten, termasuk pesan scan/error.
- Source web terpisah, aset gzip di flash, batas 160 KiB, hash sumber dan runtime Node 24.22.1 yang dikunci.
- Pengiriman aset nonblocking per potongan dengan feed watchdog; tidak menyalin bundle utuh ke heap.
- API menambah kode kualitas/error English, CSV baru dan OLED memakai English; arsip lama serta layout NVS tetap dipertahankan.
- README/panduan English dan Indonesia, diagram boot/RS485, penjelasan task/core/queue/mutex dan batas scheduling.
- Lisensi dependency dan provenance React Bits dicatat terpisah dari kode orisinal GPL-3.0-only.


## 1.1.0 — 2026-10-26

- Tambah Scan Fort manual dari web: rentang ID 1–247, enam baud, parity N/E/O, progres dan pembatalan.
- Konfirmasi kandidat dengan pembacaan energi dan CT/PT; CRC/ID/fungsi/panjang/range wajib valid.
- UART tetap dimiliki core 1; checkpoint interval diputus saat scan, setting lama dipulihkan, hasil diterapkan secara eksplisit.
- Tambah pengujian enumerasi, transisi scan dan kontrol dashboard; tanpa perubahan setting meter.


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

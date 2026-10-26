# Hasil validasi — 26 Oktober 2026

[English](../VALIDATION.md) · **Bahasa Indonesia**

Firmware **1.2.1**. Hasil ini diperoleh dari tes lokal; tidak sama dengan uji penerimaan hardware atau status CI remote. Rincian ukuran tiap aset tersedia di [laporan English](../VALIDATION.md).

## Logika native — lulus

`tools/test.ps1`, g++ C++11 dengan `-Wall -Wextra -Werror`. Pengujian mencakup CRC/manual/frame rusak, sembilan interval, interpolasi, gap dan startup terlambat, reboot/clock/reset, kualitas data hilang, debounce/long press/rollover, retry CT/PT, fault injection filesystem, enumerasi 4.446 kandidat scan, prioritas ID, batal/restart/selesai/ditemukan, rentang satu ID dan kompatibilitas kode kualitas API. Header yang diuji adalah kode yang dipakai firmware; driver ESP-IDF tidak dijalankan oleh tes ini.

## Browser — lulus

Playwright dengan Microsoft Edge melayani **array gzip header firmware**, dengan JSON/CSV API simulasi. English default, pilihan Indonesia/format angka, persistensi reload, fallback storage terblokir, enam/tiga interval, config/time/token/CSV, scan/batal/reload/terapkan/hasil kosong, HTTP error dan timeout request lima detik telah diuji. Data hilang tidak dianggap nol. Status stale/offline dan tombol mutasi saat putus koneksi ikut diperiksa.

Lebar 320–2560 px tidak mengalami overflow dokumen; reduced motion bekerja, tidak ada page error JavaScript atau request aset eksternal. Hash Git blob React Bits sesuai provenance upstream. Seluruh **17 SVG** dirender dengan teks di dalam viewBox. Screenshot desktop/HP diperiksa visual, termasuk [HP Indonesia](../assets/ui-mobile-id.png); semuanya memakai data simulasi.

## Build dan payload — lulus

Node **24.22.1**, esbuild **0.28.2**, React/React DOM **19.3.1**, GSAP **3.15.0**. Build/check web berhasil: total **136669 byte gzip**, di bawah batas 160 KiB. Hash source/header diperiksa oleh guard Python sebelum PlatformIO, sehingga build firmware biasa tidak memerlukan Node.

| Target | RAM statis / 327.680 byte | Program / partisi aplikasi |
| --- | ---: | ---: |
| ESP32-S3 N16R8, flash 16 MB / OPI PSRAM | 47.088 (14,4%) | 996.293 / 6.553.600 (15,2%) |

Build hardware exit code 0, tanpa warning/error compiler. RAM statis tidak mencakup seluruh heap/stack runtime. Kompilasi tidak mengukur latency HTTP atau turnaround UART fisik.

## Dokumentasi dan paket

Checker memeriksa link/anchor English–Indonesia bertingkat, 17 SVG, 31 sambungan netlist, pin, versi, lisensi dan hash web. Paket memuat source React/notice dependency, firmware/panduan/aset; app/factory dibandingkan dengan build, CRC ZIP dan SHA-256 manifest diverifikasi. Cache pip CI sebelumnya mencari file yang salah; workflow sekarang menunjuk requirements-dev.txt dan memeriksa aset React dengan Node terkunci. Status remote dibaca dari Actions untuk commit terkait, bukan diasumsikan dari tes lokal.

## Belum diuji secara fisik

Pengguna mengonfirmasi GPIO48 sebagai pin RGB board pada 26 Oktober 2026. Uji penerimaan hardware lengkap versi 1.2.1 belum dicatat di sini. Log pengguna 23 Oktober menunjukkan firmware sebelumnya boot pada ESP32-S3 tanpa modul, bukan penerimaan versi ini. Scan/scaling Fort, I2C OLED/RTC, baterai, tombol/buzzer, AP multi-client, noise, ketahanan power-cut/endurance masih membutuhkan [commissioning](COMMISSIONING.md).

## Pemeliharaan repository

Setelah target/tooling uji lama dan banner yang tidak dipakai dihapus, `pio run -e esp32s3` lulus dengan RAM statis 47.088 byte dan program 996.293 byte. Seluruh 17 SVG tersisa dirender tanpa overflow. Guard Python untuk hash source/header juga lulus setelah dependency Node lokal dihapus.

Packager memilih lima BIN N16R8 secara eksplisit. Hasil render/build sementara, dependency terpasang dan arsip versi lama dibersihkan setelah verifikasi. Paket hardware terbaru, source/header browser, aset dokumentasi dan manual Fort asli dipertahankan; proses build berikutnya membuat ulang cache yang diperlukan.

## Tambahan verifikasi OLED/RGB 1.2.1

Tes native lulus untuk lima mode LED, scaling brightness, record NVS ber-CRC, kerusakan/panjang/range dan rollover millis. Tes browser byte gzip final lulus untuk payload POST termasuk token, semua mode, draft tetap saat polling, reload status, peringatan storage, driver tidak tersedia, kontrol saat terputus dan label Indonesia. API/NVS browser adalah simulasi, bukan bukti persistensi board.

Build N16R8 final lulus: static RAM 47.240/327.680 byte; program 1.002.577/6.553.600 byte. Payload web 138.395 byte gzip. Tidak ada peringatan kompilator pada log build final. Tampilan RGB desktop/mobile diperiksa secara visual dengan data simulasi. OLED fisik 180°, cahaya GPIO48, timing RMT/jitter dan pengaturan pulih sesudah restart tetap memerlukan commissioning hardware.

## Koreksi dokumentasi GPIO48 — 26 Oktober 2026

Nilai RGB_LED_PIN=48 yang diubah pengguna tetap dipertahankan. README/desain/commissioning, instruksi agent, fallback UI dan screenshot RGB simulasi diselaraskan. Mock browser membaca pin langsung dari board.h; GPIO38 hanya disebut sebagai alternatif revisi board.

Pemeriksaan dokumentasi/hash, tes browser aset firmware, dan build N16R8 lulus tanpa warning kompilator. Ukuran tetap: RAM statis 47.240 byte, program 1.002.577 byte, web gzip 138.395 byte. Binary dan paket diperbarui bersama source. Konfirmasi pin pengguna tidak dianggap uji penerimaan seluruh sistem.

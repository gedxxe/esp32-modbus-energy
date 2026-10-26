# Hasil validasi — 26 Oktober 2026

[English](../VALIDATION.md) · **Bahasa Indonesia**

Firmware **1.3.0**. Hasil ini diperoleh dari tes lokal; tidak sama dengan uji penerimaan hardware atau status CI remote. Rincian ukuran tiap aset tersedia di [laporan English](../VALIDATION.md).

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

Pengguna mengonfirmasi GPIO48 sebagai pin RGB board pada 26 Oktober 2026. Uji penerimaan hardware lengkap versi 1.3.0 belum dicatat di sini. Log pengguna 23 Oktober menunjukkan firmware sebelumnya boot pada ESP32-S3 tanpa modul, bukan penerimaan versi ini. Scan/scaling Fort, I2C OLED/RTC, baterai, tombol/buzzer, AP multi-client, noise, ketahanan power-cut/endurance masih membutuhkan [commissioning](COMMISSIONING.md).

## Pemeliharaan repository

Setelah target/tooling uji lama dan banner yang tidak dipakai dihapus, `pio run -e esp32s3` lulus dengan RAM statis 47.088 byte dan program 996.293 byte. Seluruh 17 SVG tersisa dirender tanpa overflow. Guard Python untuk hash source/header juga lulus setelah dependency Node lokal dihapus.

Packager memilih lima BIN N16R8 secara eksplisit. Hasil render/build sementara, dependency terpasang dan arsip versi lama dibersihkan setelah verifikasi. Paket hardware terbaru, source/header browser, aset dokumentasi dan manual Fort asli dipertahankan; proses build berikutnya membuat ulang cache yang diperlukan.

## Tambahan verifikasi OLED/RGB 1.2.1

Tes native lulus untuk lima mode LED, scaling brightness, record NVS ber-CRC, kerusakan/panjang/range dan rollover millis. Tes browser byte gzip final lulus untuk payload POST termasuk token, semua mode, draft tetap saat polling, reload status, peringatan storage, driver tidak tersedia, kontrol saat terputus dan label Indonesia. API/NVS browser adalah simulasi, bukan bukti persistensi board.

Build N16R8 final lulus: static RAM 47.240/327.680 byte; program 1.002.577/6.553.600 byte. Payload web 138.395 byte gzip. Tidak ada peringatan kompilator pada log build final. Tampilan RGB desktop/mobile diperiksa secara visual dengan data simulasi. OLED fisik 180°, cahaya GPIO48, timing RMT/jitter dan pengaturan pulih sesudah restart tetap memerlukan commissioning hardware.

## Koreksi dokumentasi GPIO48 — 26 Oktober 2026

Nilai RGB_LED_PIN=48 yang diubah pengguna tetap dipertahankan. README/desain/commissioning, instruksi agent, fallback UI dan screenshot RGB simulasi diselaraskan. Mock browser membaca pin langsung dari board.h; GPIO38 hanya disebut sebagai alternatif revisi board.

Pemeriksaan dokumentasi/hash, tes browser aset firmware, dan build N16R8 lulus tanpa warning kompilator. Ukuran tetap: RAM statis 47.240 byte, program 1.002.577 byte, web gzip 138.395 byte. Binary dan paket diperbarui bersama source. Konfirmasi pin pengguna tidak dianggap uji penerimaan seluruh sistem.

## Diagnostik mentah dan sketch mandiri — 1.3.0

Tes native capture/classifier lulus: batas 12, freeze/stop/reset, echo persis, CRC/short/overflow/exception/ID salah dan respons valid. Echo/prefix overflow tidak dianggap data valid. Tes browser byte gzip lulus untuk panel tertutup tanpa polling, capture/stop, ekspor TXT, label bilingual, API gagal dan layout mobile. Screenshot raw memakai fixture simulasi, bukan rekaman Fort pengguna.

Firmware utama lulus build tanpa warning: RAM statis 47.784/327.680 byte, program 1.010.581/6.553.600 byte. Payload web 140.866 byte gzip. Stack high-water mark, latensi empat klien dan sumber echo belum diukur di hardware.

FortSerialDebug.ino berhasil dikompilasi sebagai sketch Arduino dengan core 2.0.17 / espressif32 6.10.0 tanpa dependensi library eksternal. RAM statis sketch 19.372 byte, program 317.213 byte; konfigurasi PlatformIO yang disertakan juga lulus setelah filter hasil konversi INO diperiksa. GUI Arduino IDE, versi core lain, interaksi Serial serta pembacaan Fort fisik belum diuji. Build opsional PlatformIO ditambahkan ke CI dengan target board N16R8 yang sama. Sketch tidak menulis storage; pemilihan partisi/erase uploader dibahas terpisah pada panduan dan CSV yang disertakan.

## Diagnostik mandiri berlapis 1.1.0 — diuji lokal

Gambar manual PDF18,27,30 diperiksa visual; ambiguitas panjang energi tetap dicatat. Sketch lulus kompilasi core2.0.17 tanpa warning, RAM19.396 byte/program320.741 byte. tools/test.ps1 juga menjalankan INO sebenarnya dengan UART/Serial tiruan: RX pasif tanpa TX, respons/exception sah, tolak ID/CRC/echo salah, scan transport terpisah dari profil energi, exception tidak menghasilkan kWh, konversi valid, hanyaFC03, batasregister/abort/counter UART. Ini bukti perilaku kode, bukan bukti sinyal A/B atau mapping hardware sudah benar. Firmware utama tetap1.3.0.

## Diagnostik runtime 1.2.0 — diuji lokal

Suite INO sebenarnya lulus untuk konfigurasi probe/pin/timeout/interval, mode/daftarscan, tolakduplikat/inputinvalid, batasplan, decodeoff tanpaTX/kWh, perubahan alamat/scale/order, antreansaatUARTsibuk, komentar/prefixHEX, overflow/stop serta delapan resepTXT. Semua resep selesai tanpa unknown-command/antreanpenuh; semua request tetap8byteFC03 berCRCvalid dan IDnonbroadcast. Sketch S3 lulus tanpa warning: RAMstatis21.068byte, program323.793byte. Firmwareutama1.3.0/asetbrowser tidak berubah; kualitas sinyal/timing/maphardware belum dibuktikan.

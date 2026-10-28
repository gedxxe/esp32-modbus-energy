[English](CHANGELOG.md) · **Bahasa Indonesia**

# Changelog

## 2.3.2 / 28 Oktober 2026

Jalur TX/RX sekarang memakai titik 2 px dengan puncak halus 3 px, menggantikan batang yang melebar. Kerapatannya mengikuti lebar tampilan, dari 64 sampai 256 titik per jalur. Gerakan, easing saat tiba serta fade 360 ms tetap dipertahankan.

Angka kWh utama kini memakai React Bits Counter yang diberikan, dengan transisi bergulir 650 ms, stagger ringan serta tiga angka desimal. Pemisah angka mengikuti bahasa Inggris/Indonesia, data hilang tetap ditandai tidak tersedia dan reduced motion tetap dihormati. Source cocok dengan komponen yang diberikan, sedangkan animasi mengikuti pembacaan yang diterima tanpa menghasilkan sampel energi tambahan. Regresi gerak browser merekam frame langsung agar tidak bergantung pada keterlambatan pemanggilan dari driver tes.

## 2.3.1 / 28 Oktober 2026

Replay TX dan RX sekarang memakai easing quintic agar kecepatan naik dan turun bertahap. Saat tiba, paket tetap di ujung sementara paket serta gelombang WakeSlider memudar bersama selama 360 ms. Jalur dasar tetap terlihat di belakangnya, sehingga gelombang aktif tidak lagi mendadak rata dalam satu frame. Satu pasangan request/respons diputar sekitar 3,2 detik, sedangkan timestamp UART yang tertangkap tetap memakai timing asli.

Tes regresi memeriksa kontinuitas posisi saat tiba, perubahan opacity paket dan gelombang, TX/RX bergantian serta timeout tanpa respons buatan. Video representatif berakhir setelah replay kembali idle agar rekamannya tidak terpotong saat paket masih bergerak.

## 2.3.0 / 28 Oktober 2026

Panel komunikasi mengganti kedipan paket milidetik serta tap yang saling menabrak dengan dua jalur WakeSlider terpisah. Transaksi selesai yang tercatat memutar TX selama 1,1 detik, berhenti 140 ms lalu memutar RX 1,1 detik hanya jika byte benar-benar tertangkap. Playback mempertahankan satu transaksi aktif dan transaksi terbaru yang menunggu, sementara timestamp asli dan riwayat raw event tetap tersedia. Reboot, sequence hilang, setting serial berubah, halaman tersembunyi serta telemetry terputus membersihkan playback yang belum selesai.

Counter yang diberikan merender jam berbasis RTC dengan digit bergulir, sedangkan StarBorder menghias tombol menu native di atas latar seleksi yang bergeser. Underline lama dihapus. Tiga pasangan JavaScript/CSS cocok dengan hash upstream React Bits terpin, Motion dipin pada 14.7.1, notice dependency tersedia offline dan mode reduced-motion mempertahankan pembacaan serta navigasi statis.

## 2.2.2 / 27 Oktober 2026

XBM Midea yang diberikan pengguna kini menjadi frame OLED pertama selama tiga detik, kemudian dilanjutkan animasi loading 1,5 detik beserta bunyi startup yang sudah ada. Implementasi memakai Adafruit SSD1306/GFX terpasang, mempertahankan piksel dan posisi asli, mengikuti rotasi master 180° serta melewati penahanan logo jika OLED tidak terdeteksi. Durasi diatur melalui `OLED_BOOT_SPLASH_MS` pada board.h, dan generator aset memeriksa hash bitmap asli.

## 2.2.1 / 27 Oktober 2026

Discovery Wi-Fi kini menginisialisasi seluruh konfigurasi scan IDF, menunggu callback scan-done serta mencatat kegagalan driver tanpa mengandalkan deadline scanComplete Arduino empat detik. Policy persiapan dan retry terbatas mencegah benturan dengan association yang belum selesai, mempertahankan daftar SSID terakhir serta menampilkan generation, jumlah percobaan, alasan dan kode driver di web maupun log USB.

Request API browser berbagi satu slot sampai body respons selesai dibaca, dengan deadline request aktif delapan detik dan prioritas Stop/Resume diagnostik. Server melepaskan socket spekulatif kosong saat ada client lain menunggu. Status scan pulih setelah acknowledgement terputus tanpa otomatis membuat scan pengganti.

Panel komunikasi memakai simbol perangkat SVG buatan proyek, jalur TX/RX terpisah, frame berlabel, status terakhir DE-/RE, milestone transaksi serta byte mentah. Timeout tidak membuat animasi RX. CSS menangani transisi halaman sementara React Bits SpotlightCard tetap aktif, sehingga payload dashboard lebih kecil. Regresi mencakup scan gagal, acknowledgement hilang, HTTP lambat serta tampilan timeout/CRC.

## 2.2.0 / 27 Oktober 2026

Pengaturan jaringan memulai scan SSID asinkron dan pemilih jaringan tanpa mengetik SSID manual. Web serta OLED dapat menyimpan pilihan IPv4 AP privat, dengan kompatibilitas record Wi-Fi v1 dan AP pemulihan. Menu OLED mencakup mode, pemilih jaringan, input password tersamarkan serta tools oktet IP.

Pack lossless 772 frame di flash menggantikan referensi animasi yang tersebar. Screensaver diatur lewat web dengan default 60 detik, rentang 10–3600 detik atau nonaktif, dan mengonsumsi tekanan fisik pertama untuk bangun. Tidak ada inisialisasi OLED kedua maupun loop animasi yang menahan akuisisi.

UI memakai gradasi biru/ungu dan panel kaca dengan fallback kontras/transparansi. Tampilan jam maju di antara sampel tanpa penulisan RTC otomatis, dengan pembaruan acuan lima menit atau saat referensi direvisi/kedaluwarsa. Halaman event UART dibatasi delapan entri dengan metadata catch-up serta polling yang menyesuaikan retry. Tes native mencakup pack frame aktual, policy menu, migrasi IP dan bangun/lepas, sedangkan browser memakai byte gzip generated.

## 2.1.0 / 27 Oktober 2026

Dashboard lokal kini menyediakan mode Wi-Fi AP/client/gabungan dengan alamat DHCP, pergantian koneksi tertunda dan AP pemulihan. Kredensial melalui validasi serta record NVS ber-CRC di core 1 sebelum diterapkan driver core 0, dan password tersimpan tidak dikembalikan endpoint status.

Konsol web menampilkan setting sesi dan counter bukti, disertai kontrol register/setting uji serta preset float primer. Perbandingan capture menolak perubahan direction, pin arah dan timing settle/gap. Header serta OLED mengikuti IP aktual, sedangkan panduan web memetakan setiap route API produksi ke antarmukanya. Pengujian native dan byte browser generated mencakup pengaturan baru, kebijakan fallback dan payload rusak.

## 2.0.0 / 2026-10-27

Jalur pengukuran sekarang mengikuti sketch ModbusMaster yang dilaporkan berhasil: DE-/RE manual pada GPIO16, TX17/RX18, settling 100 µs dan request awal 0x0058/dua word. Polling, scanner dan INO memakai satu source FortMeter bersama yang mengganti kolektor gap software. Jalur generator/header lama dipertahankan untuk kompatibilitas, bukan sebagai parser kedua.

Record koneksi v1 ber-CRC sah dimigrasikan dengan ID/baud/parity tetap dan default manual terkonfirmasi. Scanner mengulang kandidat sah, counter sekunder tetap tersedia tanpa CT/PT, serta arus/tegangan/frekuensi langsung ditampilkan sebagai pembanding. Kualitas interval dan rekaman lengkap tetap dilindungi, sedangkan float primer tetap memerlukan perbandingan urutan word secara eksplisit.

Rilis menyertakan ZIP library Arduino, provenance ModbusMaster dan regresi source library sebenarnya, serta menulis ulang panduan English/Indonesia untuk operasi, wiring dan arsitektur. Dashboard menyediakan pilihan direction dan panel register yang mencantumkan alamatnya. Keberhasilan sketch acuan tidak menggantikan penerimaan fisik rilis terintegrasi.

## 1.4.1 / 2026-10-27

- Sinkronkan source web, header dan manifest. Error aset lama dari PlatformIO kini memberi langkah pemulihan tanpa traceback. Build firmware biasa tetap tidak memerlukan Node.
- Tunggu RX tenang secara kontinu sebelum TX. Buang byte lama yang datang selama penantian, tahan TX pada bus terus aktif dan tampilkan hitungan bus sibuk. Perbaiki pemisahan echo/respons pada turnaround minimum serta retry inisialisasi UART gagal.
- Validasi ulang umur/revisi mode perintah di core 1, publikasikan hasil eksekusi dan sediakan jalur prioritas Stop/Resume. Pembatalan memakai generasi pesan dan dequeue atomik, tanpa reset antrean lintas core.
- Lindungi hari tersimpan walau RTC invalid. Kompensasi umur antrean sinkronisasi waktu. Tolak pergantian hari mundur. Batasi pekerjaan antrean setiap putaran.
- Validasi payload browser, pertahankan snapshot terakhir saat respons rusak, blokir sinkronisasi waktu saat maintenance dan tunggu status konsol awal. Pengiriman API kini dibatasi dengan layanan watchdog per potongan.
- Tambah regresi RX terlambat, bus sibuk, turnaround sah, retry UART, mode perintah, RTC dan koneksi HTTP macet. Firmware utama dan diagnostik mandiri 1.3.1 memakai perbaikan transport yang sama. Penerimaan Fort fisik tetap memerlukan uji lapangan.

## 1.4.0 / 2026-10-27

- Semua perintah diagnostik mandiri tersedia di firmware utama melalui satu engine yang juga dipakai polling. Setting/data uji hanya RAM. Resume mengembalikan pin/serial produksi tersimpan.
- Discovery menerima data/exception sah, terpisah dari kompatibilitas energi. Tambah scan format tersimpan, probe/count yang dapat diedit dan matriks penuh eksplisit.
- Echo utuh ditolak sambil menunggu balasan terpisah. Prefix dengan byte tambahan, RX sebelum TX, frame rusak/deadline/overflow dan CRC gagal sebelum respons valid tetap terlihat. Buffer RX dituning tanpa klaim timing waveform.
- Tambah konsol HP, log USB terbatas tanpa blocking, unduh log, mailbox stop/resume dan backpressure antrean tanpa membuang perintah diterima.
- Tambah ring event UART bertimestamp, jejak paket berbasis event tanpa gerak idle, sampling core dan monitor task/heap/PSRAM/stack.
- Perluas tes engine sebenarnya, browser gzip dan panduan bilingual. Kesehatan modul serta penerimaan register Fort terpasang tetap memerlukan bukti hardware.

## Diagnostik mandiri 1.3.0 / 2026-10-26

- Plan raw terkonfigurasi tetap berjalan saat probe pertama diam. Bukti protokol dipisahkan dari decode kWh.
- Tambah observasi RX dengan pull lemah dan capture A/B berlabel operator. Tolak perbandingan konfigurasi berbeda/abort, tanpa vonis fisik palsu.
- Perjelas INPUT Serial Monitor, Enter/Send, serta OUTPUT yang disalin ke chat pada panduan dan delapan resep. Tambah diagram modul otomatis dan koreksi panduan wiring.
- Periksa ulang gambar PDF. Bandingkan raw satu/dua/tiga word pada entri energi ambigu. Tes INO asli diperluas. Firmware utama tetap 1.3.0.

## Diagnostik mandiri 1.2.0 / 2026-10-26

- Konfigurasi Serial tervalidasi untuk pin/timing UART, probe, daftar/mode scan, rencana raw register, alamat/scale/order dan toggle decode.
- Antrean terbatas mempertahankan perintah saat menunggu. Stop membatalkan antrean, prefixHEX tidak memicu stop dan inputinvalid tidak mengubah setting.
- Rombak panduan bilingual menjadi delapan resep, arti hasil dan tindakan berikutnya. Semua resep dieksekusi melalui INO sebenarnya pada tes host.
- Fungsi03/integritas/storage tetap ketat. Firmware utama tetap1.3.0.

## Diagnostik mandiri 1.1.0 / 2026-10-26

- Pisahkan RX pasif tanpa TX, respons protokol dan validasi profil energi. Tambahkan basic/intermediate/deeper/diagnose dan baca raw terbatas.
- Scan komunikasi mengenali exception sah tanpa menjadikannya data kWh. CRC/ID/fungsi/panjang tetap ketat.
- Counter atomik parity/framing/FIFO/buffer/break dan tes pada INO sebenarnya dengan Serial/UART tiruan.
- Periksa ulang gambar manual asli dan pertahankan catatan ambiguitas panjang energi. Firmware utama tetap1.3.0.

## 1.3.0 / 2026-10-26

- Capture pasif 12 percobaan Modbus berikutnya dalam RAM tetap, termasuk setting UART, TX/RX hex, overflow dan echo persis. Validasi respons/CRC tidak diubah.
- Kontrol web bilingual untuk mulai/henti/ekspor. Polling trace hanya saat panel dibuka. Rekaman bertahan sampai diganti atau restart.
- Sketch Arduino IDE FortSerialDebug tanpa library eksternal: baca kWh/CT/PT, debug Serial terbatas dan scan ID/baud/parity yang bisa dibatalkan. Sertakan layout partisi 16 MB dan build PlatformIO opsional.
- Bedakan modul empat pin arah otomatis dari referensi DE/RE manual. Tambahkan tes native/browser dan kompilasi sketch di CI.

## 1.2.1 / 2026-10-26

- Koreksi default RGB pemasangan ke GPIO48 setelah dikonfirmasi pada board pengguna. GPIO38 tetap dicatat sebagai alternatif revisi board.
- Semua halaman OLED diputar 180° melalui satu variabel master sebelum frame boot pertama.
- Tambahkan LED RGB Mati/Warna tetap/Hue/Kedip/Breathing, brightness default sedang, dan pengiriman RMT asinkron di core 0.
- Tambahkan kontrol web bilingual untuk pola, warna, kecerahan dan durasi siklus. Record LED berversi dengan CRC disimpan core 1. Status penyimpanannya terpisah dari pencatatan energi.
- Jelaskan variasi pin GPIO48/GPIO38 serta perbedaan RGB dan RGBW empat kanal. Tambahkan tes native/browser tanpa mengubah polling atau aturan interval meter.

## Pemeliharaan repository / 2026-10-26

- Pertahankan satu target hardware N16R8 dan masukkan hanya image flashing yang ditentukan ke paket.
- Hapus tooling uji lama, arsip rilis usang, cache build, file sementara dan banner yang tidak dipakai.

## 1.2.0 / 2026-10-26

- Dashboard React offline dengan React Bits FadeContent dan SpotlightCard asli yang dikunci commit upstream.
- English sebagai bahasa utama dan pilihan Indonesia persisten, termasuk pesan scan/error.
- Source web terpisah, aset gzip di flash, batas 160 KiB, hash sumber dan runtime Node 24.22.1 yang dikunci.
- Pengiriman aset nonblocking per potongan dengan feed watchdog. Tidak menyalin bundle utuh ke heap.
- API menambah kode kualitas/error English, CSV baru dan OLED memakai English. Arsip lama serta layout NVS tetap dipertahankan.
- README/panduan English dan Indonesia, diagram boot/RS485, penjelasan task/core/queue/mutex dan batas scheduling.
- Lisensi dependency dan provenance React Bits dicatat terpisah dari kode orisinal GPL-3.0-only.


## 1.1.0 / 2026-10-26

- Tambah Scan Fort manual dari web: rentang ID 1–247, enam baud, parity N/E/O, progres dan pembatalan.
- Konfirmasi kandidat dengan pembacaan energi dan CT/PT. CRC/ID/fungsi/panjang/range wajib valid.
- UART tetap dimiliki core 1. Checkpoint interval diputus saat scan, setting lama dipulihkan, hasil diterapkan secara eksplisit.
- Tambah pengujian enumerasi, transisi scan dan kontrol dashboard. Tanpa perubahan setting meter.


## Dokumentasi/lisensi / 2026-10-22

- Pemilik menetapkan GNU GPLv3 (`GPL-3.0-only`). Tambah teks LICENSE, NOTICE, badge lokal dan metadata paket.
- Perjelas judul README dengan kata kunci ESP32, Modbus dan energy monitor.
- Firmware tetap versi 1.0.1. Tidak ada perubahan perilaku firmware.

## 1.0.1 / 2026-10-22

- Tambah `AGENTS.md` berisi fakta hardware, invariants firmware, sumber kebenaran dan aturan verifikasi.
- Tulis ulang README dengan badge lokal, preview UI, ilustrasi OLED, diagram wiring SVG dan pemasangan per kabel.
- Verifikasi terminal meter dari scan manual: 58/A dan 59/B, sedangkan 70/COM adalah switch-input common.
- Tambah generator SVG, netlist CSV, pemeriksaan link/versi/aset, panduan kontribusi, serta workflow CI.
- Perbaiki recovery arsip agar backup tidak dihapus jika restorasi gagal. Tambah fault-injection test.
- Batasi percobaan ulang rasio CT/PT setiap minimal lima detik. Tambah tes termasuk rollover `millis()`.
- Feed watchdog pada setiap percobaan Modbus agar satu siklus beberapa transaksi lambat tidak memicu reset yang tidak perlu.
- Sediakan versi firmware melalui API. Samakan versi rilis, badge dan metadata paket.
- Perbarui paket rilis agar seluruh dokumentasi dan aset lokal ikut tersertakan.

## 1.0.0 / 2026-10-22

- Implementasi awal ESP32-S3 N16R8: AP lokal, dashboard web, OLED, tiga tombol, buzzer dan DS3231.
- Pembacaan Modbus RTU dengan CRC. Counter energi sekunder dan rasio CT/PT.
- Enam interval pagi dan tiga interval sore dengan kualitas data.
- Eksekusi dua core, checkpoint NVS, CSV LittleFS, tes native dan simulasi browser.

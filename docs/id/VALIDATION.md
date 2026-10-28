# Catatan validasi / 28 Oktober 2026

[English](../VALIDATION.md) · **Bahasa Indonesia**

Firmware, FortMeter dan FortSerialDebug menggunakan versi **2.3.0**. Pemeriksaan ini mencakup software rilis terpadu yang mengikuti sketch ModbusMaster dengan GPIO16 ke DE dan /RE. Pengguna melaporkan sketch referensi tersebut berhasil membaca Fort, sedangkan firmware terpadu masih memerlukan penerimaan hardware melalui [COMMISSIONING](COMMISSIONING.md). Riwayat perubahan tersedia di [CHANGELOG](../../CHANGELOG.id.md).

## Kompilasi

Kedua build lulus dengan espressif32 **6.10.0** dan Arduino ESP32 **2.0.17** untuk ESP32-S3 N16R8 fisik, dengan flash 16 MB serta OPI PSRAM. Output compiler akhir tidak memuat warning atau error.

| Build | RAM statis | Program |
| --- | ---: | ---: |
| Firmware utama `esp32s3` | 83.088 byte | 1.397.497 byte |
| FortSerialDebug dengan library FortMeter | 30.284 byte | 336.577 byte |

RAM statis bukan total alokasi runtime. Stack loop core 0 sebesar 16 KiB dan akuisisi sebesar 12 KiB, sedangkan sisa stack, heap dan alokasi driver perlu diperiksa pada perangkat melalui process monitor. INO mandiri kini memakai library FortMeter yang disertakan bersama source ModbusMaster. Build diuji melalui PlatformIO, sementara GUI Arduino IDE serta versi core lain belum dijalankan.

## Pengujian native

Ketiga suite C++11 melalui `tools/test.ps1` lulus dengan `-Wall -Wextra -Werror`. Tes diagnostik mengompilasi implementasi ModbusMaster yang sebenarnya bersama adapter bersama, dan tes mandiri menyertakan INO asli. Stub host menyediakan byte UART dan waktu, sehingga mesin transaksi ModbusMaster tetap dieksekusi.

Kasus transport meliputi request tepat `01 03 00 58 00 02 45 D8`, urutan callback arah manual, respons byte-per-byte berdasarkan panjang yang diharapkan, CRC/ID/fungsi/count salah, input pendek atau berlebih, echo utuh dan exception dengan CRC sah. Tes juga mencakup inisialisasi UART gagal lalu dicoba kembali, write parsial, pembatalan sebelum TX dan selama RX, byte yang datang di deadline, mode otomatis tanpa penulisan pin arah, input antrean serta `resume` Serial yang dijalankan setelah transaksi selesai dibatalkan. Byte terlambat tidak diubah menjadi data diterima hanya karena kemudian membentuk CRC sah.

Tes jaringan mencakup validasi mode/SSID/key, pilihan jaringan terbuka, WPA key 64 digit, record rusak/terpotong, retry, tenggang AP, kehilangan link, mode gabungan, rollover dan subnet tumpang tindih termasuk /28. Helper NetworkPolicy yang sama dipakai oleh firmware. Timestamp injeksi rollover dikonversi eksplisit ke uint32_t agar kasusnya sama pada Windows dan Linux 64-bit. Driver Wi-Fi, DHCP dan kegagalan NVS fisik merupakan jalur integrasi yang ditinjau/dikompilasi, bukan radio yang dijalankan pada host. Regresi capture juga menolak perbedaan direction, pin arah serta timing settle/gap.

Opsi runtime, raw plan, scan terbatas, observasi pull lemah, guard capture berpasangan, delapan resep lapangan dan pembatasan request ke FC03 ikut diuji. Enumerator memeriksa 4.446 kandidat matriks, rentang current-format, prioritas setting, cancel serta rollover. Tes konfigurasi memeriksa migrasi v1 dengan CRC sah ke manual dan pelestarian pilihan otomatis yang sudah tersimpan dalam format v2.

Suite energi dan perangkat tetap lulus untuk sembilan interval, interpolasi/gap, waktu hilang atau invalid, reboot, penurunan counter, perubahan CT/PT, pelestarian interval lengkap, penolakan hari mundur, kompensasi waktu antrean, guard mode/revisi/umur perintah, pemulihan arsip, LED, tombol serta batas trace/event. Writer HTTP diuji dengan progres parsial, tanpa progres, error fatal dan rollover tanpa memperpanjang deadline total.

Tes tersebut membuktikan perilaku software pada kasus injeksi, bukan timing fisik UART, kesehatan modul, noise jalur atau kesesuaian nilai meter. Kepemilikan FreeRTOS ditinjau dan dikompilasi, tetapi helper kebijakan perintah tidak mengeksekusi task hardware secara bersamaan. Respons RTU tidak membawa transaction ID maupun alamat register, sehingga asal semua respons terlambat tidak selalu bisa dipastikan.

## Browser dan diagram

Playwright dengan Microsoft Edge lulus saat menguji **byte gzip dalam header firmware**, menggunakan API simulasi. Cakupannya meliputi EN/ID dan penyimpanan bahasa, fallback storage, seluruh interval, nilai register dan umur sampel, simpan/reload arah manual atau otomatis, scanner, sinkronisasi jam, LED, CSV serta ekspor trace. HTTP 202 tetap menunggu hasil ID eksekusi yang sesuai, Stop prioritas tersedia selama tindakan biasa menunggu, dan perubahan jam diblokir pada maintenance.

Tes browser tambahan mencakup simpan/reload mode AP/client/gabungan, pemakaian ulang password tanpa mengembalikan key tersimpan, pilihan jaringan terbuka, scan/pemilih SSID otomatis, validasi IPv4 AP khusus, tampilan fallback, payload jaringan rusak serta pelestarian draft. Raw read dan setting uji terstruktur mencapai endpoint konsol, setting sesi diperbarui dari pemilik UART, dan payload diagnostik invalid menonaktifkan tindakan biasa sambil mempertahankan Stop.

Payload status, arsip, proses, trace dan event UART yang rusak tidak menjatuhkan UI. Data terakhir ditandai stale/unavailable, polling panel detail berhenti ketika ditutup, dan animasi UART hanya bergerak untuk event baru dengan dukungan reduced motion. Replay timestamp memiliki keterlambatan jaringan dan bukan hasil pengukuran propagasi A/B.

Lebar 320–2560 px lulus tanpa overflow dokumen, error JavaScript atau request aset eksternal. Hash React Bits cocok dengan upstream dan seluruh **24 SVG** lolos render serta pemeriksaan batas teks. [Dashboard](../assets/ui-desktop.png), [tampilan HP](../assets/ui-mobile-id.png), [scanner](../assets/ui-scan-desktop.png) serta [diagnostik HP](../assets/ui-diagnostic-mobile.png) menggunakan data simulasi, sehingga bukan bukti penerimaan hardware.

## Provenance dan paket

Node **24.22.1**, esbuild **0.28.2**, React/React DOM **19.3.1** bersama React Bits SpotlightCard menghasilkan total **134.158 byte gzip**, di bawah 160 KiB. Header dan manifest sinkron, guard hash Python lulus di dalam build firmware, dan build PlatformIO biasa tidak membutuhkan Node. Tidak ada font/CDN eksternal atau source map. Hash source ModbusMaster **2.0.1+fort.1** beserta patch service-hook juga lolos pemeriksaan provenance, dengan header lisensi upstream dipertahankan.

Checker dokumentasi memeriksa 29 panduan Markdown dan link/anchor relatifnya, serta kesesuaian seluruh route web/firmware pada kedua bahasa. Pemeriksaan juga mencakup versi, GPIO, aset generated serta **31 koneksi manual terpasang**. Wiring terkonfirmasi adalah TX17 ke DI, RX18 dari RO dan DIR16 ke DE serta /RE, dengan Fort 58/A dan 59/B. Gambar halaman manual untuk register/skala terkait sudah diperiksa. Ketidakkonsistenan kolom panjang energi dan CRC contoh tercetak dijelaskan di [DESIGN](DESIGN.md).

Packager membandingkan app dengan build terbaru, memeriksa komponen/offset factory, CRC ZIP dan SHA256 setiap file pada manifest. Source, panduan, tools, notice, manual Fort serta ZIP library Arduino ikut disertakan, tanpa cache build atau dependency terpasang. Offset mengikuti [FLASHING](FLASHING.md). CI remote harus diperiksa terhadap commit yang benar karena hasil lokal tidak otomatis menjadi bukti CI lulus.

## Penerimaan fisik yang tersisa

Pergantian fisik AP/client/gabungan, pemulihan kehilangan router, DHCP/subnet serta pembacaan Fort dan kesesuaiannya dengan display, turnaround/noise RS485, OLED/RTC/baterai, tombol/buzzer, LED GPIO48, pemulihan putus daya dan banyak klien AP masih memerlukan pengujian perangkat terpadu. Writer respons HTTP dibatasi, tetapi parser upstream belum dinyatakan tahan semua traffic rusak. Modul tidak menyediakan input presence/power-good, sehingga diamnya UART atau adanya RX lokal tidak cukup untuk memastikan komponen mana yang terlepas atau rusak.

## Cakupan layar dan jaringan pada 2.2

Tes native mencakup perpindahan menu/editor OLED, penolakan IP privat invalid, pembacaan record jaringan lama 104 byte dan record IP khusus baru 108 byte. Policy idle menguji batas timeout, tekanan bangun yang ditahan melewati long press, guard pelepasan, nonaktif serta rollover. Tes menautkan pack frame generated yang sebenarnya, memeriksa decoder terbatas dan menolak output terpotong/berlebih. Generator memeriksa hasil dekompresi seluruh 772 frame dengan SHA256 gabungan, mempertahankan 790.528 byte asli dalam pack lossless 266.739 byte. Import check juga mereproduksi pack dari referensi yang diberikan.

Tes byte browser generated mencakup scan Wi-Fi otomatis/ulang, pemilihan jaringan, draft IP khusus, simpan/reload/nonaktif timeout, payload layar rusak serta jam yang terus maju pada sampel status identik tanpa POST perubahan RTC. UI memakai panel kaca biru/ungu dengan fallback transparansi/kontras. [OLED](OLED.md) menampilkan kontrol representatif serta contoh frame logis, bukan foto hardware.

Target animasi 42 ms, penggunaan bus I2C, latensi bangun fisik, repeat karakter, discovery/association AP dan klien bersamaan selama playback tetap perlu penerimaan perangkat. Kelulusan decoder host atau mock browser tidak membuktikan timing fisik. Alasan retry event UART menjelaskan ketersediaan HTTP, bukan bukti elektris Fort.

## Koreksi Wi-Fi dan komunikasi pada 2.2.1

Build utama final memakai 83.088 byte RAM statis dan 1.378.569 byte program. Sketch mandiri tetap memakai 30.284 byte RAM statis serta 336.577 byte program. Keduanya selesai tanpa warning/error compiler. Ketiga suite native lulus setelah pembaruan versi diagnostik final, termasuk batas persiapan, retry habis, deadline radio/keseluruhan dan rollover unsigned scanner.

Tes browser byte generated lulus untuk pemulihan acknowledgement terputus, daftar SSID yang bertahan saat radio gagal, HTTP sementara gagal, respons tertunda 3,1 detik dan scan berulang. Regresi UART mencakup label direction, simbol vektor, raw frame, CRC mismatch, timeout tanpa RX buatan serta riwayat reboot. Modul request aktual diuji melalui Node dengan body respons yang sengaja ditahan, pembatalan GET mengantre, preemption GET oleh Stop serta perlindungan POST yang sudah diterima saat Resume masuk. Seluruh regresi dashboard dan 23 pemeriksaan vektor lulus pada Microsoft Edge terpasang dengan lebar 320–2560 px.

GSAP 3.15.0 tetap dipin untuk referensi FadeContent yang dipertahankan, tetapi tidak diimpor pada bundle runtime saat ini. Transisi halaman memakai CSS ringan dan React Bits SpotlightCard tetap dipakai.

Adapter radio menginisialisasi struktur scan IDF yang dipin dan menerima callback scan-done Arduino melalui mailbox RTOS. Radio nyata, pelayanan AP selama scan antarkanal, DHCP association dan penanganan socket kosong sudah dikompilasi serta ditinjau, dengan policy atau perilaku browser yang diuji pada host. Rilis ini belum menjalani penerimaan radio fisik pada kombinasi board/router yang terpasang.

## Logo boot pada 2.2.2

Build utama lulus dengan RAM statis 83.088 byte dan program 1.380.281 byte, bertambah 1712 byte program tanpa menambah RAM statis. Ketiga suite native lulus dengan source OLED generated yang sebenarnya. Generator memverifikasi 1350 byte XBM terhadap SHA256 bitmap yang diberikan, dan urutan LSB-first dicocokkan dengan implementasi Adafruit GFX `drawXBitmap()` terpasang. Render piksel logis sudah diperiksa secara visual, dan seluruh 24 SVG lolos pemeriksaan render serta batas teks browser.

Ukuran dan SHA256 seluruh payload web sama dengan 2.2.1, sedangkan yang berubah hanya provenance versi/input. Regresi dashboard lengkap sebelumnya tetap berlaku, dengan pemeriksaan vektor baru serta modul request yang kembali lulus lokal. Urutan setup aktual sudah ditinjau: rotasi dan transfer frame pertama mendahului alokasi objek RTOS, penahanan 3000 ms tetap yield serta melayani RGB, lalu loading/bunyi lama 1500 ms dijalankan. OLED yang tidak terdeteksi melewati penahanan logo. Timing/orientasi OLED fisik serta waktu sampai AP tersedia tetap perlu diterima pada perangkat.

## Gerak browser yang terbaca pada 2.3.0

Build terpadu lulus dengan RAM statis 83.088 byte dan program 1.397.497 byte. Akuisisi, transport Modbus, GPIO, kepemilikan RTC serta logo boot dipertahankan. Ketiga suite native lulus, termasuk INO aktual serta source ModbusMaster terpin. Bundle browser offline berukuran 134.158 byte gzip, di bawah batas 163.840 byte.

Pasangan JavaScript/CSS WakeSlider, Counter dan StarBorder yang diberikan dibandingkan dengan commit upstream e12c22192ac7b4b452c5e2feb9b586d74765d696 sebelum disalin tanpa perubahan. Hash blob Git diperiksa guard preview byte generated, Motion dipin pada 14.7.1 dan empat notice MIT paket Motion tersedia offline. Hash input dan hasil generated sinkron.

Assembler frame aktual lulus untuk pengelompokan valid/CRC/timeout, sequence terputus, pin direction berubah, hasil tanpa request, reset, kegagalan UART lokal serta rollover sequence uint32. Suite browser lulus pada 320–2560 px, unit jam bergulir tanpa POST RTC, mount WakeSlider/StarBorder aktual, penghapusan underline/tap, TX/RX bergantian serta timeout tanpa frame kembali. Sampel posisi paket setelah 320 ms menunjukkan gerak maju tetapi belum sampai ujung, menggantikan kedipan di bawah 100 ms. Highlight menu dibatasi saat viewport berubah agar transisinya tidak melebarkan dokumen.

Regresi scanner, Wi-Fi, diagnostik, Stop/Resume, arsip, LED, proses, data stale, bahasa Inggris/Indonesia serta reduced-motion lulus. MP4 representatif 5,23 detik berisi 127 frame browser yang berubah dari UI yang dibuild dengan API simulasi, dienkode pada 30 fps. Video ini merupakan dokumentasi dan tidak disajikan ESP32. Pemeriksaan memvalidasi perilaku browser serta kompilasi, bukan penerimaan fisik pada HP, OLED dan meter yang terpasang.

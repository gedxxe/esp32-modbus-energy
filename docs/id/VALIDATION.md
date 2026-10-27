# Catatan validasi / 27 Oktober 2026

[English](../VALIDATION.md) · **Bahasa Indonesia**

Firmware, FortMeter dan FortSerialDebug menggunakan versi **2.1.0**. Pemeriksaan ini mencakup software rilis terpadu yang mengikuti sketch ModbusMaster dengan GPIO16 ke DE dan /RE. Pengguna melaporkan sketch referensi tersebut berhasil membaca Fort, sedangkan firmware terpadu masih memerlukan penerimaan hardware melalui [COMMISSIONING](COMMISSIONING.md). Riwayat perubahan tersedia di [CHANGELOG](../../CHANGELOG.id.md).

## Kompilasi

Kedua build lulus dengan espressif32 **6.10.0** dan Arduino ESP32 **2.0.17** untuk ESP32-S3 N16R8 fisik, dengan flash 16 MB serta OPI PSRAM. Output compiler akhir tidak memuat warning atau error.

| Build | RAM statis | Program |
| --- | ---: | ---: |
| Firmware utama `esp32s3` | 80.904 byte | 1.134.809 byte |
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

Tes browser tambahan mencakup simpan/reload mode AP/client/gabungan, pemakaian ulang password tanpa mengembalikan key tersimpan, pilihan jaringan terbuka, batas byte SSID UTF-8, tampilan fallback, payload jaringan rusak serta pelestarian draft. Raw read dan setting uji terstruktur mencapai endpoint konsol, setting sesi diperbarui dari pemilik UART, dan payload diagnostik invalid menonaktifkan tindakan biasa sambil mempertahankan Stop.

Payload status, arsip, proses, trace dan event UART yang rusak tidak menjatuhkan UI. Data terakhir ditandai stale/unavailable, polling panel detail berhenti ketika ditutup, dan animasi UART hanya bergerak untuk event baru dengan dukungan reduced motion. Replay timestamp memiliki keterlambatan jaringan dan bukan hasil pengukuran propagasi A/B.

Lebar 320–2560 px lulus tanpa overflow dokumen, error JavaScript atau request aset eksternal. Hash React Bits cocok dengan upstream dan seluruh **21 SVG** lolos render serta pemeriksaan batas teks. [Dashboard](../assets/ui-desktop.png), [tampilan HP](../assets/ui-mobile-id.png), [scanner](../assets/ui-scan-desktop.png) serta [diagnostik HP](../assets/ui-diagnostic-mobile.png) menggunakan data simulasi, sehingga bukan bukti penerimaan hardware.

## Provenance dan paket

Node **24.22.1**, esbuild **0.28.2**, React/React DOM **19.3.1** dan GSAP **3.15.0** menghasilkan total **157.191 byte gzip**, di bawah 160 KiB. Header dan manifest sinkron, guard hash Python lulus di dalam build firmware, dan build PlatformIO biasa tidak membutuhkan Node. Tidak ada font/CDN eksternal atau source map. Hash source ModbusMaster **2.0.1+fort.1** beserta patch service-hook juga lolos pemeriksaan provenance, dengan header lisensi upstream dipertahankan.

Checker dokumentasi memeriksa 27 panduan Markdown, 174 link/anchor relatif, serta kesesuaian seluruh route web/firmware pada kedua bahasa. Pemeriksaan juga mencakup versi, GPIO, aset generated serta **31 koneksi manual terpasang**. Wiring terkonfirmasi adalah TX17 ke DI, RX18 dari RO dan DIR16 ke DE serta /RE, dengan Fort 58/A dan 59/B. Gambar halaman manual untuk register/skala terkait sudah diperiksa. Ketidakkonsistenan kolom panjang energi dan CRC contoh tercetak dijelaskan di [DESIGN](DESIGN.md).

Packager membandingkan app dengan build terbaru, memeriksa komponen/offset factory, CRC ZIP dan SHA256 setiap file pada manifest. Source, panduan, tools, notice, manual Fort serta ZIP library Arduino ikut disertakan, tanpa cache build atau dependency terpasang. Offset mengikuti [FLASHING](FLASHING.md). CI remote harus diperiksa terhadap commit yang benar karena hasil lokal tidak otomatis menjadi bukti CI lulus.

## Penerimaan fisik yang tersisa

Pergantian fisik AP/client/gabungan, pemulihan kehilangan router, DHCP/subnet serta pembacaan Fort dan kesesuaiannya dengan display, turnaround/noise RS485, OLED/RTC/baterai, tombol/buzzer, LED GPIO48, pemulihan putus daya dan banyak klien AP masih memerlukan pengujian perangkat terpadu. Writer respons HTTP dibatasi, tetapi parser upstream belum dinyatakan tahan semua traffic rusak. Modul tidak menyediakan input presence/power-good, sehingga diamnya UART atau adanya RX lokal tidak cukup untuk memastikan komponen mana yang terlepas atau rusak.

# Tinjauan implementasi versi 2.3.3

[English](../AUDIT.md) · **Bahasa Indonesia**

Pengguna melaporkan sketch ModbusMaster berhasil dengan transceiver manual dan DIR16. Default instalasi sebelumnya memakai arah otomatis, sementara receiver khusus bergantung pada gap yang diamati task untuk menyelesaikan frame. V2 mengubah kedua bagian agar mengikuti acuan berhasil, tanpa mengklaim bahwa satu perbedaan saja telah terbukti menjadi penyebab seluruh kegagalan sebelumnya.

## Transport dan source bersama

Akuisisi, probe scanner dan INO memakai library FortMeter yang sama. ModbusMaster membentuk request serta mengoleksi panjang respons yang diharapkan, dengan callback manual sesuai acuan. Adapter merekam I/O, membatasi drain/capture serta memeriksa FC03 dan CRC secara menyeluruh, termasuk exception. Echo atau byte tambahan tidak dihapus diam-diam untuk membuat hasil yang diinginkan.

Source upstream dikunci pada v2.0.1 dan patch hook layanan disimpan bersama hash serta lisensi. Tes native mengompilasi source itu dengan INO sebenarnya atau class diagnostik bersama, bukan menggantinya dengan implementasi ModbusMaster tiruan. Jalur include/generator lama dipertahankan untuk kompatibilitas tanpa parser aktif kedua.

## Akuisisi, migrasi dan antarmuka

Query kontak pertama memakai contoh arus 0x0058/dua word. Exception sah mempertahankan bukti kontak protokol dan membiarkan nilainya kosong, sehingga pembacaan profil lain dapat dilanjutkan. Counter sekunder tersedia tanpa rasio, tetapi interval produksi tetap memerlukan CT/PT sah yang diterapkan sekali serta waktu/kontinuitas yang diterima. Float primer tetap perbandingan eksplisit dan bukan fallback otomatis.

Record koneksi v1 ber-CRC sah mempertahankan setting serial serta dimigrasikan ke arah manual. Format hari dan LED tetap, interval lengkap dilindungi dan format filesystem membutuhkan konfirmasi fisik. Dashboard menyediakan direction serta pembanding register, memisahkan Stop/Resume dari penantian kontrol biasa dan memvalidasi baris sebelum dirender. Wiring serta penjelasan teknis mengikuti instalasi manual terkonfirmasi.

## Integrasi web dan Wi-Fi

Aplikasi lokal kini menyediakan setting sesi, counter bukti, raw read terstruktur, kontrol setting uji dan perintah float primer bersama scanner, trace, flow serta process monitor. Mode AP/client/gabungan mengikuti persistensi core 1 dan driver core 0, dengan validasi kredensial, penundaan aplikasi serta AP pemulihan. IP header/OLED mengikuti antarmuka aktual. Perbandingan capture juga menolak perbedaan direction dan timing settle/gap. [Panduan web](WEB.md) memetakan setiap route API ke antarmuka dan pemiliknya.

## Batas yang masih ada

Timestamp UART merupakan observasi software, bukan pengukuran t1.5/t3.5 atau propagasi A/B. Respons Modbus tidak membawa transaction ID/alamat register sehingga asal seluruh balasan tertunda tidak selalu dapat dipastikan dari byte. Tidak ada input presence/power-good modul, sehingga UART diam, CRC gagal atau RX lokal berulang tidak meluluskan kesehatan komponen fisik.

Interpolasi interval dibatasi pada gap sah 15 detik dan resolusi counter bergantung CT/PT. Tes checkpoint/backup mencakup jalur gagal terdefinisi, bukan seluruh kemungkinan flash atau putus daya. AP/HTTP tidak memiliki login/TLS terpisah, cloud, NTP ataupun OTA dan parser request upstream masih perlu diuji bebannya pada perangkat. [VALIDATION](VALIDATION.md) mencatat hasil software aktual, sedangkan [COMMISSIONING](COMMISSIONING.md) memuat penerimaan hardware terintegrasi.

Integrasi 2.2 menambahkan scan Wi-Fi asinkron, editor jaringan OLED, record IP khusus yang kompatibel serta policy animasi idle persisten. Referensi animasi dikonversi lossless dengan decoder terbatas tanpa display/loop kedua. Input bangun dikonsumsi sebelum aksi tombol biasa. Animasi jam terpisah dari mutasi RTC, dan halaman API mempertahankan validasi sequence/kualitas dengan respons event yang lebih kecil. Timing radio, menu fisik dan animasi bersama banyak klien tetap perlu diuji pada perangkat.

Koreksi 2.2.1 mengganti policy start/completion Wi-Fi, menserialisasi body API browser, membatasi penantian socket spekulatif serta menampilkan alasan dan kode driver. Panel komunikasi mempertahankan milestone teknis dan raw frame dengan label arah berbasis event. Regresi browser mencakup acknowledgement hilang, pembacaan lambat, radio gagal, CRC serta timeout tanpa RX buatan. Scheduler aktual juga diuji untuk body completion, pembatalan dan Stop prioritas. Perubahan radio serta HTTP tetap perlu diterima pada perangkat.

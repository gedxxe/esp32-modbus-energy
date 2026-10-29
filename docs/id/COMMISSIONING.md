# Commissioning dan uji penerimaan

[English](../COMMISSIONING.md) · **Bahasa Indonesia**

Contoh khusus Fort di bawah tetap berlaku untuk Fort. Untuk Acrel, simpan model yang dipilih terlebih dahulu lalu ikuti [Profil meter](METER_PROFILES.md) untuk register, terminal, konversi dan alur diagnostiknya.

Pengguna melaporkan sketch ModbusMaster berhasil dengan transceiver manual dan DIR16. Jadikan koneksi tersebut baseline firmware 2.6.0, lalu catat pembacaan rilis terintegrasi secara terpisah dari tes software atau screenshot simulasi.

## Penerimaan checkpoint

Sebelum penerimaan di lapangan, cocokkan tanggal RTC serta WIB dengan acuan operator/meter. Biarkan logger menyala melewati checkpoint nyata dengan HP terputus dan router tidak tersedia, kemudian hubungkan belakangan untuk membandingkan angka kumulatif tersimpan, timestamp aktual serta CSV dengan observasi meter. Sepuluh jadwal diuji lewat injeksi jam pada tes native, tanpa memundurkan jam produksi untuk membuat rekaman lampau.

Setelah mengekspor rekaman hari uji, lakukan power-cycle terencana dan periksa bahwa baris tersimpan tidak berubah. Meter yang tidak tersedia sepanjang jendela lima detik harus menghasilkan Terlewat, bukan angka yang diambil belakangan. Periksa arsip saat hari berganti, unduhan hari sebelumnya, tanggal pada kedua halaman capture OLED serta peringatan storage tanpa menimpa hari yang belum diarsipkan. Pemeriksaan fisik ini melengkapi tes CRC/fault native dan mock browser.

## Uji meja sebelum ke panel

Pastikan board boot tanpa Serial Monitor, menyediakan Midea Engineering AP pada 192.168.4.1 dan menampilkan Firmware 2.6.0. Periksa orientasi OLED 180°, tiga tombol dengan debounce, buzzer opsional serta RGB setelah restart. OLED yang tidak ada tidak boleh menahan AP, sedangkan filesystem gagal hanya boleh diformat setelah konfirmasi fisik OK lima detik.

Saat A/B dilepas, jalankan diagnosis terbatas dan simpan output. RX pada kondisi itu berasal dari jalur lokal. Loopback UART sementara dapat dibuat setelah kabel TTL modul dilepas, dengan menghubungkan GPIO17 ke GPIO18. Hasilnya harus dikenali sebagai echo request dan ditolak sebagai energi. Lepas jumper serta pulihkan modul manual sebelum tes Fort.

## Meter Fort

Gunakan TX17→DI, RX18←RO, DIR16→DE+/RE, A→58 dan B→59, lalu samakan menu CONi. Pada Command input web, masukkan `status` dan `intermediate` terpisah dengan Run command untuk setiap baris. Simpan TX/RX serta hasil 0x00 pada request manual 0x0058. Bandingkan `reg 0042 2`, `reg 006A 2` dan `reg 0080 1` dengan halaman tegangan, daya dan frekuensi yang sesuai.

Periksa `reg 0082 2` terhadap counter impor sekunder dan validasi PT numerator/denominator serta CT sebelum menerima konversi primer. `primary` hanya menyediakan kandidat word order float untuk dibandingkan, bukan pilihan otomatis berdasarkan angka finite. Exception ber-CRC sah membuktikan kontak protokol dan belum meluluskan pembacaan data atau skala.

## Scan, interval dan pemulihan

Uji ID yang diketahui pada format tersimpan sebelum memperluas rentang. Hasil memerlukan dua transaksi sah, tidak diterapkan otomatis dan mempertahankan direction saat disimpan. Pembatalan perlu memutus penantian dan memulihkan koneksi. Counter error tidak dipakai sebagai sensor hadirnya komponen.

Benarkan waktu HP sebelum sinkronisasi, lalu periksa WIB terhadap RTC serta setelah restart. Catat batas interval pagi/petang yang lengkap, interval yang sengaja terputus dan pelestarian baris lengkap. RTC invalid, perubahan rasio, gap atau reset counter tidak boleh menghasilkan konsumsi lengkap. Ekspor CSV serta pemulihan backup saat uji putus daya perlu diverifikasi secara terencana.

## Catatan penerimaan

Simpan versi/commit, modul dan mode wiring, setting meter, word mentah, perbandingan display, RTC, sampel interval/CSV serta headroom heap/stack. Persentase core merupakan bantuan sampling tick, bukan profiler instruksi, dan latensi beberapa klien AP tetap perlu diuji pada perangkat. [VALIDATION](VALIDATION.md) memisahkan penerimaan tersebut dari hasil compiler/native/browser lokal.

Dari panel Wi-Fi web, uji AP, client dan mode gabungan pada jaringan 2,4 GHz yang diketahui. Catat IP DHCP, tenggang AP 15 detik pada mode client serta fallback saat router diputus. Periksa pemakaian ulang password, penolakan kredensial invalid, pemulihan setelah reboot dan subnet router yang bertabrakan dengan 192.168.4.0/24. Pastikan polling UART berjalan selama reconnect lalu kembalikan mode deployment yang dipilih.

Atur screensaver OLED sepuluh detik dari web, tunggu animasi, lalu bangunkan dengan masing-masing tombol. Tahan tombol bangun lebih dari 900 ms dan pastikan tidak terjadi mute/navigasi/simpan, kemudian lepas dan tekan lagi untuk kontrol normal. Periksa rotasi 180°, persistensi timeout setelah reboot, opsi nonaktif serta respons akuisisi/HTTP selama satu putaran animasi. Uji mode, pemilih SSID/password dan tools IP AP OLED sebelum menetapkan setting deployment.

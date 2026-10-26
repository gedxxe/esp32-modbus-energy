[English](../COMMISSIONING.md) · **Bahasa Indonesia**

# Commissioning dan uji penerimaan

## Uji meja sebelum ke panel

1. Periksa model board, pin GPIO dan suplai modul. Tes OLED+RTC melalui 3,3 V, lalu tombol dan buzzer melalui driver.
2. Flash firmware. OLED menampilkan animasi, bunyi boot berlangsung sekitar 1,5 detik, lalu IP 192.168.4.1 muncul di halaman utama.
3. Buat filesystem lewat prosedur tahan OK saat boot bila diperlukan. Pastikan status storage berubah OK setelah waktu RTC valid.
4. Sambungkan HP ke hotspot dengan kredensial pada README. Pastikan dashboard berfungsi tanpa internet dan tanpa CDN.
5. Sinkronkan waktu. OLED dan web harus menunjukkan WIB. Matikan daya ESP32 selama beberapa menit, nyalakan, lalu pastikan DS3231 tetap berjalan dengan baterai.
6. Tekan tombol cepat: satu siklus tekan/lepas hanya berpindah satu halaman/interval. Tahan OK: mute hanya berubah sekali, bukan berulang. Periksa pola alarm ketika RTC atau meter offline.

## Meter Fort

1. Hubungkan ESP32 sebagai satu-satunya master pada port Modbus Fort yang masih kosong. Ambil foto/nota setting komunikasi meter.
2. Cek slave ID, baud, parity pada menu meter. Firmware mulai ID 1, 9600, 8N1. Setting yang sebenarnya bisa berbeda walaupun manual dari box yang sama.
3. Hubungkan pasangan RS485, gunakan reference/proteksi sesuai panel. Jika tidak ada respons, cek A/B, arah RO/DI, DE-/RE, ground reference, terminasi, dan setting komunikasi. Jangan terus mengganti alamat register saat masalahnya ada pada kabel.
4. Lihat CRC/timeout. Keduanya tidak harus nol seumur hidup, tetapi pertambahan terus menerus saat uji steady-state harus diselidiki.
5. Bandingkan **energi impor positif primer** dashboard dengan halaman energi impor yang sesuai di Fort. Jangan membandingkan terhadap net energy, energi ekspor, kvarh, atau penjumlahan energi dua arah.
6. Catat rasio CT/PT dari meter dan bandingkan dengan rasio web. Jika angka berbeda tepat faktor CT/PT, hentikan penerimaan data dan verifikasi tipe register serta setting meter.
7. Bandingkan kW terhadap display meter. Saat beban stabil, catat perubahan kWh selama 10–30 menit. Contoh 12 kW rata-rata selama 0,5 jam menghasilkan kira-kira 6 kWh, dengan toleransi kelas meter dan resolusi counter.

## Penerimaan interval

1. Nyalakan paling lambat 05.55 WIB untuk pagi dan 16.25 WIB untuk sore; waktu dan komunikasi harus valid sebelum batas pertama.
2. Catat foto counter sekitar 06.00, 06.30, dan 07.00. Setelah batas terlewati, cek selisih terhadap CSV dengan mempertimbangkan waktu foto, interval polling, dan resolusi counter.
3. Verifikasi seluruh enam interval pagi dan tiga sore, termasuk dua interval bersebelahan yang berbagi batas waktu.
4. Saat satu interval berjalan, lepas koneksi RS485 >15 detik. Data harus ditandai parsial; jangan tampil sebagai hasil interval lengkap.
5. Ulangi dengan reboot di tengah interval. Hasil interval selesai yang tersimpan harus tetap ada; interval yang terputus harus parsial.
6. Sinkronkan RTC di tengah interval. Pastikan interval aktif parsial; pengujian ini sengaja memutus kesinambungan.
7. Uji perubahan tanggal hanya pada bench dengan data uji. Pastikan tanggal lokal WIB digunakan; hasil kemarin dapat diunduh melalui arsip.
8. Uji mati daya beberapa kali di sekitar akhir interval dan saat penyimpanan. Unduh CSV sebelum/sesudah untuk memeriksa pemulihan. Jangan melakukan format sebagai langkah awal jika arsip tampak hilang.

## Belum dianggap lulus hardware sampai

- Skala, arah energi, urutan word, dan setting komunikasi cocok dengan unit Fort fisik.
- OLED, semua tombol, buzzer, AP, RTC battery backup, dan filesystem bekerja.
- Dashboard tetap responsif ketika meter offline/retry dan beberapa HP membuka halaman.
- Uji interval pagi/sore serta gangguan daya/komunikasi memenuhi perilaku di atas.
- Panel, kabel, isolasi, suplai dan pemasangan disetujui teknisi fasilitas.

## Uji Scan Fort (firmware 1.2.1)

1. Di luar interval produksi, catat setting Fort dan setting pembaca yang lama. Buka Koneksi meter → Scan Fort.
2. Untuk uji cepat, batasi ID awal/akhir pada ID meter yang diketahui. Jalankan Scan Fort. Periksa progres, web dan OLED tetap merespons; CRC/timeout scan terpisah dari penghitung pembacaan normal.
3. Saat hasil ditemukan, pastikan ID/baud/parity sesuai menu meter. Pembaca tetap memakai setting lama sebelum Terapkan hasil; setelah diterapkan, reload dashboard dan reboot untuk memastikan setting NVS bertahan.
4. Jalankan ulang lalu Batalkan scan. Pastikan pembacaan kembali dengan setting lama, tidak ada perubahan menu Fort. Reload halaman di tengah scan: progres harus kembali terlihat dari status perangkat.
5. Uji meter dilepas/rentang ID salah: scan selesai tanpa hasil; jangan menganggapnya bukti kerusakan meter. Periksa A/B dan koneksi.
6. Scan di tengah interval uji harus menghasilkan interval parsial, dengan interval selesai tetap tersimpan. Hasil scan tidak bertahan setelah reboot dan tidak dijalankan otomatis.

Rentang penuh 1–247 memiliki 4.446 kandidat; kondisi tanpa respons dapat membutuhkan sekitar 70 menit. Tombol batal diproses antarkandidat; kandidat terdiri dari maksimal dua transaksi 800 ms, dengan watchdog/yield di jalur UART.

## Pemeriksaan UI bilingual

Pastikan English muncul pertama kali, pilihan Indonesia bertahan setelah reload, dan error/scan ikut diterjemahkan. Uji navigasi saat scan, unduh CSV dari Perangkat & arsip, lalu putuskan HP dari AP: status harus menunjukkan data lama dan tombol mutasi dinonaktifkan. React dan animasi dieksekusi browser; uji ini tidak membuktikan timing UART atau performa radio pada perangkat fisik.

## Uji posisi OLED dan RGB

1. OLED terpasang terbalik harus terbaca pada boot dan semua halaman. OLED_ROTATE_180=false hanya untuk posisi normal. Jangan format arsip hanya untuk mencoba prompt.
2. Pastikan GPIO LED board sesuai: default 48 sudah dikonfirmasi pada pemasangan ini; revisi DevKitC v1.1 dapat memakai 38. Periksa hue awal, brightness 128, siklus 8 detik.
3. Terapkan semua pola melalui Perangkat & arsip, warna primer, brightness 0/128/255 serta siklus 0,5/30 detik. Cocokkan cahaya fisik dengan status terapan.
4. Reload browser lalu restart board; pilihan tersimpan harus kembali setelah boot. Cahaya boot masih memakai default compile. Pastikan tidak ada peringatan penyimpanan.
5. Ulangi saat polling, scan dan transfer aset. Amati jitter, respons UART dan watchdog; tes software belum mengukur timing pada hardware.

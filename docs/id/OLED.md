# Kontrol OLED dan pengaturan jaringan

[English](../OLED.md) · **Bahasa Indonesia**

SSD1306 terpasang diputar 180° melalui setting board sebelum frame pertama, termasuk layar recovery. PREV memakai GPIO4, NEXT GPIO5 dan OK GPIO6, masing-masing terhubung ke GND saat ditekan dengan pull-up internal. Debounce stabil 35 ms dan long press 900 ms tetap digunakan.

## Membuka pilihan jaringan

Gunakan PREV/NEXT sampai halaman **NETWORK / WIFI**, lalu tekan OK untuk membuka **NETWORK OPTIONS**. PREV/NEXT memilih Wi-Fi mode, Scan/select SSID, AP IPv4 address, AP password atau Back. OK singkat membuka pilihan, sedangkan menahan OK kembali ke menu atau membuka tools ketika sedang mengedit password/IP.

![Ilustrasi menu jaringan](../assets/oled-network.svg)

Pada **Wi-Fi mode**, pilih AP only, Client + AP recovery atau AP + client lalu tekan OK. Mode client memerlukan SSID yang sudah dipilih atau tersimpan, sehingga konfigurasi kosong meminta scan terlebih dahulu. Perubahan ditolak ketika scanner Modbus, maintenance, simpan lain atau scan Wi-Fi sedang memakai operasi terkait. Tombol dan web mengirim konfigurasi yang sama ke core 1, sehingga task tombol tidak menulis Preferences sendiri.

## Memindai dan menghubungkan SSID

Pilih **Scan / select SSID**, tunggu proses radio selesai, lalu gunakan PREV/NEXT untuk memilih jaringan yang terlihat. OLED menunjukkan level sinyal dan keamanannya. OK memilih SSID tersebut. Jaringan terbuka dapat langsung dihubungkan, SSID berpassword yang sama dengan koneksi tersimpan memakai key lama, dan jaringan baru berpassword membuka editor. Autentikasi enterprise dengan identitas/sertifikat belum tersedia.

Pada editor, PREV/NEXT mengganti karakter ASCII tercetak dan OK singkat menambahkan karakter terpilih. Tahan PREV/NEXT untuk pengulangan lebih cepat setelah 450 ms. Password tetap disamarkan dengan batas input 63 karakter. Tahan OK untuk membuka tools, lalu pilih Save and connect, Delete last, Clear password atau Cancel. Simpan memerlukan key 8–63 karakter. Key WPA heksadesimal 64 digit dapat dimasukkan melalui web.

Simpan yang berhasil dijadwalkan untuk diterapkan setelah delapan detik. Operasi tidak kompatibel atau kegagalan simpan memberikan pesan pada OLED dengan koneksi aktif tetap dipertahankan. Halaman jaringan menunjukkan IP client/AP setelah perubahan, dan AP pemulihan tetap tersedia ketika percobaan client gagal. OLED yang tidak terpasang tidak menghambat startup web.

## Mengganti IP AP

Pilih **AP IPv4 address**. PREV/NEXT mengubah oktet aktif, OK singkat berpindah oktet, dan tahan OK membuka Save AP address, Reset 192.168.4.1 atau Cancel. Pengulangan cepat juga berlaku pada oktet. Alamat harus berupa host IPv4 privat di 10.x.x.x, 172.16–31.x.x atau 192.168.x.x dengan oktet akhir 1 hingga 254. Subnet AP tetap /24 dan gateway memakai alamat AP sendiri.

Simpan memakai perpindahan delapan detik. Sambungkan ulang HP ke hotspot agar mendapat lease DHCP baru, kemudian buka IP yang baru. Jika subnet LAN bertabrakan dalam mode gabungan, alamat AP runtime dapat berpindah dan ditampilkan pada OLED/web sementara alamat pilihan tetap tersimpan.

## Kontrol lain

Di luar pilihan jaringan, PREV/NEXT tetap berpindah di lima halaman utama. OK singkat mengganti baris interval pada halaman pagi/petang. Tahan OK di luar halaman Network mengubah mute alarm, sedangkan OK pada Network membuka opsinya. Konfirmasi format filesystem lima detik tetap hanya tersedia setelah mount gagal saat boot dan tidak digantikan menu jaringan.

Editor tidak mencetak key client ke output diagnostik, ekspor atau status HTTP. [WEB](WEB.md) menjelaskan kontrol browser yang setara, sedangkan [VALIDATION](VALIDATION.md) memisahkan tes host menu dari penerimaan tombol, OLED, radio dan DHCP fisik.

## Animasi idle dan cara bangun

Secara default, animasi wajah berjalan setelah 60 detik tanpa input fisik. Buka **Perangkat & arsip → Screensaver OLED**, aktifkan/nonaktifkan animasi, pilih waktu idle 10–3600 detik, lalu simpan. Core 1 menulis setting dalam record terpisah delapan byte ber-versi dan CRC. OLED yang belum terpasang tidak menghalangi penyimpanan untuk boot berikutnya.

![Ilustrasi frame animasi idle](../assets/oled-screensaver.svg)

Tekan dan lepas tombol fisik mana pun untuk membangunkan OLED. Tekanan pertama beserta pelepasannya hanya untuk bangun, termasuk jika ditahan, sehingga tidak berpindah halaman, mengubah mute atau menyimpan opsi. Layar kembali ke dashboard monitoring yang menampilkan IP, lalu tekanan berikutnya bekerja normal. Buffer password yang belum selesai dibersihkan saat bangun. Proses penerapan jaringan atau scan Wi-Fi menahan layar monitoring agar tetap terlihat.

Sebanyak 772 frame logis 128×64 menargetkan jeda 42 ms, dengan frame tertinggal dilewati tanpa loop catch-up yang memblokir. Satu bitmap kerja 1 KiB didekode dari flash dan dikirim melalui instance SSD1306 serta mutex I2C yang sudah ada, mengikuti setting master rotasi 180°. Modbus dan pencatatan tetap berjalan di core 1. Pack terkompresi berukuran 266.739 byte dari 790.528 byte mentah, dengan SHA256 hasil decode keseluruhan diperiksa saat build.

`firmware/assets/oled/idle.rle` dan `idle.json` menjadi aset serta metadata utama. Setelah sengaja mengubah aset, jalankan `python tools/build-oled.py` pada terminal komputer di root repo, lalu build ulang firmware. Pack C++ generated menggantikan header referensi yang tersebar dan memakai komentar English. Referensi asli disimpan lokal pada output/reference yang diabaikan Git, di luar source deployment.

Contoh kontrol browser dengan status perangkat simulasi: [desktop](../assets/ui-display-desktop.png), [HP](../assets/ui-display-mobile.png). Untuk mengimpor urutan header baru, jalankan `python tools/import-oled.py PATH_TO_REFERENCE` pada terminal komputer di root repo sebelum `python tools/build-oled.py`.

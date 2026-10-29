# Koneksi WPA2-Enterprise dan pengisian sertifikat

[English](../ENTERPRISE_WIFI.md) · **Bahasa Indonesia** · [Panduan dashboard](WEB.md)

Station menyediakan pilihan client WPA2-Enterprise dengan PEAP dan EAP-MSCHAPv2, berdampingan dengan profil Personal serta jaringan terbuka yang dipilih secara eksplisit. ESP32-S3 memindai jaringan 2,4 GHz. Label Enterprise menunjukkan keamanan 802.1X yang diumumkan AP, tetapi hasil scan tidak dapat menentukan metode EAP luar maupun dalam. Pilih PEAP jika sesuai dengan konfigurasi jaringan, seperti pada instalasi yang dilaporkan.

Konfigurasi Windows yang dilaporkan untuk Midea adalah WPA2-Enterprise, AES, PEAP dan EAP-MSCHAPv2 dengan validasi server aktif. Informasi itu cukup untuk menentukan metode client, tetapi belum memberikan CA tepercaya atau izin akses perangkat ini. Firmware tidak berisi akun, CA atau nama RADIUS Midea yang ditebak. Pengujian software tidak dinyatakan sebagai bukti berhasil masuk ke jaringan perusahaan.

![Kepemilikan task dan alur kepercayaan Enterprise](../assets/enterprise-wifi.svg)

## Runtime dan pemilihan API

Firmware 3.0.0 berpindah dari Arduino-ESP32 2.0.17 / ESP-IDF 4.4.7 ke Arduino-ESP32 3.3.11 / ESP-IDF 5.5.5 yang dipin. PlatformIO memakai [pioarduino 55.03.311](https://github.com/pioarduino/platform-espressif32/releases/tag/55.03.311), integrasi build komunitas yang memaketkan core Arduino dan SDK Espressif. URL arsip platform dipin pada kedua project target fisik. Requirement pengembangan Python memakai PlatformIO 6.1.19 dengan dependency SCons 4.8.1 yang sesuai.

API EAP publik lama belum menyediakan kontrol nama server dan pembatasan metode yang digunakan di sini. [API EAP Espressif](https://docs.espressif.com/projects/esp-idf/en/v5.5.5/esp32s3/api-reference/network/esp_wifi.html) menyediakan `esp_eap_client_set_domain_name()` serta `esp_eap_client_set_eap_methods()`. Adapter membatasi metode luar ke `ESP_EAP_TYPE_PEAP`, memasang CA yang diberikan dan mengaktifkan pemeriksaan waktu sertifikat. [Konfigurasi EAP peer SDK terpin](https://github.com/espressif/esp-idf/blob/v5.5.5/components/wpa_supplicant/src/eap_peer/eap.c) menggunakan `auth=MSCHAPV2` sebagai default phase 2. Periksa kembali konfigurasi itu ketika SDK dinaikkan, tanpa menganggap selector phase 2 TTLS sebagai API khusus PEAP.

Setiap hasil setup EAP diperiksa sebelum asosiasi dimulai. CA yang gagal dimuat, kegagalan memasang nama server atau API yang tidak mendukung konfigurasi akan memblokir percobaan. Tidak ada tombol untuk menonaktifkan validasi, percobaan Personal sebagai pengganti Enterprise, fallback CA publik atau perpindahan metode EAP otomatis. Saat beralih ke Personal, state EAP sebelumnya dinonaktifkan terlebih dahulu.


Platform memakai `custom_sdkconfig` untuk mengompilasi SDK dengan `CONFIG_MBEDTLS_HAVE_TIME_DATE=y` dan pemeriksaan key usage sertifikat tetap aktif. SDK precompiled bawaan mematikan pemeriksaan tanggal, sehingga mengatur boolean EAP saja belum cukup. Guard kompilasi menolak konfigurasi tersebut. Build pertama mengompilasi komponen SDK serta aplikasi. Pada path Windows yang mengandung spasi, `tools/pio_paths.py` menjalankan `tools/sdk_bootstrap.py` untuk mengompilasi SDK yang sama di cache PlatformIO tanpa spasi. Helper memeriksa konfigurasi hasil kompilasi dan hash library sebelum menggunakannya kembali. Hanya metadata konfigurasi yang disalin ke project, sedangkan aplikasi produksi tetap dikompilasi dari sumber aslinya. Aplikasi uji tidak menggantikan firmware rilis. Header konfigurasi proyek bernama `network_settings.h` agar tidak berbenturan dengan `Network.h` Arduino 3 pada Windows.
## Asal sertifikat dan isian yang diperlukan

Sertifikat CA adalah berkas publik dari organisasi atau otoritas sertifikat yang mengesahkan server Wi-Fi. Berkas ini berbeda dari password akun dan tidak membutuhkan private key. Windows yang sudah dikonfigurasi perusahaan mungkin menyimpannya sehingga dapat diekspor. Profil Wi-Fi juga dapat mencatat fingerprint CA meskipun sertifikat yang cocok tidak tersedia di penyimpanan yang bisa dibaca alat ekspor. Fingerprint saja tidak dapat diubah menjadi sertifikat. Jika berkasnya memang tidak tersedia, sumbernya adalah pengelola jaringan Wi-Fi tersebut, bukan sertifikat baru buatan sendiri atau unduhan sembarang.

Pada Windows yang sudah memiliki profil Wi-Fi kantor, jalankan di **PowerShell pada direktori utama repo**. Ganti contoh nama profil dengan hasil `netsh wlan show profiles`:

```powershell
./tools/export-wifi-trust.ps1 -ProfileName 'Nama profil Wi-Fi kamu'
```

Helper hanya mengekspor CA publik yang cocok dengan referensi SHA1 profil dan menulis `output/wifi-trust/report.json`. Script tidak membuka password, mengekspor private key atau mengubah koneksi Wi-Fi. Exit code 2 berarti sertifikat yang dirujuk tidak tersedia, sehingga belum ada PEM yang bisa diunggah. Laporan tetap berada di luar source dan paket rilis. Jika CA ditemukan, gunakan PEM yang sesuai pada pengaturan lanjutan setelah memeriksa laporan. Batas firmware tetap 4096 byte dan empat CA untuk setiap unggahan.

Kamu tidak perlu mengetahui hostname RADIUS untuk memakai form. **Nama DNS server bersifat opsional.** Jika diisi, namanya ikut diperiksa bersama rantai CA dan tanggal sertifikat. Jika dikosongkan, CA yang dipasang tetap menjadi batas kepercayaan, tetapi autentikasi tidak dibatasi ke satu nama server tertentu. Firmware tidak menebak hostname dari SSID atau alamat email.

| Isian | Nilai dan makna |
| --- | --- |
| SSID dan jangkauan radio | Jaringan berizin yang terjangkau pada 2,4 GHz di lokasi receiver. AP yang hanya memakai 5 GHz tidak dapat digunakan ESP32-S3 ini. |
| Autentikasi | Konfirmasi PEAP dengan inner EAP-MSCHAPv2 untuk akun/perangkat tersebut. Form ini belum mengimplementasikan metode EAP lain. |
| Identitas luar | Secara default memakai username. Buka pengaturan lanjutan hanya jika jaringan memerlukan identitas luar atau identitas anonim yang berbeda. |
| Username dan password dalam | Kredensial berizin dalam format yang diminta IT. Tiap field menerima maksimal 127 byte UTF-8 tanpa karakter kontrol. |
| CA tepercaya | CA atau rantai sertifikat Base-64 PEM, maksimal 4096 byte dan empat sertifikat CA. Ekspor DER ke PEM sebelum unggah. Sertifikat leaf server dan private key ditolak. |
| Nama DNS sertifikat RADIUS | Batasan nama tambahan yang opsional. Jika dipakai, isi hostname sertifikat tanpa wildcard, URL, port atau daftar nama. Kosongkan jika profil jaringan tidak menentukannya. |
| Kebijakan perangkat | Izin MAC ESP32, akun perangkat, segmen jaringan serta receiver tanpa operator. Laptop karyawan yang berhasil terhubung belum membuktikan izin tersebut. |

UI menerima satu nama DNS opsional. Jaringan yang memerlukan daftar beberapa nama membutuhkan implementasi allow-list berikutnya, bukan nama domain organisasi yang ditebak.

## Langkah melalui website lokal

1. Hubungkan HP atau laptop ke **Midea Engineering AP** yang diproteksi, lalu buka alamat IPv4 pada OLED. Pengisian Enterprise hanya diterima melalui request yang ditujukan ke antarmuka AP. Dashboard HTTP yang ada belum menyediakan TLS end-to-end, sehingga pengubahan kredensial Enterprise melalui antarmuka client perusahaan ditolak.
2. Buka **Perangkat & arsip → Koneksi Wi-Fi**. Pilih **AP + client** selama commissioning agar dashboard lokal tetap terjangkau setelah asosiasi. Pilihan **Client + AP pemulihan** dapat menutup AP setelah koneksi stabil dan mempertahankannya kembali ketika client tidak tersedia.
3. Pilih SSID perusahaan dari daftar scan. Labelnya menampilkan WPA2-Enterprise beserta RSSI dan keluarga keamanan yang diumumkan AP. Pilih **WPA2-Enterprise · PEAP / EAP-MSCHAPv2** setelah metode itu dikonfirmasi IT.
4. Isi username dan password. Identitas EAP luar secara default mengikuti username. CA untuk jaringan tersimpan dipertahankan bila sudah tersedia. Jika belum ada, buka **Pengaturan Wi-Fi lanjutan** dan unggah atau tempel CA publik PEM sekali. Nama DNS server opsional dan identitas luar yang berbeda juga berada di bagian tersebut. Browser tidak mengambil kembali kredensial tersimpan, dan kunjungan berikutnya dapat menggunakannya kembali untuk SSID yang sama.
5. Pastikan jam perangkat valid. DS3231 menyimpan UTC dan dashboard menampilkan WIB. Bila diperlukan, gunakan tindakan pengaturan waktu yang memang eksplisit dengan RTC terpasang. RTC yang hilang atau invalid memblokir percobaan Enterprise, tanpa mematikan pemeriksaan tanggal sertifikat. Waktu jaringan belum bisa menyelesaikan autentikasi pertama karena jaringan belum tersedia. Selama profil Enterprise aktif, core 0 memperbarui referensi jam sistem dari snapshot RTC valid setiap lima menit atau setelah revisi waktu eksplisit. Pembaruan ini menjaga waktu TLS tanpa menulis RTC atau mengganti pemilik jam checkpoint.
6. Pilih **Simpan pengaturan Wi-Fi** dan tunggu hasil ID eksekusi yang sesuai. Core 1 memverifikasi file CA serta konfigurasi NVS sebelum core 0 menerapkannya setelah jeda delapan detik yang sudah ada. Setelah terhubung, catat IP client dan RSSI. Pindahkan browser ke jaringan tersebut hanya jika kebijakannya mengizinkan akses ke ESP32.

Password baru yang kosong tidak berarti mengizinkan fallback terbuka. Pergantian SSID menghapus pilihan penggunaan ulang kredensial di browser. Checkbox **Pertahankan CA tepercaya yang tersimpan** bersifat eksplisit, dan status hanya menampilkan fingerprint SHA256 beserta nama server yang diharapkan. Refresh atau penutupan halaman tidak memasukkan kredensial EAP ke localStorage.

Pergantian nama server tepercaya atau CA juga mewajibkan pengisian ulang kredensial EAP. Checkbox penggunaan ulang tidak dapat mengarahkan kredensial tersimpan ke target kepercayaan berbeda.

[Form desktop](../assets/ui-enterprise-desktop.png) dan [form HP](../assets/ui-enterprise-mobile.png) memakai respons API serta kredensial uji simulasi. Keduanya merupakan contoh antarmuka, bukan bukti login perusahaan.

## Penyimpanan, kepemilikan dan retry

Record Wi-Fi versi 3 memakai struktur CRC16 eksplisit 779 byte. Isinya meliputi mode autentikasi, field EAP terbatas, IP AP serta referensi SHA256 CA. Record Personal/open lama berukuran 104 dan 108 byte tetap dibaca. CA adalah material kepercayaan publik dan disimpan terpisah di LittleFS sebagai `/wifi-ca-<sha256>.pem`, sehingga sertifikat besar tidak memenuhi partisi NVS yang sudah ada. Maksimal delapan file CA immutable dipertahankan. Konfigurasi yang belum terverifikasi tidak diterapkan dan file lama dipertahankan untuk investigasi. Filesystem yang gagal tidak diformat otomatis.

Core 1 memiliki penulisan kredensial dan persistensi CA. Queue terpisah satu item membawa sertifikat baru yang dibatasi ukurannya, sementara perintah biasa membawa token request dan field koneksi. CA harus dibaca kembali serta divalidasi sebelum record kredensial diganti. Core 0 memiliki driver Wi-Fi/EAP dan membaca sertifikat dengan lock filesystem. Definisi register, jadwal checkpoint dan kepemilikan UART dipertahankan.

Rilis ini tidak mengaktifkan enkripsi flash, enkripsi NVS, Secure Boot atau membakar eFuse secara otomatis. Build pengembangan menyimpan kredensial NVS tanpa enkripsi kredensial dan tidak menjanjikan perlindungan dari pembacaan flash secara fisik. Indikator enkripsi flash saat runtime belum membuktikan bahwa key NVS diprovisikan dengan aman. Gunakan akun perangkat yang disetujui IT dan build keamanan terpisah bila diwajibkan kebijakan produksi.

Asosiasi berjalan asinkron. Personal mempertahankan retry 30 detik, sedangkan Enterprise memberi jarak 60 detik agar pertukaran PEAP memiliki waktu lebih panjang. Scan radio memiliki station sampai selesai, sehingga retry asosiasi menunggunya. AP tersedia selama kegagalan, dan profil client-only mempertahankan grace period 15 detik setelah memperoleh alamat. Timer ini tidak menjeda Modbus atau pencatatan lokal. Setup sertifikat serta write flash merupakan operasi konfigurasi terbatas, bukan loop yang menunggu Wi-Fi terhubung.


Penolakan Enterprise dengan alasan 23 atau 202 menjeda percobaan asosiasi otomatis agar password tersimpan yang salah tidak terus dicoba setiap menit. Periksa login dan pengaturan kepercayaan, lalu pilih Simpan pengaturan Wi-Fi untuk satu percobaan baru memakai kredensial tersimpan atau yang diperbaiki. Jeda tetap berlaku setelah scan, sedangkan kehilangan jangkauan dan timeout handshake mengikuti kebijakan reconnect. Penolakan belum membuktikan field autentikasi mana yang gagal.

## Menafsirkan kegagalan

| Pengamatan | Makna dan tindakan berikutnya |
| --- | --- |
| `enterprise_clock_required` | Referensi RTC valid yang masih baru belum tersedia. Periksa RTC dan atur waktunya sebelum percobaan Enterprise berikutnya. |
| `enterprise_ca_missing` | CA rujukan tidak tersedia atau gagal parse/hash. Isi ulang lewat AP lokal dan pertahankan file yang ada untuk investigasi. |
| `enterprise_api_failed` | Salah satu API setup EAP gagal. Catat kode ESP beserta versi firmware. Tidak ada autentikasi yang lebih lemah sebagai pengganti. |
| Autentikasi gagal, alasan 23 atau 202 | Stack melaporkan kegagalan autentikasi. Kredensial, kepercayaan/nama/waktu sertifikat, metode EAP atau kebijakan dapat menjadi penyebabnya. Kode disconnect saja belum membedakannya. |
| SSID tidak tersedia, alasan 201 | Target tidak ditemukan saat asosiasi. Periksa jangkauan 2,4 GHz, SSID dan kebijakan AP di lokasi receiver kantor sebenarnya. |
| Handshake timeout atau putus berulang | Catat alasan, RSSI dan jumlah percobaan. Minta IT mencocokkan MAC/waktu perangkat dengan log RADIUS/AP tanpa langsung menyimpulkan password salah. |

Status memuat phase, IP, RSSI, jumlah percobaan, alasan disconnect dan kode API. Serial hanya mencetak kode percobaan/alasan yang sudah disanitasi, tanpa kredensial EAP atau isi sertifikat. Jika perlu dianalisis, salin Serial Monitor **OUTPUT** ke chat dukungan. Alur pengisian website ini tidak memerlukan perintah yang diketik ke Serial Monitor.

## Penerimaan hardware dengan akun uji IT

Gunakan form koneksi website untuk tiap percobaan dan simpan Serial OUTPUT beserta kode alasannya. Dengan referensi RTC valid, pastikan CA/nama/akun yang disahkan mencapai DHCP, sementara CA salah, nama server salah, sertifikat kedaluwarsa dan sertifikat yang belum berlaku ditolak tanpa alamat client. RTC hilang/invalid harus memblokir percobaan sebelum EAP dimulai. Metode luar yang tidak didukung tidak boleh memicu metode pengganti, dan profil Personal harus tetap berfungsi setelah keluar dari Enterprise. Gunakan akun uji IT untuk kasus gagal, tanpa mengulang password karyawan berkali-kali.

Setelah itu, hilangkan lalu pulihkan jangkauan AP sambil memeriksa checkpoint lokal. Pastikan AP pemulihan tetap terjangkau dan status DHCP yang terlambat dibedakan dari client yang sudah memperoleh alamat. Percobaan fisik ini masih diperlukan dan tidak digantikan suite browser.

## Receiver kantor yang direncanakan

Topologi yang diinginkan menempatkan logger RS485 di ruang panel dan receiver Wi-Fi terpisah di kantor, dengan transport SX1278 di antaranya. Rilis ini menambahkan autentikasi Wi-Fi opsional pada station yang sudah ada. Framing LoRa, acknowledgement, wiring radio dan client server/database belum diimplementasikan. Lapisan tersebut nantinya dapat membaca status jaringan tanpa mengambil alih EAP atau memblokir logger, setelah protokol dan hardware-nya ditentukan.

Tes browser memakai respons jaringan simulasi, sedangkan build target fisik mengompilasi API Espressif yang sebenarnya. Parse CA, penolakan sertifikat pada RADIUS uji, autentikasi Midea, DHCP, roaming/reconnect dan lalu lintas AP bersamaan tetap membutuhkan penerimaan perangkat dengan konfigurasi IT yang berizin. Suite software yang lulus belum membuktikan akses ke jaringan perusahaan.

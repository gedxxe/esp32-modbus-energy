# Menggunakan dashboard lokal

[English](../WEB.md) · **Bahasa Indonesia**

Firmware 2.2.0 menyajikan seluruh aplikasi dari ESP32, termasuk pengukuran, scanner koneksi, konsol diagnostik ModbusMaster, traffic mentah, metrik proses dan pengaturan Wi-Fi. INO mandiri tetap menjadi alat opsional untuk diagnostik USB, sehingga fitur terbaru tidak mengharuskan flash sketch lain atau memakai Serial Monitor. Pergantian Wi-Fi tidak mengubah setting meter.

## Membuka aplikasi dan memilih Wi-Fi

Pada instalasi baru, hubungkan HP ke **Midea Engineering AP** memakai **betahdijre12345**, kemudian buka **http://192.168.4.1**. Password AP mengikuti [board.h](../../firmware/include/board.h). Bahasa awal adalah English dan dapat diganti ke Indonesia melalui pemilih bahasa. Jika ESP32 menjadi client, gunakan IP DHCP dari router yang ditampilkan pada halaman jaringan OLED maupun panel Wi-Fi.

Buka **Perangkat & arsip → Koneksi Wi-Fi**, pilih mode, pilih SSID dari scan otomatis jaringan 2,4 GHz lalu isi password, kemudian pilih **Simpan pengaturan Wi-Fi**. Password menerima 8–63 karakter ASCII tercetak atau WPA key heksadesimal 64 digit. Jaringan terbuka harus dipilih melalui checkbox khusus. Kolom password kosong mempertahankan key lama hanya untuk SSID yang sama, dan password tersimpan tidak pernah dikirim kembali lewat API status atau dimasukkan ke storage browser. Form ini tidak menyediakan identitas atau sertifikat untuk jaringan enterprise.

| Mode | Jalur akses yang tersedia |
| --- | --- |
| Access point saja | Koneksi langsung ke hotspot Midea, dengan kredensial client tetap tersimpan untuk digunakan kembali |
| Client dengan pemulihan AP | AP selama percobaan koneksi, kemudian IP client setelah DHCP berhasil dan masa tenggang AP 15 detik berakhir |
| Access point + client | Kedua antarmuka menyajikan aplikasi serta API yang sama |

Perintah simpan masuk ke core 1 untuk disimpan dalam record NVS ber-CRC. Jika penyimpanan gagal, koneksi aktif dipertahankan. Setelah berhasil, core 0 menunggu delapan detik sebelum menerapkan mode agar browser sempat membaca hasil eksekusi. Percobaan client tidak menahan task akuisisi, dicoba kembali setiap 30 detik, dan AP tetap tersedia selama belum tersambung. AP muncul kembali jika client kehilangan koneksi. Proses ini tidak memformat storage, menghapus interval atau me-reboot perangkat.

![Mode Wi-Fi dan pemulihan](../assets/network-modes.svg)

AP dan client memakai satu radio, sehingga AP mengikuti kanal router dalam mode gabungan. Jika subnet LAN bertabrakan, firmware memilih alamat AP yang tidak tumpang tindih dari 192.168.4.1, 192.168.5.1, 10.42.0.1 atau 172.23.42.1. Baca alamat terbaru setelah perubahan jaringan. HP pada router harus diizinkan mengakses client LAN lain. Antarmuka HTTP lokal yang sama menjadi tersedia pada LAN tersebut, dengan token mutasi per boot dan tanpa akun/TLS tersendiri.

## Memilih jaringan, IP khusus dan OLED

Pemilih SSID memulai scan otomatis ketika cache belum tersedia atau berumur lebih dari 30 detik. Pindai lagi memperbaruinya secara eksplisit. Firmware menjalankan scan radio asinkron di core 0, menyimpan maksimal 16 jaringan terlihat dengan sinyal/keamanan, melepas hasil driver dan membiarkan akuisisi Modbus di core 1. Jaringan terbuka dan personal yang didukung dapat dipilih, sedangkan enterprise/WEP/keamanan lain yang belum didukung dinonaktifkan. SSID tersembunyi tidak ditampilkan oleh pemilih jaringan terlihat ini.

Kolom alamat AP menerima host IPv4 privat dengan subnet tetap /24. Simpan mempertahankan alamat pilihan sebelum menerapkannya sesudah delapan detik. Hubungkan ulang HP ke hotspot untuk memperbarui DHCP dan buka IP baru. Jika subnet LAN bertabrakan, alamat runtime AP dapat berbeda dan ditampilkan terpisah dari alamat pilihan. Record Wi-Fi v1 tetap terbaca dengan default 192.168.4.1, lalu simpan berikutnya memakai format record baru.

Halaman Network OLED kini membuka opsi mode, scan/pemilih SSID, editor password tersamarkan dan editor oktet IP. Simpan memakai antrean core 1 yang sama dengan browser. [OLED](OLED.md) menjelaskan alur tiga tombol beserta tools long press.

## Tampilan jam

Label jam sebelumnya baru maju ketika respons status 2,5 detik diterima, bukan karena RTC terus disetel dari HP. Sekarang tampilan berjalan dari acuan UTC perangkat memakai waktu monotonic browser. Acuan diperbarui setiap lima menit atau segera setelah revisi jam, koneksi pulih maupun RTC kembali valid. Polling pengukuran tetap berjalan seperti biasa, dan penyetelan RTC dari HP tetap merupakan tindakan eksplisit. Gangguan status singkat tidak mereset tampilan jam, tetapi setelah 15 detik tanpa pembaruan koneksi valid nilainya ditandai tidak tersedia.

OLED juga memperkirakan detik tampilan dari sampel RTC valid terakhir. Perhitungan tersebut hanya untuk tampilan, sedangkan batas interval tetap memakai pembacaan RTC tervalidasi oleh pemilik akuisisi. Keduanya tidak menulis waktu otomatis maupun mengganti acuan jam tracker.

## Menjalankan pipeline meter dari web

Mulai dari **Ringkasan** untuk melihat arus, tegangan, frekuensi, counter sekunder dan energi impor primer. Nilai primer memerlukan CT/PT yang valid dan diterapkan sekali, sedangkan counter sekunder tetap dapat dilihat ketika validasi rasio belum selesai. Umur sampel serta kualitas interval menunjukkan data stale atau terputus, dan animasi komunikasi mengikuti event UART tanpa menciptakan paket ketika jalur diam.

Pada **Koneksi meter**, cocokkan ID/baud/parity dengan CONi dan pilih arah manual untuk instalasi GPIO16. Uji ID yang diketahui dengan format tersimpan sebelum memperluas scanner. Probe awal memakai FC03 0x0058 sebanyak dua word, dengan dua balasan data atau exception ber-CRC sah sebelum kandidat ditawarkan. Terapkan hasil hanya menyimpan koneksi pembaca, sedangkan penerimaan register dan skala energi diperiksa tersendiri.

Buka **Konsol diagnostik** untuk melihat setting dari task pemilik UART dan hasil observasi yang memisahkan data sah, exception, CRC, input tidak lengkap, echo, timeout dan error lokal. Pemeriksaan `status`, `report` serta `help` tidak menghentikan produksi. Pembacaan aktif atau perubahan setting uji memasuki maintenance dan memutus kontinuitas interval berjalan, dengan nilai uji tetap di luar pencatatan.

Gunakan **Baca register langsung** untuk memasukkan alamat PDU heksadesimal dan satu/dua/tiga word, kemudian klik **Baca word mentah**. Bagian **Ubah setting uji** menyediakan ID, format, direction, timing, probe, profil dan daftar scan. Terapkan satu perubahan sebelum pembacaan berikutnya, lalu periksa output jika argumennya ditolak. Preset **Float primer** mengisi `primary` yang setelah **Jalankan perintah** akan membandingkan kandidat ABCD/CDAB tanpa memilih otomatis.

Perintah lain tetap dapat diketik satu baris pada **Input perintah**, lalu klik **Jalankan perintah**. **Hentikan tes** tetap tersedia selama penantian, sedangkan **Lanjutkan pengukuran** membatalkan pekerjaan dan mengembalikan pin, arah serta koneksi produksi tersimpan. Unduh output sebelum meninggalkan panel agar frame mentah dan hasil analisis tersimpan. [DIAGNOSTICS](DIAGNOSTICS.md) menjelaskan perintah dan delapan resep, termasuk keterbatasan membedakan kesehatan fisik dari observasi UART.

## Peta integrasi

| Fitur web | Jalur HTTP | Pemilik dan hasil |
| --- | --- | --- |
| Pengukuran, kualitas, koneksi tersimpan dan Wi-Fi | `/api/status` | Snapshot akuisisi dan status jaringan core 0 |
| Koneksi meter dan scanner | `/api/config`, `/api/scan/start`, `/api/scan/cancel`, `/api/scan/apply` | Eksekusi core 1 melalui pembaca FC03 bersama |
| Setting uji, raw read, float dan resep diagnostik | `/api/diagnostic` GET/POST | FortDiagnostics core 1, FortTransport sama dengan produksi dan INO |
| Replay arah serta frame UART | `/api/uart` | Ring event software dengan informasi gap sequence |
| Capture dan ekspor transaksi mentah | `/api/trace`, `/api/trace/start`, `/api/trace/stop` | Salinan trace terbatas tanpa melewati validasi CRC |
| Core/task/heap/PSRAM | `/api/system` | Sampling tick dan metrik task milik aplikasi/idle |
| Mode jaringan, IP AP khusus dan kredensial | `/api/network` | Persistensi core 1 lalu pesan satu slot ke state machine Wi-Fi core 0 |
| Scan SSID terdekat | `/api/network/scan` GET/POST | Scan radio asinkron core 0 dan cache jaringan terlihat terbatas |
| Setting animasi idle OLED | `/api/display` | Preferensi layar ber-CRC di core 1 dan playback/bangun di core 0 |
| LED, mute dan jam RTC | `/api/led`, `/api/mute`, `/api/time` | Kontrol antrean dengan hasil eksekusi |
| Arsip dan CSV | `/api/history`, `/api/export` | Akses filesystem terbatas dengan aturan pemulihan |

Seluruh route tersebut terdaftar pada firmware produksi. HTTP 202 berarti diterima antrean, dan kontrol biasa tetap menunggu hasil ID eksekusinya. Mode yang tidak kompatibel diperiksa kembali oleh pemilik UART. Perubahan jaringan ditolak ketika scanner atau maintenance aktif, sedangkan Hentikan/Lanjutkan memakai mailbox prioritas. Polling panel detail berhenti saat ditutup dan payload rusak tidak diubah menjadi pengukuran valid.

## Memeriksa instalasi terpadu

Setelah upload 2.2.0, periksa versi footer, lakukan pembacaan manual 0x0058 dari konsol web dan cocokkan register dengan display meter. Uji Hentikan saat timeout, Lanjutkan ke produksi, scan sempit, unduh trace, process monitor, CSV serta pergantian mode Wi-Fi. Pada mode client, putuskan router secara sengaja untuk memastikan AP kembali sementara akuisisi tetap berjalan. Ini melengkapi pengujian software pada [VALIDATION](VALIDATION.md), yang memakai API simulasi untuk tes browser.

Endpoint komunikasi mengirim maksimal delapan event per halaman dengan penanda backlog. Polling browser menargetkan 500 ms dan mencoba kembali permintaan gagal setelah 1,2 detik, sambil mempertahankan riwayat serta membedakan timeout, payload invalid dan endpoint tidak terjangkau. Kegagalan HTTP event tidak membuktikan ada atau tidaknya slave Modbus.

Kontrol Screensaver OLED pada Perangkat & arsip memilih waktu idle 10–3600 detik atau mematikan animasi, dengan default 60 detik. Tekanan fisik pertama membangunkan dashboard tanpa menjalankan aksi tombolnya. [OLED](OLED.md#animasi-idle-dan-cara-bangun) menjelaskan playback dan perilaku bangun.

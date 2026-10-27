# Menggunakan dashboard lokal

[English](../WEB.md) · **Bahasa Indonesia**

Firmware 2.1.0 menyajikan seluruh aplikasi dari ESP32, termasuk pengukuran, scanner koneksi, konsol diagnostik ModbusMaster, traffic mentah, metrik proses dan pengaturan Wi-Fi. INO mandiri tetap menjadi alat opsional untuk diagnostik USB, sehingga fitur terbaru tidak mengharuskan flash sketch lain atau memakai Serial Monitor. Pergantian Wi-Fi tidak mengubah setting meter.

## Membuka aplikasi dan memilih Wi-Fi

Pada instalasi baru, hubungkan HP ke **Midea Engineering AP** memakai **betahdijre12345**, kemudian buka **http://192.168.4.1**. Password AP mengikuti [board.h](../../firmware/include/board.h). Bahasa awal adalah English dan dapat diganti ke Indonesia melalui pemilih bahasa. Jika ESP32 menjadi client, gunakan IP DHCP dari router yang ditampilkan pada halaman jaringan OLED maupun panel Wi-Fi.

Buka **Perangkat & arsip → Koneksi Wi-Fi**, pilih mode, isi SSID jaringan 2,4 GHz beserta password, kemudian pilih **Simpan pengaturan Wi-Fi**. Password menerima 8–63 karakter ASCII tercetak atau WPA key heksadesimal 64 digit. Jaringan terbuka harus dipilih melalui checkbox khusus. Kolom password kosong mempertahankan key lama hanya untuk SSID yang sama, dan password tersimpan tidak pernah dikirim kembali lewat API status atau dimasukkan ke storage browser. Form ini tidak menyediakan identitas atau sertifikat untuk jaringan enterprise.

| Mode | Jalur akses yang tersedia |
| --- | --- |
| Access point saja | Koneksi langsung ke hotspot Midea, dengan kredensial client tetap tersimpan untuk digunakan kembali |
| Client dengan pemulihan AP | AP selama percobaan koneksi, kemudian IP client setelah DHCP berhasil dan masa tenggang AP 15 detik berakhir |
| Access point + client | Kedua antarmuka menyajikan aplikasi serta API yang sama |

Perintah simpan masuk ke core 1 untuk disimpan dalam record NVS ber-CRC. Jika penyimpanan gagal, koneksi aktif dipertahankan. Setelah berhasil, core 0 menunggu delapan detik sebelum menerapkan mode agar browser sempat membaca hasil eksekusi. Percobaan client tidak menahan task akuisisi, dicoba kembali setiap 30 detik, dan AP tetap tersedia selama belum tersambung. AP muncul kembali jika client kehilangan koneksi. Proses ini tidak memformat storage, menghapus interval atau me-reboot perangkat.

![Mode Wi-Fi dan pemulihan](../assets/network-modes.svg)

AP dan client memakai satu radio, sehingga AP mengikuti kanal router dalam mode gabungan. Jika subnet LAN bertabrakan, firmware memilih alamat AP yang tidak tumpang tindih dari 192.168.4.1, 192.168.5.1, 10.42.0.1 atau 172.23.42.1. Baca alamat terbaru setelah perubahan jaringan. HP pada router harus diizinkan mengakses client LAN lain. Antarmuka HTTP lokal yang sama menjadi tersedia pada LAN tersebut, dengan token mutasi per boot dan tanpa akun/TLS tersendiri.

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
| Mode jaringan dan kredensial | `/api/network` | Persistensi core 1 lalu pesan satu slot ke state machine Wi-Fi core 0 |
| LED, mute dan jam RTC | `/api/led`, `/api/mute`, `/api/time` | Kontrol antrean dengan hasil eksekusi |
| Arsip dan CSV | `/api/history`, `/api/export` | Akses filesystem terbatas dengan aturan pemulihan |

Seluruh route tersebut terdaftar pada firmware produksi. HTTP 202 berarti diterima antrean, dan kontrol biasa tetap menunggu hasil ID eksekusinya. Mode yang tidak kompatibel diperiksa kembali oleh pemilik UART. Perubahan jaringan ditolak ketika scanner atau maintenance aktif, sedangkan Hentikan/Lanjutkan memakai mailbox prioritas. Polling panel detail berhenti saat ditutup dan payload rusak tidak diubah menjadi pengukuran valid.

## Memeriksa instalasi terpadu

Setelah upload 2.1.0, periksa versi footer, lakukan pembacaan manual 0x0058 dari konsol web dan cocokkan register dengan display meter. Uji Hentikan saat timeout, Lanjutkan ke produksi, scan sempit, unduh trace, process monitor, CSV serta pergantian mode Wi-Fi. Pada mode client, putuskan router secara sengaja untuk memastikan AP kembali sementara akuisisi tetap berjalan. Ini melengkapi pengujian software pada [VALIDATION](VALIDATION.md), yang memakai API simulasi untuk tes browser.

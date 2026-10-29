# Menggunakan dashboard lokal

[English](../WEB.md) · **Bahasa Indonesia**

Controller discovery bersama, registry flash bernama, ID sumber checkpoint serta codec setting yang dipisahkan dijelaskan pada [Pencarian meter](METER_DISCOVERY.md). Scanner tetap hanya memakai FC03 dan tidak menebak produsen dari nilai mentah.

Contoh khusus Fort di bawah tetap berlaku untuk Fort. Untuk Acrel, simpan model yang dipilih terlebih dahulu lalu ikuti [Profil meter](METER_PROFILES.md) untuk register, terminal, konversi dan alur diagnostiknya.

Firmware 2.6.0 menyajikan seluruh aplikasi dari ESP32, termasuk pengukuran, scanner koneksi, konsol diagnostik ModbusMaster, traffic mentah, metrik proses dan pengaturan Wi-Fi. INO mandiri tetap menjadi alat opsional untuk diagnostik USB, sehingga fitur terbaru tidak mengharuskan flash sketch lain atau memakai Serial Monitor. Pergantian Wi-Fi tidak mengubah setting meter.

## Mengambil angka meter terjadwal

Ringkasan dimulai dari energi langsung serta **Checkpoint kWh terjadwal**. Lima jadwal pagi adalah 06.00, 06.30, 07.00, 07.30 dan 08.00 WIB, sedangkan jadwal sore adalah 17.00, 17.30, 18.00, 18.30 dan 19.00 WIB. Angka kumulatif yang sudah tercatat tetap tersedia ketika petugas datang belakangan, tanpa bergantung pada router atau dashboard terbuka. Periksa tanggal, pilih shift lalu unduh CSV checkpoint. Rekaman checkpoint sebelumnya memilih hari lain yang tersedia. Tampilkan selisih merupakan pilihan tambahan dan tidak mengganti angka asli. [CHECKPOINTS](CHECKPOINTS.md) menjelaskan timing, pemulihan penyimpanan, retensi serta arti setiap kolom.

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
| Angka kumulatif terjadwal dan CSV checkpoint | `/api/checkpoints` | Salinan hari berjalan/pemulihan atau arsip CRC terbatas |
| Arsip dan CSV | `/api/history`, `/api/export` | Akses filesystem terbatas dengan aturan pemulihan |

Seluruh route tersebut terdaftar pada firmware produksi. HTTP 202 berarti diterima antrean, dan kontrol biasa tetap menunggu hasil ID eksekusinya. Mode yang tidak kompatibel diperiksa kembali oleh pemilik UART. Perubahan jaringan ditolak ketika scanner atau maintenance aktif, sedangkan Hentikan/Lanjutkan memakai mailbox prioritas. Polling panel detail berhenti saat ditutup dan payload rusak tidak diubah menjadi pengukuran valid.

## Memeriksa instalasi terpadu

Setelah upload 2.6.0, periksa versi footer, lakukan pembacaan manual 0x0058 dari konsol web dan cocokkan register dengan display meter. Uji Hentikan saat timeout, Lanjutkan ke produksi, scan sempit, unduh trace, process monitor, CSV serta pergantian mode Wi-Fi. Pada mode client, putuskan router secara sengaja untuk memastikan AP kembali sementara akuisisi tetap berjalan. Ini melengkapi pengujian software pada [VALIDATION](VALIDATION.md), yang memakai API simulasi untuk tes browser.

Endpoint komunikasi mengirim maksimal delapan event per halaman dengan penanda backlog. Polling browser menargetkan 500 ms dan mencoba kembali permintaan gagal setelah 1,2 detik, sambil mempertahankan riwayat serta membedakan timeout, payload invalid dan endpoint tidak terjangkau. Kegagalan HTTP event tidak membuktikan ada atau tidaknya slave Modbus.

Kontrol Screensaver OLED pada Perangkat & arsip memilih waktu idle 10–3600 detik atau mematikan animasi, dengan default 60 detik. Tekanan fisik pertama membangunkan dashboard tanpa menjalankan aksi tombolnya. [OLED](OLED.md#animasi-idle-dan-cara-bangun) menjelaskan playback dan perilaku bangun.

### Progres scan dan pemulihan

Saat scan diminta, firmware mengaktifkan antarmuka station lalu menunggu 300 ms sebelum memulai scan driver. Association client yang belum selesai ditunda sampai discovery berakhir, sedangkan koneksi client yang sudah aktif dipertahankan. Scanner mengizinkan tiga percobaan start/kegagalan dengan jeda retry 750 ms, batas radio 12 detik serta batas keseluruhan 20 detik. Radio memakai 100–200 ms per kanal dan kembali ke kanal utama selama 50 ms di antara kanal. Akuisisi tetap berjalan pada core 1.

Seluruh field `wifi_scan_config_t` diinisialisasi, termasuk home-channel dwell, dan hasil dibaca setelah callback scan-done Arduino selesai mengumpulkan daftar jaringan. Jalur ini menghindari timeout wrapper Arduino yang dihitung dwell × 20, yaitu hanya empat detik pada dwell 200 ms. Panel menampilkan generation, jumlah percobaan, alasan kegagalan serta kode driver mentah. `radio_start` berarti driver menolak mulai, `radio_failed` berarti completion melaporkan kegagalan, `radio_timeout` berarti deadline habis dan `radio_enable` berarti STA gagal diaktifkan. Scan selesai tanpa SSID ditampilkan sebagai hasil kosong.

Kegagalan HTTP tidak membuang pilihan jaringan atau otomatis memulai scan baru. Halaman membaca ulang status generation yang sama dan mempertahankan daftar terakhir. Setelah upload, hubungkan kembali ke IP ESP32 yang aktif lalu muat ulang halaman, kemudian pilih Client atau AP + client di Perangkat & arsip. Jika masih gagal, salin alasan scan dan kode driver yang terlihat pada panel web ke chat bantuan. Alternatifnya, buka **Arduino IDE Serial Monitor pada 115200 baud**, lalu salin baris **OUTPUT** yang diawali `[WiFi scan]` ke chat bantuan. Baris tersebut adalah hasil yang dibaca, bukan perintah untuk diketik.

### Membaca diagram komunikasi

Simbol MCU mewakili UART1, dua simbol driver berlawanan mewakili transceiver dan simbol display mewakili meter Fort. Jalur biru membawa request menuju meter, sedangkan jalur hijau menunjukkan RX yang teramati menuju ESP32. RX amber menandai frame yang tidak lolos validasi normal. Label pin memakai direction pin yang ikut dicatat pada event, termasuk pin percobaan diagnostik.

Empat milestone mempertahankan TX diantrekan, UART selesai, byte RX pertama dan hasil transaksi terbaru. Durasi dihitung dari TX queued memakai timestamp perangkat dengan pengurangan unsigned yang menangani rollover mikrodetik. Nilai DE serta /RE menunjukkan state software terakhir yang tercatat dan menjadi tidak diketahui saat endpoint event terputus. Nilai ini bukan pengukuran feedback elektris. Timeout mempertahankan RX kosong dan tidak mengisi milestone RX yang tidak terjadi.

Jalur animasi memakai React Bits WakeSlider yang diberikan sebagai visual non-interaktif. Setiap transaksi selesai yang tercatat bergerak keluar selama 1100 ms dengan easing quintic, sehingga kecepatannya naik bertahap lalu turun sampai berhenti di ujung. Paket dan gelombangnya kemudian memudar bersama selama 360 ms tanpa berpindah posisi. Setelah jeda 140 ms, byte RX yang tertangkap mengikuti gerak serta fade yang sama ke arah sebaliknya, dilanjutkan jeda 160 ms. Satu replay request/respons membutuhkan sekitar 3,2 detik. Jalur dasar tetap terlihat ketika gelombang aktif memudar, dan label paket baru dilepas setelah opacity mencapai nol. Timeout hanya memutar request keluar tanpa frame kembali. Durasi ini membuat replay terbaca dan terpisah dari timing transaksi asli di bawahnya, tanpa menyimpulkan waktu tempuh listrik dari animasi.

Playback hanya menyimpan satu transaksi aktif dan transaksi terbaru yang menunggu. Burst mengganti visual yang menunggu tanpa menumpuk antrean panjang, sedangkan raw event terbaru tetap tersedia pada log 32 entri. Pengelompokan membutuhkan sequence berurutan dengan ID, baud, parity, register, jumlah word, pin dan konteks diagnostik yang sama. Riwayat awal, riwayat reboot, sequence hilang atau respons tanpa request tidak membuat replay baru. Halaman tersembunyi, endpoint terputus dan preferensi reduced-motion membersihkan playback.

Saat diam, jalur berupa titik berukuran 2 px, sedangkan gerakan membentuk gelombang seperti sound wave dengan tinggi sampai 36 px. Gelombang melebar di sekitar puncaknya lalu kembali menuju garis titik ketika gerakan melambat. ResizeObserver menyesuaikan setiap jalur antara 64 dan 256 titik, sehingga titik tetap kecil di layar lebar serta terpisah di HP. Angka kWh utama memakai Counter yang diberikan dengan tiga desimal, pemisah sesuai bahasa dan transisi bergulir 650 ms saat pembacaan berubah. Nilai pertama tampil langsung, energi yang hilang tetap tidak tersedia dan reduced motion menampilkan nilai saat ini tanpa bergulir. Efek ini berjalan di browser tanpa menambah sampel atau mengubah perhitungan energi kumulatif.

![Replay komunikasi representatif dengan UART simulasi](../assets/ui-flow-desktop.png)

Jam memakai Counter yang diberikan untuk pasangan jam/menit/detik, dengan roll 300 ms tanpa putaran pembuka, bounce atau blur. Sumbernya tetap referensi UTC/RTC perangkat dan re-anchor presentasi lima menit. Browser tidak menambah NTP atau berulang kali menulis RTC. Label timer aksesibel menyampaikan waktu lengkap sementara numerik visual berulang disembunyikan dari pembaca bantu.

Menu memakai StarBorder yang diberikan di sekitar tombol native serta latar seleksi terpisah yang bergeser ke halaman aktif dalam sekitar 320 ms. Underline biru lama dan tap transport SVG dihapus. Reduced-motion menonaktifkan replay paket dan roll angka serta memakai tombol navigasi statis tanpa canvas dekoratif. WakeSlider telemetry bersifat inert dan disembunyikan dari penemuan kontrol bantu agar tidak dianggap sebagai pengaturan nilai meter.

![Jam bergulir representatif](../assets/ui-clock.png)

![Tombol menu representatif](../assets/ui-navigation.png)

Fetch API tetap berbagi satu slot sampai body respons diterima dengan deadline aktif delapan detik. Waktu antre tidak menghabiskan deadline dan Stop/Resume prioritas dapat membatalkan GET telemetry aktif. Animasi dijalankan browser. ESP32 menyajikan bundle offline yang dikompilasi tanpa mengeksekusi React, Motion atau rendering canvas.

[Lihat transisi kWh utama dengan pembacaan simulasi](../assets/ui-energy-demo.mp4).


## API discovery dan meter bernama

Adapter meter-lab menyediakan discovery serta registry flash yang sama dengan panel lokal melalui queue pemilik. Request baca tidak mengganggu pengukuran. Alur operator serta batasnya dijelaskan pada [Pencarian meter](METER_DISCOVERY.md).

| Route | Methods | Perilaku |
| --- | --- | --- |
| `/api/meter-lab` | GET, POST | GET mengembalikan bukti serta record bernama, dengan `download=1` untuk ekspor JSON. POST mengantrekan `start`, `save`, `select` atau `remove` beserta ID penyelesaian. |
| `/api/meter-lab/stop` | POST | Stop prioritas untuk run yang cocok melalui token atomik terpisah tanpa membatalkan POST yang sudah diterima. |

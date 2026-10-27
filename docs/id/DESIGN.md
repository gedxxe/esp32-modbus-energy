# Dasar desain firmware

[English](../DESIGN.md) · **Bahasa Indonesia**

Versi 2.2.2 memusatkan polling, discovery dan diagnostik mandiri pada library FortMeter yang sama. FortTransport menjalankan transaksi melalui ModbusMaster 2.0.1+fort.1, sehingga parser jeda software v1 tidak lagi menjadi jalur lain yang menafsirkan byte yang sama. Acuannya adalah sketch manual-direction yang dilaporkan berhasil oleh pengguna, dengan detail penerimaan rilis terintegrasi tetap dipisahkan pada [VALIDATION](VALIDATION.md).

## Sumber register Fort

Gambar PDF halaman 26 memperlihatkan request `01 03 00 58 00 02 45 D8` untuk arus fasa A, dengan contoh `0000 C317` yang berarti 4,9943 A. CRC respons yang tercetak pada tabel tersebut tidak sama dengan perhitungan ulang, sehingga software selalu menghitung CRC dan tidak memakai nilai cetak sebagai pengecualian validasi. Halaman 27–30 diperiksa sebagai tabel rasio serta nilai listrik.

| Alamat PDU | Word | Konversi | Halaman PDF |
| --- | ---: | --- | ---: |
| 0x002C–0x002E | 3 | PT numerator, denominator, CT, uint16 1–9999 | 27 |
| 0x0042–0x0043 | 2 | Tegangan fasa A, uint32 / 10000 V | 28 |
| 0x0058–0x0059 | 2 | Arus fasa A, uint32 / 10000 A | 26, 29 |
| 0x006A–0x006B | 2 | Daya aktif total, int32 / 10000 kW | 29 |
| 0x0080 | 1 | Frekuensi, uint16 / 100 Hz | 30 |
| 0x0082–0x0083 | 2 | Counter impor sekunder, uint32 / 1000 kWh | 30 |
| 0x008A–0x008B | 2 | Float impor primer, kandidat urutan word | 30 |

Kolom panjang energi sekunder tercetak 3 meski nilainya bertipe unsigned long dan memakai dua alamat. V2 membaca dua register sesuai alamat serta sketch yang berhasil, sedangkan diagnostik dapat menguji satu/dua/tiga word secara eksplisit. Integer digabung high word lebih dahulu. Float ditampilkan dalam kandidat ABCD dan CDAB tanpa memilih otomatis berdasarkan angka yang terlihat masuk akal. Hanya kalkulasi energi produksi yang menerapkan CT/PT, satu kali pada counter sekunder.

## UART dan kontrol RS485

![Urutan arah manual](../assets/rtu-timing.svg)

Instalasi terkonfirmasi memakai TX17→DI, RX18←RO dan GPIO16→DE+/RE. ModbusMaster membuang RX lama dari buffer, memanggil preTransmission untuk HIGH serta settling 100 µs, menulis request, menunggu flush UART, kemudian memanggil postTransmission untuk LOW dan settling 100 µs sebelum menerima. Respons lengkap selesai berdasarkan panjang yang diharapkan dalam timeout, tanpa menunggu gap t3.5 yang diamati task.

Default timeout dua detik dan jarak request 200 ms mengikuti acuan. Penantian memberi waktu ke scheduler, watchdog dan pemeriksaan Stop/Resume. Pre-drain serta capture dibatasi agar RX kontinu tidak membuat loop tanpa batas. Pilihan arah otomatis tetap tersedia secara eksplisit dan tidak mengubah GPIO arah saat transaksi.

Patch pada source ModbusMaster menambah timeout runtime, pembatalan kooperatif dan perbaikan inisialisasi, sementara konstruksi request serta loop respons tetap berasal dari library tersebut. Hash upstream/current dan diff disimpan pada provenance. Adapter memeriksa lagi ID, FC03, count, panjang tepat dan CRC. Pemeriksaan exception perlu dilakukan karena upstream 2.0.1 mengembalikan exception sebelum jalur CRC normal. Echo tidak dihapus untuk membuat respons baru, dan input berlebih tidak dapat menjadi energi.

## Penjadwalan dua core

![Kepemilikan task](../assets/architecture.svg)

Core 1 memiliki UART1, ModbusMaster, GPIO arah, Preferences dan mutasi tracker. Core 0 menangani HTTP, tombol/OLED serta RMT RGB. Perintah dan snapshot dikirim sebagai salinan melalui antrean RTOS. Buffer log mempertahankan 8 KiB terbaru berikut hitungan byte terbuang, sehingga layanan web tidak membaca UART atau NVS langsung.

Perintah biasa membawa ID, waktu antre serta revisi mode dan divalidasi lagi oleh core 1. Perubahan yang mengganggu akuisisi ditolak jika lebih tua dari sepuluh detik atau berasal dari revisi mode lain. LED/mute/capture dieksekusi pada batas transaksi agar penulisan NVS tidak disengaja terjadi saat mengoleksi respons. Satu putaran menangani maksimal enam kontrol dan enam belas input diagnostik nonaktif.

Stop/Resume memakai mailbox dan generasi pembatalan tersendiri. Hanya core 1 yang mengeluarkan input, tanpa reset antrean lintas core. Input sesudah Resume masuk generasi berikutnya. Pemeriksaan stop juga berjalan saat byte terus datang. Mutex I2C dan filesystem memiliki waktu tunggu terbatas, serta filesystem dilepas sebelum mengirim CSV melalui jaringan. Respons HTTP memakai send nonblocking dalam potongan 1 KiB dengan budget lima detik. Parser request upstream tetap perlu pengujian beban fisik.

## Alur akuisisi dan kualitas interval

Ketika kontak gagal, firmware hanya mencoba handshake manual lalu menunggu tiga detik sebelum mengulang, sehingga tidak langsung menyapu seluruh profil. Kontak yang berhasil membuka polling bertarget dua detik, pembacaan rasio dengan retry/backoff dan nilai pembanding sekitar setiap sepuluh detik. Timeout dapat memperpanjang periode aktual karena target tersebut bersifat kooperatif.

Counter sekunder tetap dibaca ketika rasio invalid, tetapi counter utama dan tracker tidak menerima pengganti tanpa skala. Energi produksi dihitung dengan `uint32 × 0,001 × PT_num / PT_den × CT` dalam double. UTC disimpan pada DS3231, sedangkan WIB diterapkan untuk tampilan, hari serta sembilan interval. Reboot, RTC invalid, counter turun, perubahan rasio dan gap lebih dari 15 detik memutus kontinuitas. Hanya sampel sah yang boleh diinterpolasi dan interval lengkap tetap tersimpan.

`clock_guard.h` melindungi hari rekaman terlepas dari kesehatan RTC. Timestamp HP antre dikompensasi dalam detik penuh, dibatasi umur sepuluh detik dan diperiksa lagi sebelum ditulis. Rollover otomatis menolak tanggal sebelum hari yang dipertahankan. Rotasi OLED diterapkan sebelum gambar boot dan tidak memengaruhi perhitungan waktu.

## Discovery dan penyimpanan

Enumerator menguji ID/format tersimpan lebih dahulu tanpa duplikasi, dengan pilihan current atau 18 kombinasi. Probe default 0x0058/dua word diulang setelah kontak sah. Data atau exception ber-CRC sah pada transaksi kedua diperlukan sebelum menjadi hasil. Exception membuktikan komunikasi, bukan ketersediaan register ataupun kebenaran skala. Direction mengikuti setting koneksi dan tidak ada broadcast atau write register meter.

Record koneksi v2 memakai magic baru dengan layout tetap 12 byte. Byte padding sebelumnya menampung direction serta reserved. Record v1 ber-CRC sah mempertahankan ID/baud/parity dan dimigrasikan ke default manual yang telah dikonfirmasi, tanpa mengubah record hari atau LED. Kegagalan menyimpan migrasi dilaporkan sebagai masalah storage.

Checkpoint hari memiliki magic serta CRC. Penggantian CSV memakai file sementara dan backup agar kegagalan tidak sengaja menghapus semua salinan yang dapat dipulihkan. LittleFS tidak pernah melakukan format otomatis. Konfirmasi fisik hanya tersedia setelah mount gagal. Arsip dibatasi pada jendela 31 hari dan salinan CSV dibaca terbatas sebelum lock dilepas untuk transfer jaringan.

## Browser dan diagnostik mandiri

HTTP 202 berarti perintah diterima antrean. Browser menunggu hasil ID yang tepat dari ring delapan completion, sementara Stop/Resume memiliki jalur prioritas. Skema payload melindungi tabel arsip/proses, trace dan event agar respons rusak tidak menjatuhkan halaman. Animasi hanya mereplay timestamp event dengan delay polling/jaringan, bukan mengukur waveform A/B. Persentase core mengambil sampel non-idle pada tick, sedangkan stack high-water merupakan minimum historis.

INO mandiri mengikutsertakan FortDiagnostics dari library yang sama dengan firmware utama. File `diagnostic_engine.h` dan `build-diagnostic.py` dipertahankan sebagai jalur kompatibilitas/provenance yang tidak menduplikasi parser. Arduino IDE memakai ZIP FortMeter yang disertakan agar hook serta lisensinya cocok. Nilai diagnostik hanya RAM dan tidak menjadi data interval produksi. Langkah operator tersedia pada [DIAGNOSTICS](DIAGNOSTICS.md).

## State Wi-Fi dan persistensi

Driver Wi-Fi berjalan di core 0, sedangkan core 1 memvalidasi dan menyimpan record kredensial v2 108 byte ber-versi pada key Preferences `wifi`. Formatnya mengodekan mode, 33 byte SSID, 65 byte key, empat byte pilihan IPv4 AP dan CRC16 secara eksplisit agar tidak bergantung padding struct. CRC mendeteksi kerusakan record dan tidak mengenkripsi kredensial. WiFi.persistent(false) mencegah driver membuat konfigurasi persisten kedua di luar kepemilikan core 1.

Perintah jaringan harus lolos guard mode/revisi/umur dan tersimpan sebelum dikirim melalui antrean satu slot ke core 0. Penundaan delapan detik memberi browser waktu membaca hasil HTTP. Kegagalan simpan mempertahankan koneksi, sedangkan record boot rusak menyisakan AP default. Mode AP mempertahankan kredensial client dan endpoint GET tidak mengembalikan password tersimpan.

NetworkPolicy pada network.h menghitung waktu dengan aman terhadap rollover tanpa memanggil driver. Core 0 memeriksa link setiap 250 ms, menjaga AP selama percobaan serta retry, lalu memberi tenggang 15 detik sesudah DHCP berhasil sebelum menonaktifkan AP dalam mode client. Koneksi yang hilang mengembalikan AP dan percobaan diulang setiap 30 detik. Mode AP + client memakai radio bersama, dengan pemeriksaan prefix subnet LAN/AP untuk memilih alamat yang tidak bertabrakan. Timing WLAN, DHCP dan isolasi router tetap memerlukan penerimaan perangkat fisik.

Endpoint diagnostik kini membawa setting sesi aktual serta counter bukti, dan mengikuti koneksi/status produksi ketika maintenance tidak aktif. Kontrol browser terstruktur menyusun perintah yang sama dengan konsol INO. Perbandingan capture juga mewajibkan direction, pin arah dan timing settle/gap sama agar perubahan transport tidak dibaca sebagai perubahan A/B semata. Peta route ke pemilik dan langkah operator tersedia pada [WEB](WEB.md).

## Perubahan jaringan dan jam pada 2.2

Setting jaringan memakai record v2 eksplisit 108 byte dengan empat byte alamat AP pilihan sebelum CRC. Record v1 berukuran 104 byte tetap dibaca dengan default 192.168.4.1. Serialisasi tidak memakai padding struct. AP tetap /24, mendahulukan alamat pilihan dan menghindari tumpang tindih subnet client.

Scan SSID berjalan asinkron pada driver core 0 dengan cache 16 baris yang dipakai web serta OLED. Kredensial persisten tetap milik core 1, dan kedua antarmuka menghasilkan SET_NETWORK yang sama. State machine OledNetworkMenu tidak menyentuh GPIO atau NVS, sehingga perpindahan mode, pemilih jaringan, input ASCII tersamarkan dan tools IP dapat diuji pada host. Repeat tombol dimulai setelah 450 ms dengan langkah 80 ms dan event pelepasan diabaikan setelah repeat agar tidak menambah karakter lagi.

API status membawa UTC perangkat, umur sampel monotonic dan revisi jam. Browser menghitung detik tampilan tanpa menulis RTC dan mempertahankan acuan selama lima menit kecuali sesi/perangkat/validitas waktu berubah. OLED juga memakai umur sampel untuk tampilan, sedangkan tracker hanya menerima pembacaan RTC tervalidasi. Halaman event UART dibatasi delapan entri dengan flag more agar catch-up serta gap sequence tetap terlihat.

## Kepemilikan render idle

Preferensi layar memakai record CRC delapan byte tersendiri dan melalui core 1 seperti setting LED. Core 0 mengamati input fisik dan menahan seluruh tekanan bangun sampai guard pelepasan selesai sebelum event debounce bisa menjalankan kontrol monitoring/menu. Policy IdleScreen dapat diuji untuk timeout, nonaktif, bangun dan rollover tanpa periferal.

Artwork disimpan sebagai paket RLE per frame dengan offset tervalidasi. build-oled.py mendekode seluruh 772 frame dan memeriksa gabungan SHA256 mentah sebelum memeriksa/menghasilkan C++, sehingga header referensi tersebar tidak diperlukan saat deployment. Playback memakai satu bitmap kerja 1 KiB, rotasi display dan mutex I2C terbatas yang sudah ada, lalu maju berdasarkan waktu pada target 42 ms. Beban HTTP/I2C dapat melewati frame, sedangkan akuisisi tetap independen dan tidak menerima sampel dari animasi.

## Discovery radio dan penjadwalan HTTP

Discovery Wi-Fi dan association client berbagi radio pada core 0. Scanner menunggu 300 ms setelah persiapan, menunda association yang belum selesai, mencoba ulang penolakan/kegagalan driver setiap 750 ms sampai tiga percobaan serta membatasi scan radio 12 detik dalam operasi total 20 detik. Konfigurasi IDF diinisialisasi nol dan menetapkan dwell aktif 100–200 ms serta dwell kanal utama 50 ms. Completion diterima setelah task event Arduino mengumpulkan hasil, melalui mailbox RTOS satu elemen berisi status dan timestamp. Core 0 menyalin maksimal 16 jaringan terlihat terkuat lalu membebaskan daftar Arduino. Deadline memakai waktu completion, bukan waktu loop HTTP yang mungkin terlambat, dan daftar sebelumnya bertahan saat scan gagal.

Adapter ini mengikuti internal scan Arduino ESP32 2.0.17 yang dipin, sehingga perlu ditinjau ulang saat framework dinaikkan. Inisialisasi field konfigurasi lengkap dan callback menggantikan ketergantungan pada timeout wrapper dwell × 20, dengan satu pemilik storage hasil Arduino. Keberhasilan radio, kelangsungan AP dan kegagalan resource driver tetap membutuhkan penerimaan fisik.

Scheduler browser menserialisasi fetch API sampai seluruh body respons diterima. Deadline delapan detik berlaku setelah request aktif dan tidak menghitung waktu antre. POST mendahului telemetry yang mengantre, sedangkan Stop/Resume diagnostik dapat membatalkan GET aktif tanpa membatalkan POST yang sudah diterima. Pada firmware, koneksi kosong yang menunggu lebih dari 250 ms mengalah saat socket lain tersedia. Parser upstream dan batas writer lima detik tetap berlaku. Perubahan ini menjadwalkan request WebServer satu client tanpa memindahkan akuisisi dari core 1.

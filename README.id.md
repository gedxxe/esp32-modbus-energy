# Fort Modbus Energy Monitor

[English](README.md) · **Bahasa Indonesia**

![Firmware 2.4.0](docs/assets/badges/firmware.svg) ![ESP32-S3](docs/assets/badges/board.svg) ![Arduino](docs/assets/badges/arduino.svg) ![ModbusMaster](docs/assets/badges/modbusmaster.svg) ![SSD1306](docs/assets/badges/ssd1306.svg) ![RTClib](docs/assets/badges/rtclib.svg) ![Lisensi](docs/assets/badges/license.svg)

Project ini menjadikan ESP32-S3 N16R8 sebagai pencatat kWh lokal untuk power meter Fort. Tugas utamanya menyimpan angka kWh kumulatif pada sepuluh checkpoint harian, sehingga petugas dapat datang belakangan dan mengambil rekamannya walaupun lokasi tidak terjangkau router. Pencatatan berjalan di ESP32 dengan jam DS3231 tanpa bergantung pada HP, dashboard terbuka maupun internet. Pengukuran langsung dan pilihan selisih antar-checkpoint tersimpan tetap tersedia dari web lokal.

Jadwal utamanya adalah **06.00, 06.30, 07.00, 07.30, 08.00** serta **17.00, 17.30, 18.00, 18.30, 19.00 WIB**. Angka yang tercatat di sekitar 07.00 tetap menjadi rekaman 07.00 saat petugas terhubung pukul 09.20. Waktu request serta respons aktual ikut disimpan, sedangkan checkpoint yang gagal tetap ditandai terlewat dan tidak diisi dengan angka belakangan. [Panduan checkpoint dan penyimpanan](docs/id/CHECKPOINTS.md) menjelaskan timing, pengambilan data, kolom CSV serta pemulihan.

![Jadwal checkpoint harian](docs/assets/checkpoint-schedule.svg)

## Dashboard

![Dashboard representatif dengan nilai simulasi](docs/assets/ui-desktop-id.png)

React 19.3.1 dan komponen React Bits yang disimpan lokal berjalan pada browser HP atau komputer. ESP32 hanya menyajikan aset terkompresi dari flash. Bahasa awal English dapat diganti menjadi Indonesia melalui pemilih bahasa. Screenshot repository memakai data simulasi dan bukan bukti pembacaan Fort terpasang.

## 1. Sambungan hardware

Instalasi yang dikonfirmasi sekarang memakai **transceiver manual**. GPIO16 terhubung ke DE dan /RE yang disatukan, sehingga HIGH mengaktifkan driver sekaligus mematikan receiver, lalu LOW melepas bus dan mengaktifkan penerimaan. DI/RO merupakan sisi TTL modul, sedangkan A/B merupakan pasangan diferensial menuju meter.

| Koneksi ESP32 | Tujuan | Fungsi |
| --- | --- | --- |
| GPIO17 / TX UART1 | DI RS485 | Byte request dari ESP32 |
| GPIO18 / RX UART1 | RO RS485 | Byte respons ke ESP32 |
| GPIO16 | DE dan /RE disatukan | Kontrol arah manual |
| 3V3 dan GND | VCC/GND transceiver kompatibel | Catu rangkaian tegangan rendah |
| GPIO8 / SDA | SDA OLED dan DS3231 | Data I2C bersama |
| GPIO9 / SCL | SCL OLED dan DS3231 | Clock I2C bersama |
| 3V3 dan GND | VCC/GND OLED dan RTC | Catu bersama |
| GPIO4, GPIO5, GPIO6 | PREV, NEXT, OK ke GND | Pull-up internal, ditekan berarti LOW |
| GPIO7 | Input driver buzzer opsional | Pola buzzer aktif |
| GPIO48 | LED RGB bawaan | Tanpa kabel tambahan |

Hubungkan **A modul ke terminal Fort 58/A** dan **B ke terminal 59/B**. Terminal 70/COM merupakan common input sakelar meter, bukan ground RS485. Port meter ini khusus untuk ESP32 sebagai satu-satunya master.

![Wiring lengkap instalasi](docs/assets/wiring-overview.svg)

Diagram menunjukkan net listrik, bukan urutan header fisik. Label 3V3/GND berulang menunjuk net yang sama, titik menandai sambungan, dan persilangan tanpa titik tidak tersambung. Driver buzzer dua kabel memakai NPN, resistor basis 1 kΩ serta pull-down 10 kΩ sebagai komponen tambahan. Urutan B/C/E fisik harus mengikuti datasheet transistor yang dipakai.

[Modul arah otomatis](docs/assets/rs485-auto.svg) tetap tersedia sebagai alternatif eksplisit. Pilih Automatic pada Meter setup dan biarkan GPIO16 tidak terhubung jika memakai modul tersebut. Default konfigurasi baru serta migrasi v1 mengikuti instalasi manual yang telah dikonfirmasi.

## 2. Build dan upload

Buka direktori **`firmware/`** pada PlatformIO karena `platformio.ini` berada di sana, lalu tutup Serial Monitor yang sedang memegang port upload. Jalankan perintah berikut pada terminal komputer yang sudah berada di `firmware/`:

```sh
pio run -e esp32s3
pio run -e esp32s3 -t upload
pio device monitor -e esp32s3
```

Build biasa memakai header web yang sudah disimpan dan tidak memerlukan Node. PlatformIO memasang dependency display/RTC/JSON yang dikunci serta menemukan library FortMeter lokal secara otomatis, termasuk source ModbusMaster yang sama dengan diagnostik mandiri. [FLASHING](docs/id/FLASHING.md) menjelaskan offset app/factory dan ZIP library untuk Arduino IDE.

| Komponen | Versi atau konfigurasi |
| --- | --- |
| PlatformIO Core | 6.1.16 |
| Espressif32 / Arduino ESP32 | 6.10.0 / 2.0.17 |
| Memori target | Flash 16 MB, OPI PSRAM, ESP32-S3 N16R8 |
| FortMeter / ModbusMaster | 2.4.0 / 2.0.1+fort.1, source lokal |
| Adafruit SSD1306 / GFX / BusIO | 2.5.13 / 1.11.11 / 1.17.0 |
| RTClib / ArduinoJson | 2.1.4 / 7.3.1 |
| React / React DOM / Motion | 19.3.1 / 19.3.1 / 14.7.1 |
| GSAP untuk referensi FadeContent | 3.15.0, tidak dimuat dalam bundle runtime saat ini |
| Node / esbuild / Playwright | 24.22.1 / 0.28.2 / 1.62.1 |

## 3. Menyalakan pertama kali

Hubungkan HP ke **Midea Engineering AP** dengan password **betahdijre12345**, lalu buka **http://192.168.4.1**. Pastikan footer menunjukkan Firmware 2.4.0, samakan slave ID/baud/parity dengan menu CONi Fort, dan pilih Manual direction. Firmware menguji kontak melalui FC03 `0x0058`, dua register, sebelum meminta profil pengukuran lainnya. Saat kontak gagal, pembacaan kembali ke retry terbatas setiap tiga detik.

Pada board baru, LittleFS mungkin belum memiliki filesystem yang dapat dipakai. Firmware tidak melakukan format otomatis: hanya sesudah mount gagal, tahan OK saat boot dan lanjutkan selama lima detik sesuai petunjuk OLED. Prosedur ini menghapus arsip CSV, sedangkan migrasi konfigurasi v1 tidak sengaja menghapus checkpoint hari atau interval lengkap yang sudah tersimpan.

### Mode AP dan client Wi-Fi

Buka **Perangkat & arsip → Koneksi Wi-Fi** untuk memilih AP, Client dengan pemulihan AP, atau AP + client dan memilih SSID dari scan otomatis 2,4 GHz. Setting disimpan sebelum koneksi berubah setelah delapan detik. Mode client mempertahankan AP selama mencoba terhubung dan 15 detik setelah DHCP berhasil, lalu memunculkannya kembali jika koneksi putus. Baca IP terbaru pada panel web atau OLED, khususnya jika subnet LAN bertabrakan dengan AP. [Panduan web terpadu](docs/id/WEB.md) menjelaskan penyimpanan password, pemulihan dan seluruh fitur yang tersedia.

![Mode koneksi Wi-Fi](docs/assets/network-modes.svg)

### Diagnostik tanpa mengganti firmware

Buka **Meter setup → Diagnostic console**, ketik `intermediate` di **Command input**, lalu pilih **Run command** untuk menjalankan contoh arus pada manual. Masukkan `diagnose` secara terpisah bila memerlukan observasi RX pasif dan plan register yang lebih luas. Output memuat TX/RX mentah, status ModbusMaster dan word hasil baca. Nilai uji tidak masuk rekaman interval. Panduan [DIAGNOSTICS](docs/id/DIAGNOSTICS.md) menjelaskan alur serta cara menyimpan OUTPUT.

## Scan Fort

Mulai dari format tersimpan dan ID yang diketahui atau rentang kecil. Probe awal `0x0058/2` memakai contoh manual. Kandidat dengan data atau exception ber-CRC sah diuji sekali lagi sebelum menjadi hasil. Apply result menyimpan setting pembaca, bukan register meter, serta belum membuktikan skala energi. Pencarian 247 ID dan 18 format mempunyai 4.446 kandidat dan dapat memerlukan sekitar tiga jam bila semuanya mencapai timeout dua detik.

### Posisi OLED dan LED RGB bawaan

`OLED_ROTATE_180` pada [board.h](firmware/include/board.h) memutar semua layar 180° sebelum frame pertama, termasuk boot dan pemulihan storage. LED RGB menggunakan GPIO48. Pola, warna, kecerahan dan periodenya dapat disimpan dari Perangkat & arsip. RMT asinkron pada core 0 mengirim data LED dan tidak menghasilkan waveform UART.

### Kontrol lokal

| Kontrol | Aksi |
| --- | --- |
| PREV / GPIO4 | Halaman OLED sebelumnya |
| NEXT / GPIO5 | Halaman berikutnya |
| OK / GPIO6, singkat | Lihat checkpoint tersimpan berikutnya atau buka opsi Network |
| OK ditahan 900 ms | Toggle mute alarm |
| OK ditahan saat boot ketika filesystem gagal | Konfirmasi format arsip lima detik |

Tombol memakai pull-up internal dan debounce stabil 35 ms, dengan satu event long-press per penahanan. Buzzer boot berbunyi sekitar 1,5 detik. Alarm setelah startup berupa pulsa 120 ms setiap sepuluh detik ketika waktu, storage atau pengukuran belum tersedia, serta dinonaktifkan selama scan atau maintenance yang disengaja.

## 4. Perhitungan dan kualitas interval

Counter sekunder `0x0082–0x0083` dibaca sebagai integer unsigned high-word-first dan dibagi 1000. Nilainya tetap tersedia saat validasi rasio gagal. Counter energi utama serta tracker interval menghitung `kWh_sekunder × (PT numerator / PT denominator) × CT`, dengan rasio pada `0x002C–0x002E`. CT/PT hanya diterapkan sekali dan kalkulasi kumulatif memakai `double`.

Float primer `0x008A–0x008B` merupakan tampilan diagnostik terpisah. Perintah `primary` mencetak dua kandidat urutan word untuk dibandingkan dengan halaman Fort yang sesuai, tanpa memilih angka yang terlihat masuk akal secara otomatis. Ketidakselarasan kolom panjang energi pada scan manual tetap dicatat di [DESIGN](docs/id/DESIGN.md).

| Checkpoint pagi, WIB | Checkpoint sore, WIB |
| --- | --- |
| 06.00 | 17.00 |
| 06.30 | 17.30 |
| 07.00 | 18.00 |
| 07.30 | 18.30 |
| 08.00 | 19.00 |

Tabel menyimpan angka kumulatif meter. Selisih opsional dihitung dari dua checkpoint berurutan dalam shift yang sama dengan pemeriksaan konfigurasi serta kontinuitas. Nilai checkpoint mentah tidak diinterpolasi. Enam interval pagi dan tiga interval sore versi sebelumnya tetap tersedia melalui **Jadwal interval sebelumnya dan analisis**, dengan API ekspor serta arsip lama dipertahankan. [CHECKPOINTS](docs/id/CHECKPOINTS.md) menjelaskan perbedaan serta aturan capture/persistensinya.

## 5. RTOS dan scheduling

![Kepemilikan task](docs/assets/architecture.svg)

Core 1 memiliki UART1, kontrol arah, transaksi ModbusMaster, mutasi tracker dan Preferences. Core 0 menangani HTTP, OLED/tombol dan RGB. Kedua core bertukar perintah serta salinan snapshot melalui antrean RTOS, sementara I2C dan filesystem memakai mutex dengan waktu tunggu terbatas. Akuisisi menargetkan dua detik, pembacaan pembanding sekitar sepuluh detik, dan kontak gagal dicoba lagi setelah tiga detik. Periode ini memberi waktu ke scheduler dan bukan jaminan hard real-time.

Monitor proses mengambil sampel aktivitas non-idle pada tick FreeRTOS serta menampilkan heap/PSRAM, state task dan stack high-water. Persentase tersebut membantu pemeriksaan headroom, tetapi bukan profiler waktu instruksi. [Desain teknis](docs/id/DESIGN.md) menjelaskan pembatalan, revisi mode, urutan lock dan persistensi secara lebih rinci.

## 6. UART dan kontrol RS485

![Urutan request dan direction](docs/assets/rtu-timing.svg)

Transaksi membuang RX lama yang sudah berada di buffer, lalu preTransmission mengaktifkan DIR16 dan menunggu 100 µs. UART hardware mengirim request, `flush()` menunggu transmisi selesai, dan postTransmission melepas DIR16 serta menunggu 100 µs sebelum penerimaan. ModbusMaster menyelesaikan respons berdasarkan panjang yang diharapkan dalam timeout, tanpa menunggu detektor gap software lama. Jeda antarreques default 200 ms mengikuti sketch acuan yang berhasil.

Adapter merekam byte dan memeriksa ID, FC03, count yang diminta, panjang tepat serta CRC, termasuk CRC exception. Echo persis, byte tambahan dan input berlebih tidak menjadi data energi. UART dan I2C menggunakan peripheral hardware. Perubahan GPIO arah serta perhitungan CRC tidak melakukan serial bit banging. Source bersama dan patch ModbusMaster tersedia pada [library FortMeter](firmware/lib/FortMeter/src/FortTransport.h).

## 7. Build dashboard React

Sesudah mengubah `web/`, `package.json` atau lockfile, gunakan Node 24.22.1 dan jalankan **dari root repository**, yaitu folder yang memuat `package.json`:

```sh
npm ci
npm run build:web
npm run check:web
```

Simpan `firmware/include/web_ui.h` dan `web/bundle.json` bersama perubahan source sebelum compile firmware. Guard Python menolak input yang belum sinkron agar firmware tidak membawa dashboard lama. Payload gzip harus di bawah 160 KiB dan dikirim dalam potongan terbatas. Tidak diperlukan CDN, font eksternal atau upload filesystem web terpisah.

## 8. Pengujian dan pemeliharaan

`tools/test.ps1` menguji logika native, INO sebenarnya dan source ModbusMaster sebenarnya, sedangkan `python tools/check_docs.py` memeriksa link, vektor, pin, versi dan provenance. Tes browser merender byte gzip hasil build dengan API simulasi, sementara PlatformIO mengompilasi target fisik `esp32s3` dan sketch mandiri. [VALIDATION](docs/id/VALIDATION.md) mencatat hasil aktual beserta uji fisik yang belum dilakukan.

Kode asli proyek memakai **GPL-3.0-only**. Ketentuan dependency, patch ModbusMaster serta kepemilikan manual Fort dijelaskan dalam [NOTICE](NOTICE). React Bits dan dependency lain mempertahankan lisensinya. Baca [AGENTS](AGENTS.md), [CONTRIBUTING](CONTRIBUTING.id.md) dan [CHANGELOG](CHANGELOG.id.md) sebelum mengubah kepemilikan peripheral, transport atau profil register.

Halaman Network OLED juga menyediakan pilihan mode Wi-Fi, scan SSID, editor password tersamarkan dan IP AP khusus. [Panduan OLED](docs/id/OLED.md) menjelaskan tiga tombol beserta pengulangan cepat dan tools long press. Jam web berjalan di antara polling dari acuan waktu perangkat yang diperbarui lima menit sekali, sementara penyetelan RTC tetap eksplisit.

Animasi wajah 772 frame menjadi screensaver idle dengan waktu 10–3600 detik atau opsi nonaktif dari web. Default-nya 60 detik. Tekanan fisik pertama membangunkan dashboard dan tidak menjalankan aksi tombol lain.

Saat boot, logo Midea yang diberikan ditampilkan selama tiga detik sebelum loading bar dan bunyi startup yang sudah ada. `OLED_BOOT_SPLASH_MS` pada [board.h](firmware/include/board.h) mengatur tahap pertama ini, dan `OLED_ROTATE_180` diterapkan sebelum frame pertama. [Panduan OLED](docs/id/OLED.md#frame-pertama-saat-boot) memuat preview piksel serta timing startup.

Panel komunikasi memakai React Bits WakeSlider untuk gerak TX/RX bergantian yang terbaca, Counter untuk jam perangkat serta angka kWh utama, dan StarBorder untuk tombol menu. Jalur TX/RX memakai titik kecil 2 px yang kerapatannya mengikuti layar, sedangkan perubahan digit kWh bergulir selama 650 ms dengan tiga desimal tetap. Replay visual memakai sekitar 3,2 detik untuk satu pasangan request/respons, dengan percepatan bertahap, perlambatan saat tiba dan fade 360 ms pada paket serta gelombangnya. Timestamp UART asli tetap ditampilkan di bawahnya. [Lihat rekaman flow simulasi](docs/assets/ui-flow-demo.mp4) atau baca [panduan web](docs/id/WEB.md#membaca-diagram-komunikasi) untuk penjelasan operasi.

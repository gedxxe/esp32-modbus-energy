# Dasar desain firmware

## Sumber register Fort

Sumber utama adalah PDF `Fort Power Meter_User Manual_compressed.pdf` di root project. PDF berupa scan; tabel dibaca dari gambar halaman.

| Lokasi PDF / nomor halaman tercetak | Informasi |
| --- | --- |
| Halaman PDF 18 / tercetak 16, §7.1 | RTU half-duplex, 1200–38400 baud, default 9600, N81/E81/O81, default N81 |
| Halaman PDF 26 / tercetak 24, Tabel 13–14 | Contoh fungsi 03 mulai 0x0058 sebanyak dua register; urutan integer high word/high byte lebih dahulu |
| Halaman PDF 27 / tercetak 25, Tabel 17 | PT numerator 0x002C, PT denominator 0x002D, CT multiplier 0x002E |
| Halaman PDF 29 / tercetak 27, Tabel 17 | Total active power 0x006A–0x006B, signed long, 0.0001 kW |
| Halaman PDF 30 / tercetak 28, Tabel 17 | Positive active energy sekunder 0x0082–0x0083, unsigned long, 0.001 kWh; primer float 0x008A–0x008B |

Alamat di kode adalah alamat PDU **zero-based hexadecimal persis seperti tabel**, tanpa menambah angka 40001. Firmware hanya mengirim **fungsi 03**, tidak menulis register meter. Pengaturan web hanya mengubah UART/slave pembaca ESP32.

Rasio CT/PT dibaca otomatis sebelum energi, dan diperiksa lagi tiap 60 detik. Bila pembacaan rasio terus gagal hingga dua menit, konversi dihentikan. Perubahan rasio memutus kesinambungan interval. Pastikan setting rasio di meter memang sesuai CT/PT terpasang; pembaca tidak dapat memperbaiki setting instalasi yang salah.

Topologi yang dikonfirmasi: port Fort belum terhubung ke perangkat lain, ESP32 menjadi satu-satunya master, dan modul RS485 pengguna kompatibel dengan TTL ESP32 3,3 V.

Counter integer sekunder dipilih untuk menghindari pengurangan dua angka float32 primer besar yang dapat kehilangan resolusi. Konversi memakai `double` dan integer 32 bit big-endian. Resolusi akhir yang dilaporkan bergantung pada `0.001 × CT × PT`; enam angka desimal CSV tidak menjamin akurasi enam desimal.

### Kejanggalan manual yang tidak disalin ke protokol

Contoh query `01 03 00 58 00 02 45 D8` memiliki CRC yang benar. Contoh respons Tabel 14 tercetak `01 03 04 00 00 C3 17 72 E5`, tetapi perhitungan Modbus untuk tujuh byte awal menghasilkan **EA CD** (low byte dahulu), bukan 72 E5. Tes otomatis memverifikasi bahwa respons tercetak ditolak dan versi dengan CRC EA CD diterima.

Baris energi sekunder mencantumkan panjang “3”, tetapi memberikan tipe `ulong` dan dua alamat register; firmware meminta dua register / empat byte. Tabel juga memiliki salah label fase dan baud rate pada beberapa baris. Karena itu validasi pada meter aktual wajib: jangan mengganti CRC atau menggeser register untuk membuat data rusak lolos.

## Penjadwalan dua core

```text
Core 0: task loop Arduino (priority 1)
  WebServer + HTTP API
  tombol/debounce + OLED sekitar 3 Hz + buzzer
           | command queue (6 item)
           v
Core 1: task fort-acquisition (priority 2)
  UART1/RS485 + CRC + retry
  RTC + konversi CT/PT + tracker interval
  checkpoint NVS + arsip LittleFS
           | snapshot queue (1 item, overwrite)
           v
Core 0 membaca salinan snapshot konsisten
```

Wi-Fi/system tasks tetap dikelola ESP-IDF. Mutex I2C menjaga transaksi RTC dan OLED tidak saling menyisip; mutex filesystem menjaga ekspor/arsip. UART dan Preferences dimiliki satu task. Web menyalin CSV kecil sebelum mengirimnya, sehingga koneksi HP lambat tidak menahan mutex filesystem.

Polling energi ditargetkan setiap 2 detik; retry, pembacaan rasio, atau gangguan bus dapat memperpanjangnya. Web refresh 2,5 detik. Task akuisisi memakai delay/yield ketika menunggu respons; core tidak dipaksa 100% sibuk. Memenuhi deadline pembacaan lebih berguna daripada memaksimalkan utilisasi CPU. PSRAM diaktifkan, tetapi jalur kritis dan queue kecil cukup memakai RAM internal.

## Validasi Modbus

- CRC16 init 0xFFFF, polinomial reversed 0xA001, low byte CRC dikirim pertama.
- Satu request aktif, direction DE-/RE dijaga sampai `UART.flush()` selesai transmisi.
- Buffer tetap 16 byte untuk respons maksimum 11 byte yang dipakai firmware; UART RX buffer 128 byte.
- Tunggu silent gap minimal 3,5 karakter (atau 1,75 ms di atas 19200 baud), timeout 800 ms/request, tiga percobaan dengan jeda bertingkat.
- Tolak CRC salah, panjang/ID/fungsi/byte count salah, respons terlalu panjang, dan exception 0x83.
- Timestamp per byte UART tidak tersedia melalui polling Arduino ini; firmware tidak mengklaim memvalidasi t1.5 setiap byte secara presisi. CRC dan struktur frame menjadi pemeriksaan integritas utama. Modul harus menonaktifkan receiver/local echo ketika TX.
- Penurunan counter, termasuk kemungkinan rollover, tidak direkonstruksi otomatis: interval terkait parsial agar reset meter tidak dianggap energi rollover yang sangat besar.
- Penolakan lonjakan memakai batas fisik awal 100.000 kW (100 MW), ditambah toleransi dua tick counter. Sesuaikan batas di `main.cpp` berdasarkan kapasitas nyata pabrik. Ini pemeriksaan kasar, bukan validasi metrologi.

## Waktu dan kualitas interval

RTC disimpan dalam UTC; tracker menerima UTC+7 untuk pembagian tanggal WIB. Seluruh batas memakai timestamp kalender, bukan menghitung 30 menit sejak boot. RTC dibaca dengan pemeriksaan ACK, jumlah byte, BCD, tanggal kalender, mode 24 jam, dan oscillator-stop flag. Data tidak dicatat ketika RTC gagal divalidasi.

Untuk batas di antara dua sampel kontinu, counter batas dihitung dengan interpolasi linier. Selisih waktu antarsampel maksimal 15 detik. Jika sampel tidak tersedia tepat batas, hasil memang estimasi. Beban yang berubah cepat di sekitar batas dapat memberi error alokasi antarinterval; jumlah energi antar dua counter tetap berasal dari meter.

Waktu RTC dibaca sebelum dan sesudah respons energi; jendela >2 detik / transaksi >2,5 detik tidak dipakai sebagai sampel interval. Perubahan RTC mundur atau loncat >2 detik dibanding jam monotonic memutus tracker. Mengirim sinkronisasi waktu dari web selalu memutus interval yang aktif, termasuk saat selisih jam kecil. Koreksi tanggal mundur ditolak ketika RTC saat ini valid, untuk menghindari menimpa arsip tanggal lalu.

Interval parsial tidak diisi dengan nol dan tidak dibagi rata secara otomatis. Checkpoint tidak dipakai untuk menginterpolasikan gap reboot. Interval yang sudah lengkap tidak diubah ketika waktu atau pengaturan komunikasi berubah.

## Penyimpanan dan pemulihan

Konfigurasi dan checkpoint tersimpan sebagai blob NVS dengan CRC aplikasi. NVS menyediakan mekanisme commit internal; CRC tambahan mendeteksi blob yang tidak sesuai schema/isi. Checkpoint setiap pergantian status interval, perubahan setting/waktu, pergantian tanggal, dan paling lambat setiap 5 menit. Kerusakan checkpoint dapat menghilangkan hasil sejak checkpoint valid terakhir; perangkat ini belum memiliki cadangan eksternal.

Sejak 1.0.1, pemasangan arsip memakai helper yang sama dengan regression test fault injection. Jika backup tidak dapat direstorasi, proses berhenti sebelum menghapusnya. Kegagalan memasang file pengganti mempertahankan file backup. Ini menguji logika operasi, bukan perilaku flash terhadap seluruh kemungkinan mati daya.

CSV menyimpan sembilan baris dengan kualitas data dan counter batas. Arsip ditulis lewat file sementara dan salinan `.bak`; ekspor menggunakan backup jika file utama tidak ada. Penyimpanan penuh / kegagalan tulis ditampilkan sebagai storage error. Filesystem **tidak** diformat otomatis ketika mount gagal. Format fisik saat boot adalah tindakan pemulihan yang menghapus arsip.

Arsip dibatasi ke 31 tanggal terakhir relatif terhadap tanggal RTC dan tanggal lama dibuang setelah penulisan baru berhasil. Unduh data berkala jika perlu retensi lebih lama. Gangguan daya persis saat operasi flash tidak dapat dijamin hanya lewat simulasi; uji power-cut diperlukan. Untuk pencatatan pabrik yang harus tetap tersedia saat listrik mati, tambahkan UPS dan strategi backup data.

## HTTP lokal

`GET /api/status`, `/api/history`, `/api/export[?day=nomor_hari_epoch]`; `POST /api/config`, `/api/time`, `/api/mute`. POST memakai token acak per boot yang diberikan melalui status; input angka diperiksa rentang. Kredensial AP menggunakan WPA2 bawaan ESP32, maksimum empat klien. HTTP berjalan lokal tanpa TLS/login terpisah; siapa pun yang punya password AP dapat mengakses data dan pengaturan. Token membatasi permintaan lintas situs sederhana dan bukan pengganti autentikasi pengguna.

Tidak ada OTA, cloud, perubahan setting meter, captive DNS, atau sinkronisasi NTP pada versi ini. HP membuka IP secara manual. Waktu bersumber dari DS3231 dan sinkronisasi eksplisit dari HP.

Versi firmware berasal dari `version.h` dan dikirim lewat field `version` di status API. Badge dokumentasi dan metadata paket mengambil versi yang sama. Pembacaan rasio di-refresh setiap 60 detik dengan jeda minimal lima detik antarpencobaan; watchdog di-feed pada tiap percobaan Modbus dan pembacaan overflow dibatasi.

## Referensi protokol dan runtime

- [Modbus Serial Line Specification V1.02](https://www.modbus.org/file/secure/modbusoverserial.pdf): CRC dan framing RTU.
- [PlatformIO ESP32-S3 DevKitC-1](https://docs.platformio.org/en/latest/boards/espressif32/esp32-s3-devkitc-1.html): base board dan override konfigurasi.
- [Espressif FreeRTOS](https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/api-reference/system/freertos_idf.html): task affinity, queue, dan sinkronisasi lintas core.
- [DS3231 datasheet](https://www.analog.com/media/en/technical-documentation/data-sheets/DS3231.pdf): battery backup, waktu, dan oscillator-stop flag.

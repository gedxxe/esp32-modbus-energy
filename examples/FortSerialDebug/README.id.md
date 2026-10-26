# FortSerialDebug 1.3.1 — panduan diagnosis lapangan

[English](README.md) · **Bahasa Indonesia** · [Firmware utama](../../README.id.md)

Sketch Arduino mandiri dengan core Espressif, **tanpa library eksternal**. Saat dipasang, dashboard digantikan sementara: AP/OLED/RTC/pencatat interval tidak berjalan. Setting/capture hanya RAM. Semua request meter memakai fungsi baca 03.

## 1. Upload satu kali

1. Unduh ZIP repo dan ekstrak atau perbarui checkout. Buka file lokal [FortSerialDebug.ino](FortSerialDebug.ino) di Arduino IDE; biarkan [partitions.csv](partitions.csv) di sebelahnya. Halaman source GitHub bukan tempat mengetik perintah.
2. Pilih **ESP32S3 Dev Module**, COM board yang benar, **flash 16 MB**, **OPI PSRAM**, **Erase All Flash: Disabled**. USB native: **Hardware CDC and JTAG**, **USB CDC On Boot: Enabled**.
3. Klik Upload. Sesudah selesai, buka **Tools → Serial Monitor** pada COM yang sama. Pilih **115200 baud** dan **Newline**. `Both NL & CR` juga diterima; jangan pilih `No line ending`.
4. Banner harus menunjukkan **FortSerialDebug 1.3.1**. Jika belum terlihat, ketik `help` di input Serial Monitor dan Enter. Boot tidak menunggu USB.

Build lokal memakai Arduino ESP32 **2.0.17** melalui PlatformIO. GUI IDE, core lain dan Fort terpasang belum diuji penerimaan. Alternatif `pio run -d examples/FortSerialDebug` diketik di **terminal komputer** dari root repo, bukan Serial Monitor.

## 2. Pertahankan wiring terkonfirmasi

| Pin modul | Sambungan |
| --- | --- |
| VCC / GND TTL | ESP32 3V3 / GND |
| TXD modul | GPIO18, UART RX ESP32 |
| RXD modul | GPIO17, UART TX ESP32 |
| A+ / B− | Fort 58/A / Fort 59/B |

Modul pengguna: **empat pin TTL, arah otomatis**, GPIO16 tidak disambungkan. Gambar RO/DI/DE-/RE adalah alternatif modul arah manual. Fort 70/COM adalah common input saklar, bukan ground RS485. Pertahankan wiring terkonfirmasi selama pengumpulan bukti.

![Pemasangan modul arah otomatis](../../docs/assets/rs485-auto.svg)

## 3. Ketik perintah di INPUT Serial Monitor

**Perintah diketik ke ESP32 melalui Serial Monitor, bukan dikirim ke chat.** Ketik satu baris pada kolom input Serial Monitor, tekan **Enter / Send**, tunggu OUTPUT sebelum baris berikutnya. Jangan ikut mengetik backtick Markdown, kata `text`, prompt atau label log.

| Langkah | Ketik di INPUT Serial Monitor, lalu Enter | Tunggu OUTPUT |
| --- | --- | --- |
| 1 | `x` | `Stopped; queued commands cancelled.` |
| 2 | `status` | `[SETTINGS]`, `[PROFILE]`, plan dan counter UART |
| 3 | `diagnose` | Sampai `[REPORT_END]`; default tanpa balasan biasanya sekitar 10 detik |

Default ID1/9600/8N1 bukan bukti setting meter terpasang. Untuk mengubah setting, ketik misalnya `id 7` atau `baud 19200` pada input yang sama, Enter, periksa output. Tidak perlu edit source/upload ulang.

`diagnose` mendengarkan RX tanpa TX selama 3 detik, menguji probe terpilih, menjalankan daftar raw register terkonfigurasi, lalu mencoba kWh/rasio bila decode aktif. **Probe pertama diam tidak menghentikan daftar berikutnya.** Ini ID/format saat ini, bukan scan penuh otomatis. Maksimal request `1 + jumlah plan + 2` saat decode aktif; default diam mengirim 8 karena energi gagal sehingga query rasio dilewati. Waktu juga mencakup TX/gap/log. Ketik `x` untuk membatalkan.

### Sesudah selesai, salin OUTPUT ke chat untuk analisis

**Salin teks OUTPUT yang bergulir di Serial Monitor, lalu tempel ke chat ini.** Sertakan versi, hasil `status`, `[SERIAL_INPUT] diagnose`, semua TX/RX/CRC sesudahnya, dan `[REPORT_BEGIN]` sampai `[REPORT_END]`. Pertahankan byte HEX. Jangan tempel output kembali ke input Serial Monitor.

Contoh bentuk output saja, bukan bukti hardware:

```text
[SERIAL_INPUT] diagnose
[DIAGNOSE_BEGIN] ...
TX [8]: ...
RX [7]: ...
[REPORT_BEGIN] ...
[EVIDENCE] requests=... valid_data=... valid_exception=...
[NO_RX_UNRESOLVED] ...
[REPORT_END] ...
```

## 4. Bukti dan batas deteksi fisik

Modul empat pin ini tidak memberikan sinyal presence/power-good/fault. Modul dicabut/tanpa daya/rusak, RX terbuka, meter diam, ID/format salah dan query yang diabaikan dapat menghasilkan UART diam yang identik. Software tanpa byte tidak dapat memilih satu penyebab fisik itu.

| Ringkasan | Bukti | Langkah berikutnya |
| --- | --- | --- |
| `LOCAL_UART_ERROR` | Inisialisasi/penulisan UART lokal gagal | Tangani driver/resource lokal sebelum menilai modul |
| `NO_RX_UNRESOLVED` | Request dicoba, tidak ada byte | Opsional rxcheck/perbandingan dua keadaan; ukur suplai/cocokkan setting |
| `RX_UNVALIDATED` | Ada byte, tidak ada balasan diterima | Baca HEX/error UART; matriks serial terbatas |
| `ECHO_WITHOUT_REPLY` | Seluruh RX sama dengan request | Perbandingan dua keadaan; belum balasan Fort atau lokasi echo yang terbukti |
| `TRANSPORT_EXCEPTION_ONLY` | ID/FC83/CRC sesuai | Peer membalas, register belum dibaca;01fungsi,02alamat/count,03batas request,04kegagalan perangkat |
| `TRANSPORT_DATA_CONFIRMED` | ID/FC03/count/panjang/CRC sesuai | Audit raw map dan cocokkan display; bukan autentikasi model |
| `ENERGY_NOT_VERIFIED` | Tidak ada konversi diterima pada uji ini | Energi hilang tetap tidak diketahui, bukan nol |
| `PROFILE_DECODED_UNVERIFIED` | UInt32/scale/CTPT terkonfigurasi berhasil dihitung | Bandingkan display energi impor primer yang sesuai |
| `INCOMPLETE_TEST` | Dibatalkan | Ulangi sebelum membandingkan capture |

Ketik `report` untuk bukti uji aktif/scan/polling terakhir tanpa TX. `stats clear` menghapus bukti dan counter UART kumulatif. `status` menunjukkan setting saat ini yang mungkin sudah berubah sejak pengujian. Callback UART bisa terlambat; counter bukan pengukuran tegangan diferensial A/B.

Byte RX yang dibuang sebelum TX dihitung terpisah sebagai `pre_tx_bytes` dan dicetak prefix terbatas; bukan balasan request baru. Perbandingan juga ditolak dengan `COMPARE_LOCAL_UART_ERROR` bila ada kegagalan UART lokal.

### Opsional: observasi RX TTL

Ketik **`rxcheck` di Serial Monitor**, Enter. Polling/UART berhenti sebentar, RX diukur 32 kali dengan pull-up lemah dan 32 kali dengan pull-down lemah, lalu kembali input biasa/UART dibuka. Tidak mengirim Modbus atau menggerakkan RX sebagai output HIGH/LOW. `x` membatalkan dan tetap memulihkan UART.

| Log | Arti dan batas |
| --- | --- |
| `RX_WEAK_BIAS_FOLLOWING` | Sesuai kemungkinan impedansi tinggi/kabel terbuka/output tanpa daya atau disabled, tidak dapat memilih penyebab |
| `RX_HIGH_HELD` | HIGH bertahan melawan pull-down lemah; driven-idle/bias luar mungkin, belum bukti modul sehat |
| `RX_LOW_HELD` | LOW bertahan melawan pull-up lemah; periksa held-low/break/short/state driver, belum bukti rusak |
| `RX_VARIABLE` | Level berubah/bias bersaing; baca log atau instrumen |

Sampling TTL opsional ini bukan analyzer A/B, voltmeter atau tes continuity. Tidak membuktikan VCC; pulsa singkat dapat luput.

### Opsional: ubah A/B secara fisik di antara dua perintah

Jangan tempel kedua perintah sekaligus. Kabel TTL/suplai tetap.

1. A/B tersambung normal: ketik **`check connected` di Serial Monitor**, Enter, tunggu `[REPORT_END]`. Pesan awal `COMPARE_NEEDS_TWO` memang wajar.
2. Lepas **hanya A/B pada sisi modul tegangan rendah**, VCC/GND/TXD/RXD tetap. Jangan menyentuh wiring pengukuran/CT meter.
3. Ketik **`check isolated` di Serial Monitor**, Enter, tunggu `[REPORT_END]` dan `[COMPARE]`.
4. Sambungkan kembali A/B. Ketik `compare` untuk mencetak ulang tanpa TX. Salin OUTPUT lengkap kedua uji ke chat.

Keadaan adalah **label tindakan operator**, bukan deteksi firmware. Capture RAM harus memakai ID/format/pin/probe/plan/profil/timeout sama; perubahan/abort menolak perbandingan.

- `LOCAL_ACTIVITY_REPRODUCED`: byte juga muncul saat terisolasi; byte terisolasi bukan bukti balasan Fort. Lokalisasi aktivitas modul/TTL/ESP atau noise secara independen.
- `REPLY_REQUIRES_CONNECTED_PATH`: balasan sah hanya saat tersambung pada uji ini; validasi map energi terpisah.
- `CONNECTED_ACTIVITY_ONLY`: byte hanya tersambung, tidak ada yang diterima; setting/sinyal belum terpecahkan.
- `BOTH_SILENT_UNRESOLVED`: keduanya diam. Ukur suplai/TX/RX TTL atau bandingkan link sehat; UART saja tidak dapat mengisolasi modul dicabut/rusak. Multimeter memeriksa suplai/level statis, bukan timing bit/CRC.
- `ISOLATION_CONTRADICTED`: balasan sah pada uji berlabel terisolasi; periksa keadaan fisik atau jalur sinyal lain.

## Pilih pengujian berdasarkan hasil

| Kondisi | Paket perintah | Keputusan berikutnya |
| --- | --- | --- |
| Tidak ada byte RX | [01 RX pasif](cases/01-passive-rx.txt), lalu [02 satu request](cases/02-echo-or-no-reply.txt) | Bedakan diam tanpa TX dan diam setelah request; keduanya bukan bukti otomatis wiring salah. |
| Request kembali persis | [02 echo/tanpa respons](cases/02-echo-or-no-reply.txt) | TX=RX adalah bukti echo, belum balasan meter atau lokasi sumber echo. |
| CRC/parity/framing error | [03 matriks serial](cases/03-serial-matrix.txt) | Bandingkan ID yang sama pada baud/parity berbeda beserta counter UART dan hex mentah. |
| Exception ber-CRC sah | [04 exception/register](cases/04-register-exception.txt) | Komunikasi membalas; periksa kode exception serta alamat/count secara terpisah. |
| Komunikasi valid, profil energi gagal | [05 audit map mentah](cases/05-raw-map-audit.txt) | Matikan konversi, periksa raw word/dukungan register dan baca1/2/3word pada entri energi ambigu; tanpa kWh rekaan. |
| Raw energi/rasio masuk akal | [06 kWh terkonfigurasi](cases/06-configured-kwh.txt) | Bandingkan hasil dengan halaman energi meter yang sesuai; alamat/scale/order selalu dicetak. |
| Respons diduga terlambat | [07 perbandingan timeout](cases/07-timeout-comparison.txt) | Parameter lain tetap; timeout diperpanjang tanpa melewati pemeriksaan CRC. |
| Urutan word belum pasti | [08 perbandingan word order](cases/08-word-order-comparison.txt) | Bandingkan interpretasi normal/swap yang diberi label; tidak dipilih otomatis karena tampak masuk akal. |

Setiap TXT berisi **perintah INPUT Serial Monitor**. Buka satu TXT, ketik tiap baris non-komentar pada input Serial Monitor, Enter, tunggu selesai, lalu baris berikutnya. Jangan ketik path file. Salin OUTPUT ke chat. Baris `#` komentar. Paste multi-baris opsional hanya jika terminal mempertahankan newline. Console mengantre sampai **16 perintah lengkap** termasuk selama menunggu UART/listen, maksimal **95 karakter per baris**. Overflow antrean/baris dilaporkan, bukan dibuang diam-diam. **x** membatalkan operasi dan antrean. Perintah setelah scan menunggu scan selesai. Upload satu kali, pilih variasi uji lewat Serial.

## Referensi konfigurasi runtime

| Perintah | Arti dan nilai yang diterima |
| --- | --- |
| `help`, `status` | Bantuan atau seluruh setting, daftar scan, rencana register dan counter UART |
| `basic`, `listen 10` | RX pasif5detik atau durasi1–30detik; tanpa request Modbus |
| `intermediate`, `diagnose`, `deeper` | Probe saja; diagnosis berjenjang; rencana raw serta decode energi opsional |
| `id 7` | ID target1–247, tidak pernah broadcastID0 |
| `baud 19200`, `parity E` | Baud1200–1000000; parityN/E/O, data8bit, stop1bit. Manual Fort mencantumkan1200–38400; nilai di luar itu eksperimen, bukan klaim dukungan meter. |
| `pins 18 17` | Urutan RX GPIO lalu TX GPIO, harus berbeda. Pilihan terjaga:10–18,21,39–42,47. Pin USB, flash/PSRAM, boot/console dan I2C/tombol/buzzer/RGB pemasangan dikecualikan. Default tetap18/17. |
| `timeout 1200` | Batas tunggu respons50–5000ms setelah TXflush |
| `interval 3000` | Target polling100–60000ms; waktu baca/log bisa lebih lama |
| `probe 0080 1` | Set probe scan/intermediate: alamat PDU HEX,1–3word |
| `reg 0082 2` | Satu baca raw; alamat HEX boleh0x, count1–3, tanpa translasi40001 |
| `range 1 16` | Rentang ID scan inklusif |
| `scanmode current` | Scan ID memakai baud/parity yang sedang dipilih saja |
| `scanmode matrix` | Scan ID × daftar baud × daftar parity terkonfigurasi |
| `bauds 9600 19200` | Ganti daftar dengan1–6baud desimal unik; seluruh input valid baru diterapkan |
| `parities NE` | Ganti daftar dengan1–3modeN/E/O unik |
| `scan` | Probe kombinasi terpilih; berhenti pada data/exception sah, terpisah dari profil energi; tidak menulis setting meter |
| `plan clear`, `plan add 0080 1`, `plan show` | Sampai8query raw untuk deeper; alamatHEX,count1–3 |
| `energy 0082`, `ratios 002C` | Alamat energiUInt32 dua word serta PTnumerator/PTdenominator/CT tiga word |
| `scale 0.001`, `order normal`, `order swap` | Scale positif finite1e-12–1e6 dan urutan wordUInt32; interpretasi selalu dicetak |
| `decode off`, `decode on` | Matikan/aktifkan interpretasi kWh termasuk read/poll; reg/plan raw tetap tersedia |
| `read`, `poll` | Baca kWh/CTPT sekali atau toggle pembacaan berulang |
| `report`, `rxcheck` | Bukti uji terakhir tanpa TX; observasi input TTL dengan pull lemah, opsional |
| `check connected`, `check isolated`, `compare` | Uji dua keadaan berlabel operator; bandingkan snapshot RAM tanpa TX |
| `stats clear`, `x` | Hapus counter error lokal; batalkan operasi dan antrean |

Perubahan valid mencetak setting; nilai invalid mempertahankan setting sebelumnya. Semua hanya RAM, restart kembali default. Mengubah konfigurasi UART/profil menghentikan polling agar konfigurasi yang baru separuh diterapkan tidak memicu pembacaan otomatis. Fungsi03, integritas frame dan proteksi storage tetap menjadi batas tetap.

## Ukuran scan dan waktu

Slave yang diam ketika register probe tidak didukung tetap bisa terlewat. Ganti `probe` berdasarkan manual atau jalankan diagnosis beberapa alamat; tidak ada register universal.

Matriks default: ID1–247 × enam baud manual × N/E/O = **4.446 kandidat**. Untuk uji singkat satu ID/satu format: **ketik setiap baris berikut di INPUT Serial Monitor, lalu Enter**, tunggu selesai:

```text
x
range 1 1
scanmode current
probe 0034 1
scan
```

Untuk matriks terbatas gunakan paket03 atau ubah daftar. Current memiliki sejumlah ID kandidat; matrix = jumlahID × jumlahbaud × jumlahparity. Waktu tanpa balasan kira-kira kandidat × timeout ditambah TX/gap/log. Pada800ms, scan penuh sekitar satu jam atau lebih. `status` mencetak jumlah kandidat sebenarnya; x membatalkan saat menunggu.

## Arti log dan langkah lanjut

| Log | Arti | Langkah lanjut |
| --- | --- | --- |
| `COMM_FOUND` / `VALID_RESPONSE` | Respons dengan ID/fungsi/count/panjang/CRC sesuai | Lanjut raw map, lalu bandingkan decode terkonfigurasi dengan meter |
| `COMM_VALID_EXCEPTION` | ID sesuai,FC83,CRC sah | Kode01: cekfungsi/model;02: alamat/count/map;03: batasquantity/value;04: kegagalan sisi perangkat. Sketch tidak mereset/menghapus kondisi meter. |
| `EXACT_TX_ECHO` | Satu frameRX sama seluruh request8byte | Tetap ditolak; menunggu respons valid terpisah dalam batas waktu yang sama |
| `MISMATCH`, `SHORT_FRAME`, `WRONG_FRAME` | Rusak/terpotong atau bentuk respons tidak cocok | Simpanhex/counterUART, bandingkan format/timing sebelum menyimpulkan masalah map |
| `TIMEOUT` | Tidak ada byteRX dalam batas tunggu request | Bandingkan pasif/requesttunggal/matriks; tidak otomatis menyalahkan kabel |
| `NO_VALID_RESPONSE` | AdaRX tanpa data/exception yang diterima | Bedakan echo, korupsi dan bentuk salah dari log pendamping |
| `RX_OVERFLOW` | Hanya prefix64byte frame berlebih yang disimpan | Ditolak; periksa volume traffic dan counterFIFO/buffer |
| `DECODE_DISABLED` / `PROFILE_NOT_CONFIRMED` | Konversi mati atau energi/CTPT ditolak | Gunakanrawplan; respons protokol sah sebelumnya tetap bukti yang sah |
| `UART_EVENTS` | Counterkumulatif parity/framing/FIFO/buffer/break dan logicRX | stats clear memulai perbandingan. Counter0 bukan bukti tegangan diferensial A/B bersih. |

Alamat PDU adalah HEX literal: `0080` berarti 128 desimal, tanpa translasi40001/minus1. Satu word adalah 2 byte: `reg 0082 2` mengirim alamat `00 82` dan meminta 4 byte data. Panjang 2/4 pada tabel umumnya byte. Kasus05 membandingkan 1/2/3 word; 3 word mencakup alamat berikutnya dan tidak didecode sebagai energi 2 word. Bit float primer bukan UInt32 dan tidak boleh dikali CT/PT lagi jika sudah primer.

Default mengikuti manual yang diperiksa visual: energi 0x0082–83,PT/CT 0x002C–2E,scale 0,001,high word/byte first. Kolom panjang energi tercetak 3 meski dua alamat/ulong; ambiguitas tetap dicatat. Alamat/scale/order lain adalah eksperimen berlabel, bukan verifikasi map. Word float primer diaudit mentah, bukan ditebak menjadi kWh. [Audit manual](../../docs/DESIGN.md#manual-visual-audit-and-diagnostic-follow-up--26-october-2026).

## Batas dan verifikasi

Tanpa write register, broadcast, reset meter, NVS atau filesystem. GPIO16 tidak dipakai modul otomatis. RX 64 byte/frame; detail 4 frame/request dan 8 chunk pasif; traffic lain tetap dihitung/divalidasi. Counter callback atomik; console dicetak di loop. Timing bufferUART bukan logic analyzer, RX kosong bukan bukti A/B tidak bersinyal. Log Serial memengaruhi timing.

Tes menjalankan INO sebenarnya dengan UART/console tiruan: setting runtime, nilai invalid, berbagai hasil, antrean, stop dan konversi. PlatformIO mengompilasi sketch terpisah (`pio run -d examples/FortSerialDebug`). Timing/map hardware dan GUI IDE belum diuji. Setelah pengujian upload kembali firmware utama; pertahankan CSV partisi dan pengaturan erase sesuai panduannya.

### Koreksi transport pada 1.3.1

Guard sebelum TX mengamati jeda tenang kontinu dan membuang byte yang datang selama penantian. `[BUS_NOT_QUIET]` berarti permintaan tidak dikirim dalam budget guard; periksa aktivitas RX tercatat tanpa menyimpulkan modul hilang/rusak. Polling frame pendek lebih rapat di sekitar gap agar echo tidak tergabung dengan respons cepat yang sah. Inisialisasi UART gagal dicoba lagi. Ini observasi buffer software; task terlambat masih dapat melewatkan gap fisik dan RTU tidak memiliki transaction ID untuk memastikan asal setiap respons tertunda.

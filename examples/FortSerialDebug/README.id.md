# FortSerialDebug 1.2.0 — panduan diagnosis lapangan

[English](README.md) · **Bahasa Indonesia** · [Firmware utama](../../README.id.md)

Buka [FortSerialDebug.ino](FortSerialDebug.ino). Tidak perlu library eksternal, cukup core Espressif Arduino. Sketch ini aplikasi terpisah: dashboard/pencatatan interval berhenti selama sketch terpasang. Setting pembaca hanya RAM; meter hanya menerima fungsi baca03.

## Mulai dengan wiring pemasangan yang sama

1. Pertahankan TXD modul → GPIO18 ESP32 dan RXD modul ← GPIO17 yang sudah dikonfirmasi. VCC modul →3V3 ESP32, GND TTL →GND, A+ →Fort58/A, B− →Fort59/B. Modul arah otomatis: GPIO16 tidak disambungkan. Fort70/COM bukan groundRS485.
2. Buka INO di Arduino IDE, biarkan [partitions.csv](partitions.csv) di folder yang sama. Pilih ESP32S3 Dev Module / flash16MB / OPI PSRAM. USB native: Hardware CDC and JTAG, USB CDC On Boot Enabled. Erase All Flash Disabled.
3. Upload, Serial Monitor **115200 + Newline**. Kompilasi lokal memakai core Arduino ESP32 2.0.17 melalui PlatformIO; GUI IDE dan core lain belum diuji.
4. Kirim blok ini. ID/baud/parity contoh mengikuti default; sesuaikan lewat perintah dari setting menu meter bila diketahui. Tidak perlu mengedit source untuk mengganti parameter.

```text
x
pins 18 17
id 1
baud 9600
parity N
timeout 800
stats clear
diagnose
```

`diagnose` mendengarkan RX tanpa TX selama3detik, menguji komunikasi, lalu menjalankan rencana register bila respons/exception sah diterima. Gagal profil energi tidak membatalkan bukti respons Modbus sah. Tahap pasif hanya pada format UART terpilih; slave memang bisa diam tanpa request master.

## Pilih pengujian berdasarkan hasil

| Kondisi | Paket perintah | Keputusan berikutnya |
| --- | --- | --- |
| Tidak ada byte RX | [01 RX pasif](cases/01-passive-rx.txt), lalu [02 satu request](cases/02-echo-or-no-reply.txt) | Bedakan diam tanpa TX dan diam setelah request; keduanya bukan bukti otomatis wiring salah. |
| Request kembali persis | [02 echo/tanpa respons](cases/02-echo-or-no-reply.txt) | TX=RX adalah bukti echo, belum balasan meter atau lokasi sumber echo. |
| CRC/parity/framing error | [03 matriks serial](cases/03-serial-matrix.txt) | Bandingkan ID yang sama pada baud/parity berbeda beserta counter UART dan hex mentah. |
| Exception ber-CRC sah | [04 exception/register](cases/04-register-exception.txt) | Komunikasi membalas; periksa kode exception serta alamat/count secara terpisah. |
| Komunikasi valid, profil energi gagal | [05 audit map mentah](cases/05-raw-map-audit.txt) | Matikan konversi, periksa raw word/dukungan register; tanpa kWh rekaan. |
| Raw energi/rasio masuk akal | [06 kWh terkonfigurasi](cases/06-configured-kwh.txt) | Bandingkan hasil dengan halaman energi meter yang sesuai; alamat/scale/order selalu dicetak. |
| Respons diduga terlambat | [07 perbandingan timeout](cases/07-timeout-comparison.txt) | Parameter lain tetap; timeout diperpanjang tanpa melewati pemeriksaan CRC. |
| Urutan word belum pasti | [08 perbandingan word order](cases/08-word-order-comparison.txt) | Bandingkan interpretasi normal/swap yang diberi label; tidak dipilih otomatis karena tampak masuk akal. |

Setiap TXT berisi resep siap ditempel. Tempel satu resep per pengujian; baris `#` adalah komentar. Console mengantre sampai **16 perintah lengkap** termasuk selama menunggu UART/listen, maksimal **95 karakter per baris**. Overflow antrean/baris dilaporkan, bukan dibuang diam-diam. **x** membatalkan operasi dan antrean. Perintah setelah scan menunggu scan selesai. Upload satu kali, pilih variasi uji lewat Serial.

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
| `stats clear`, `x` | Hapus counter error lokal; batalkan operasi dan antrean |

Perubahan valid mencetak setting; nilai invalid mempertahankan setting sebelumnya. Semua hanya RAM, restart kembali default. Mengubah konfigurasi UART/profil menghentikan polling agar konfigurasi yang baru separuh diterapkan tidak memicu pembacaan otomatis. Fungsi03, integritas frame dan proteksi storage tetap menjadi batas tetap.

## Ukuran scan dan waktu

Matriks default: ID1–247 × enam baud manual × N/E/O = **4.446 kandidat**. Untuk uji singkat satu ID/satu format:

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

Default mengikuti manual yang diperiksa visual: energi0x0082–83,PT/CT0x002C–2E,scale0,001,highword/bytefirst. Kolom panjang energi tercetak3 meski dua alamat/ulong; ambiguitas tetap dicatat. Alamat/scale/order lain adalah eksperimen berlabel, bukan verifikasi map. Word float primer diaudit mentah, bukan ditebak menjadi kWh. [Auditmanual](../../docs/DESIGN.md#manual-visual-audit-and-diagnostic-follow-up--26-october-2026).

## Batas dan verifikasi

Tanpa write register, broadcast,resetmeter,NVS atau filesystem. GPIO16 tidak dipakai modul otomatis. RX64byte/frame; detail4frame/request dan8chunk pasif; traffic lain tetap dihitung/divalidasi. Countercallback atomik; console dicetak di loop. Timing bufferUART bukan logic analyzer, RX kosong bukan bukti A/B tidak bersinyal. LogSerial memengaruhi timing.

Tes menjalankan INO sebenarnya dengan UART/console tiruan: settingruntime,nilaiinvalid,berbagai hasil,antrean,stop,konversi. PlatformIO mengompilasi sketch terpisah (`pio run -d examples/FortSerialDebug`). Timing/maphardware dan GUIIDE belum diuji. Setelah pengujian upload kembali firmware utama; pertahankanCSVpartisi dan pengaturan erase sesuai panduannya.

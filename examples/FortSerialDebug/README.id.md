# Diagnostik serial Fort — ESP32-S3

[English](README.md) · **Bahasa Indonesia** · [Firmware utama](../../README.id.md)

Sketch [FortSerialDebug.ino](FortSerialDebug.ino) berdiri sendiri dan tidak memerlukan library tambahan. Cukup core **esp32 by Espressif Systems**. Saat sketch ini terpasang, aplikasi dashboard utama digantikan sementara; upload firmware utama lagi setelah pengujian.

## Sambungan

- GPIO17 = TX ESP32 → input TTL modul.
- GPIO18 = RX ESP32 ← output TTL modul.
- 3V3 ESP32 → VCC modul; GND ESP32 → GND TTL modul.
- A+ modul → Fort 58/A; B− → Fort 59/B. Fort 70/COM bukan ground RS485.
- Modul empat pin pada foto mengatur arah otomatis. GPIO16 tidak dipakai sketch ini. Wiring yang kamu laporkan adalah RXD modul → GPIO17, TXD modul → GPIO18; ini catatan pemasangan, bukan verifikasi independen arah label modul.

OLED, RTC, tombol dan buzzer tidak dibutuhkan. Tidak ada suplai yang diambil dari terminal komunikasi Fort.

## Compile di Arduino IDE

1. Install **esp32 by Espressif Systems** melalui Boards Manager. Kompilasi lokal diuji dengan core **2.0.17** melalui PlatformIO; GUI Arduino IDE dan versi core lain belum diuji.
2. Buka INO tersebut. Biarkan [partitions.csv](partitions.csv) di folder sketch agar layout 16 MB sama dengan firmware utama. Sketch tidak memakai NVS atau filesystem. Pilih **Erase All Flash Before Sketch Upload → Disabled**; ekspor arsip penting sebelum reflashing diagnostik.
3. Board **ESP32S3 Dev Module**, flash **16MB**, PSRAM **OPI PSRAM**. Untuk konektor USB native: **USB Mode → Hardware CDC and JTAG**, **USB CDC On Boot → Enabled**. Jika memakai USB-UART bridge, sesuaikan port dan routing Serial ke bridge tersebut.
4. Verify lalu Upload. Bila gagal masuk mode download, tahan BOOT sambil menekan RESET, lepas BOOT lalu ulangi upload.
5. Buka **Serial Monitor 115200**, line ending **Newline**. Kirim `help` untuk panduan. Boot tidak menunggu Serial tersambung.

Alternatif PlatformIO dari root: `pio run -d examples/FortSerialDebug`. Konfigurasi yang disertakan memakai core yang sama tanpa library eksternal.

## Uji pertama: A/B dilepas dari sisi modul ESP32

Kirim satu per satu:

```text
range 1 1
scan
```

Tetap hubungkan power dan TTL modul. Ini menguji ID 1 pada 18 kombinasi baud/parity. `EXACT_TX_ECHO` berarti frame RX delapan byte persis sama dengan request TX percobaan tersebut; belum menentukan letak loopback. CRC salah, frame pendek atau byte lain tetap ditolak. Simpan seluruh log TX/RX.

Sambungkan kembali A/B ke Fort, lalu ulangi perintah yang sama. Bila ID belum diketahui, perluas rentang:

```text
range 1 247
scan
```

Scan penuh berisi **4.446 kandidat**: baud 1200/2400/4800/9600/19200/38400 × parity N/E/O, delapan data bit, satu stop bit. Tanpa respons bisa sekitar satu jam atau lebih. Kirim **x** untuk berhenti, termasuk saat menunggu respons. Selama menunggu hanya x diproses; kirim perintah lain setelah berhenti.

`FOUND` memerlukan respons energi dan CT/PT yang lolos CRC, ID, fungsi, panjang serta rentang rasio. Ini kecocokan protokol, bukan autentikasi model. Setting yang ditemukan hanya dipilih pada pembaca di RAM; meter tidak dikonfigurasi ulang.

## Baca kWh

Jika setting menu Fort sudah diketahui:

```text
id 1
baud 9600
parity N
read
poll
```

`read` membaca sekali; `poll` menyalakan/mematikan pembacaan berulang dengan target dua detik. Timeout dan pencetakan log dapat memperlambatnya. `status` menampilkan setting pembaca; `x` menghentikan polling/scan. Setting tidak disimpan permanen.

Energi sekunder unsigned 32-bit di 0x0082–0x0083 dihitung `raw × 0,001 kWh`. Pengali primer adalah PT numerator/denominator × CT dari 0x002C–0x002E, diterapkan sekali dengan double. Acuan manual ada di [DESIGN](../../docs/DESIGN.md#fort-register-reference).

## Debug aman dan batasnya

Log memuat uptime, ID, baud/parity, alamat/count, TX/RX hex, CRC hitung/terima, echo, timeout dan exception. Data hanya dipakai setelah seluruh validasi lulus. Echo request yang berdiri sendiri dicatat/ditolak, lalu sketch tetap menunggu respons terpisah sampai deadline awal. Data gabungan/ambigu tidak dipotong sembarang agar dianggap valid.

Buffer RX maksimal **64 byte per frame**, tunggu **800 ms**, detail maksimal **empat frame per request** untuk membatasi log; frame valid/exception tetap dilaporkan. Timeout tanpa byte dibedakan dari ada RX tetapi tanpa respons valid. Ini pengamatan buffer UART, bukan timing osiloskop. Log Serial memengaruhi timing; kompilasi bukan bukti uji hardware Fort.

Sketch hanya mengirim fungsi baca **03**, tanpa write register, broadcast, reset meter, tulis NVS atau format filesystem. Firmware utama dan fitur dashboard TX/RX tetap dikembangkan terpisah.

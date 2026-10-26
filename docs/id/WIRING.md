[English](../WIRING.md) · **Bahasa Indonesia**

# Wiring dan komponen

![Modul otomatis pengguna](../assets/rs485-auto.svg)

Modul pengguna: TXD → GPIO18, RXD ← GPIO17, GPIO16 tidak disambungkan. Gambar lengkap/netlist berikut adalah **alternatif modul arah manual RO/DI/DE-/RE**; jalur OLED/RTC/tombol/buzzer tetap sama.

![Alternatif arah manual lengkap](../assets/wiring-overview.svg)

Tutorial dari awal ada di [README](../../README.id.md). Untuk menandai setiap kabel saat perakitan, gunakan [netlist point-to-point CSV](../wiring-netlist.csv). Gambar memakai posisi port logis; ikuti tulisan GPIO pada board, bukan lokasi visual port pada diagram.

Gunakan suplai USB 5 V yang stabil untuk development. Seluruh sinyal GPIO ESP32 adalah 3,3 V. Label N16R8 menunjukkan kapasitas memori; cek bahwa board memiliki pin DevKitC yang disebutkan di sini.

| Komponen / pin | ESP32-S3 | Catatan |
| --- | --- | --- |
| OLED SSD1306 VCC | 3V3 | Asumsi OLED I2C 128×64, alamat 0x3C |
| OLED GND | GND | Ground logika bersama |
| OLED SDA | GPIO8 | Bus I2C bersama RTC |
| OLED SCL | GPIO9 | Bus I2C bersama RTC, 100 kHz |
| DS3231 VCC | 3V3 | Hindari pull-up SDA/SCL ke 5 V |
| DS3231 GND | GND | |
| DS3231 SDA / SCL | GPIO8 / GPIO9 | DS3231 alamat 0x68 |
| Alternatif manual: RO / RX output | GPIO18 / UART RX | Output transceiver ke input ESP32 |
| Alternatif manual: DI / TX input | GPIO17 / UART TX | Output ESP32 ke input transceiver |
| Alternatif manual: DE dan /RE | Keduanya ke GPIO16 | HIGH kirim, LOW terima; kedua enable disatukan |
| Transceiver VCC | 3V3 | Modul milik pengguna sudah dikonfirmasi kompatibel dengan TTL ESP32 3,3 V |
| Transceiver GND | GND logika | Reference bus mengikuti desain isolasi dan meter |
| Transceiver A | Fort **58 / A** | Manual PDF halaman 7/Tabel 3 dan halaman 9/Gambar 5 |
| Transceiver B | Fort **59 / B** | Jangan hubungkan ke terminal switch input |
| Tombol PREV | GPIO4 ↔ tombol ↔ GND | `INPUT_PULLUP`; jangan hubungkan ke 5 V |
| Tombol NEXT | GPIO5 ↔ tombol ↔ GND | |
| Tombol OK | GPIO6 ↔ tombol ↔ GND | |
| Kendali buzzer | GPIO7 → resistor basis/gate | HIGH = aktif; lihat rangkaian di bawah |

Tactile switch 4 kaki biasanya memiliki dua pasang kaki yang sudah terhubung internal. Pastikan dengan continuity meter; gunakan dua sisi yang baru tersambung ketika ditekan. Cukup tiga dari sepuluh tombol.

![Ilustrasi dua pasangan kontak tactile switch](../assets/button-contacts.svg)

Pin GPIO4/5/6 masing-masing menuju satu kontak tombol; kontak satunya menuju GND. Dilepas = HIGH melalui pull-up internal; ditekan = LOW. Tidak perlu menyambung tombol ke VCC.

## Modul RS485 yang digunakan

Modul “MAX485 TTL RS485 3.3V/5V flexible” yang dibeli sudah dikonfirmasi pengguna kompatibel dengan TTL ESP32 3,3 V. Wiring terkonfirmasi adalah TXD modul ke GPIO18, RXD modul ke GPIO17, VCC ke3V3 dan GND keGND. Modul empat pin ini mengatur arah otomatis; GPIO16 tidak disambungkan. RO/DI/DE-/RE hanya untuk alternatif manual.

Jika modul hanya punya TX/RX tanpa DE-/RE dan mengatur arah otomatis, cocokkan TX/RX berdasarkan arah sinyal. GPIO16 dapat dibiarkan tidak tersambung pada modul auto-direction; pengujian turnaround tetap diperlukan. Firmware mengasumsikan tidak ada local echo ketika mengirim.

Untuk instalasi panel pabrik, gunakan antarmuka RS485 terisolasi dan suplai terisolasi yang sesuai lingkungan, cable shield/twisted pair, reference dan proteksi surge sesuai desain panel. Modul TTL sederhana cukup untuk uji meja, tetapi tidak memberi isolasi galvanik.

Port RS485 Fort masih kosong dan belum terhubung ke PLC/BMS/perangkat lain. ESP32 menjadi **satu-satunya master** yang membaca meter.

Manual menempatkan A pada **58** dan B pada **59**. Terminal **70/COM** pada gambar meter adalah common untuk switch input DI1–DI4; terminal tersebut tidak termasuk jalur RS485 dan tidak dipakai sebagai GND logika.

Terminasi sekitar 120 ohm dipasang hanya pada kedua ujung bus jika diperlukan oleh panjang/kecepatan kabel. Periksa resistor bawaan modul sebelum menambah terminasi atau bias. Hindari jaringan bintang. Jangan mengubah koneksi tegangan/arus/CT meter untuk memasang pembaca RS485; pemasangan pada panel dilakukan oleh teknisi yang berwenang.

## Buzzer dua kaki dengan driver NPN

Spesifikasi arus buzzer belum diketahui, jadi default wiring menggunakan driver:

```text
GPIO7 -- resistor 1k -- basis NPN (mis. 2N2222/BC337)
                      emitor -- GND
                      kolektor -- buzzer (-)
3V3 untuk buzzer rating 3V ----- buzzer (+)
basis -- resistor 10k -- GND
```

Tambahkan diode flyback bila buzzer bersifat elektromagnetik sesuai datasheet-nya. Transistor, resistor, dan diode belum ada dalam daftar belanja; tambahkan bila buzzer hanya komponen dua kaki. Untuk modul buzzer tiga pin dengan driver bawaan yang menerima logika 3,3 V, GPIO7 langsung ke pin signal sesuai modul. Jangan langsung membebani GPIO dengan buzzer berarus tidak diketahui.

Posisi B/C/E pada gambar menyatakan fungsi listrik, bukan urutan fisik kaki transistor. Cari urutan pada datasheet part/package yang dipakai. Modul buzzer tiga pin: VCC ke 3V3 sesuai rating, GND ke rail GND, signal/S ke GPIO7. Firmware mengaktifkan buzzer dengan HIGH, sehingga modul active-low membutuhkan penyesuaian polaritas di kode.

## RTC dan baterai

DS3231 tidak memerlukan pin SQW untuk firmware ini. Beberapa breakout memiliki charger untuk LIR2032. Jangan memakai CR2032 non-rechargeable pada rangkaian pengisi aktif; pilih baterai sesuai modul atau nonaktifkan rangkaian charger dengan benar. Semua pull-up I2C harus berujung di 3,3 V; periksa gabungan resistor bawaan OLED/RTC.

## Jika OLED tidak menyala

Cek suplai, ground, SDA/SCL, ukuran display, dan alamat 0x3C/0x3D. Alamat dapat diubah di `firmware/include/board.h`. Firmware tetap menjalankan hotspot dan pembacaan meski OLED tidak terdeteksi. OLED 128×32 atau SH1106 memerlukan perubahan driver/layout.

# Wiring hardware

[English](../WIRING.md) · **Bahasa Indonesia**

Contoh khusus Fort di bawah tetap berlaku untuk Fort. Untuk Acrel, simpan model yang dipilih terlebih dahulu lalu ikuti [Profil meter](METER_PROFILES.md) untuk register, terminal, konversi dan alur diagnostiknya.

Rangkaian terpasang memakai transceiver RS485 manual yang telah dikonfirmasi melalui sketch ModbusMaster berhasil. Ikuti label GPIO pada ESP32-S3 N16R8 dan nama sinyal pada modul karena diagram menunjukkan net listrik, bukan urutan header fisik.

![Wiring lengkap arah manual](../assets/wiring-overview.svg)

## Catu dan I2C bersama

Lepas USB sebelum merangkai, lalu pisahkan rel 3V3 dan GND serta jembatani rel breadboard yang terputus jika kedua bagiannya dipakai. OLED mendapat VCC/GND dari rel tersebut, SDA pada GPIO8 dan SCL pada GPIO9. DS3231 memakai rel serta net SDA/SCL yang sama, sedangkan SQW/32K tidak dihubungkan. Firmware memakai SSD1306 128×64 pada 0x3C dan RTC pada 0x68.

Urutan konektor mengikuti label modul karena dapat berbeda antar-breakout. Pull-up I2C mengacu ke 3,3 V, dan jenis baterai RTC mengikuti rangkaian charging yang benar-benar ada pada modul. OLED yang tidak terpasang tidak menghalangi startup AP serta pembaca.

## RS485 dan kontrol arah

| Dari | Ke | Fungsi |
| --- | --- | --- |
| GPIO17 / TX UART1 | DI | ESP32 mengirim ke input driver |
| RO | GPIO18 / RX UART1 | Output receiver menuju ESP32 |
| GPIO16 | DE dan /RE disatukan | HIGH mengirim dengan receiver mati, LOW menerima |
| 3V3/GND board | VCC/GND modul kompatibel | Catu dan referensi lokal |
| A | Fort 58/A | Konduktor diferensial |
| B | Fort 59/B | Konduktor diferensial |

Port Fort khusus untuk ESP32 sebagai satu-satunya master. Terminal 70/COM merupakan common input sakelar dan bukan ground RS485, sehingga tidak dimasukkan ke [netlist](../wiring-netlist.csv). Kompatibilitas TTL serta rangkaian tegangan rendah merupakan fakta proyek yang telah dikonfirmasi.

Pilih **Manual direction** pada Meter setup. GPIO arah aktif 100 µs sebelum request dan dilepas setelah flush UART, kemudian receiver diberi waktu 100 µs. [Modul otomatis empat pin](../assets/rs485-auto.svg) adalah alternatif terpisah yang memerlukan pilihan Automatic dan GPIO16 tidak terhubung. Jangan mencampur kedua diagram atau menganggap posisi label konektornya sama.

Ikuti rancangan referensi, shield, terminasi serta isolasi instalasi. Periksa resistor yang sudah ada sebelum menambah bias/terminasi dan pisahkan pemasangan pembaca ini dari terminal pengukuran tegangan/arus meter.

## Tombol tactile

![Pasangan kontak sakelar](../assets/button-contacts.svg)

Tombol empat kaki memiliki dua pasangan yang selalu terhubung. Tentukan pasangan melalui continuity, lalu gunakan satu kaki dari setiap pasangan agar penekanan menghubungkan GPIO ke GND. PREV memakai GPIO4, NEXT GPIO5 dan OK GPIO6. VCC tidak dihubungkan ke tombol karena input aktif-LOW memakai pull-up internal serta debounce.

## Buzzer opsional dan LED bawaan

Diagram lengkap memakai driver NPN untuk buzzer aktif dua kabel. GPIO7 menuju basis melalui 1 kΩ, emitter ke GND, collector ke negatif buzzer dan positif buzzer ke catu 3V3 yang kompatibel. Resistor 10 kΩ basis-ke-ground menjaga transistor mati saat reset. Komponen driver ini merupakan tambahan dan urutan B/C/E fisik mengikuti datasheet package transistor.

Modul buzzer dengan driver sendiri dapat memakai GPIO7 sebagai sinyal bila polaritas input serta catunya sesuai, dengan output firmware aktif HIGH. GPIO48 sudah terhubung ke RGB bawaan tanpa kabel tambahan, sedangkan rotasi OLED memakai `OLED_ROTATE_180` pada [board.h](../../firmware/include/board.h).

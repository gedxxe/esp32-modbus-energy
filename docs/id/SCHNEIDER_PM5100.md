# Schneider PM5110: pembacaan kWh D+R

[Bahasa Indonesia](SCHNEIDER_PM5100.md) · [English](../SCHNEIDER_PM5100.md)

## Hasil pengujian perangkat pada 6 November 2026

ESP32-S3 berhasil berkomunikasi dengan **Schneider Electric PM5110** (Product ID 15270 / `0x3BA6`) melalui Modbus RTU **FC03**, Slave ID **1**, **19200 baud**, **8E1**. Wiring tetap TX GPIO17, RX GPIO18, DE dan /RE digabung pada GPIO16.

**Nilai kWh utama untuk pencatatan adalah D+R (Delivered + Received), bukan Delivered-only (D) dan bukan energi reaktif (kvarh).**

| Sumber | Register Schneider | Alamat PDU | Format | Hasil validasi |
| --- | ---: | --- | --- | ---: |
| **Energi aktif D+R, kWh utama** | **2704** | **2703 / `0x0A8F`** | **Float32 ABCD ×2 register** | **412.691,00000 kWh** |
| Energi aktif D, historis | 2700 | 2699 / `0x0A8B` | Float32 ABCD ×2 register | 411.196,75000 kWh |

Frame D+R yang terbukti CRC OK: TX `01 03 0A 8F 00 02 F6 38`, RX `01 03 04 48 C9 82 60 5D 25`. Payload `48 C9 82 60` menjadi **412691.0 kWh**. Selisih 1494,25 kWh dengan counter Delivered-only **tidak boleh** dianggap sebagai pemakaian dalam satu interval.

Register pada Excel dimulai dari 1; alamat Modbus PDU dimulai dari 0. Nilai energi sudah dalam **kWh**, bukan MWh atau Wh. CT/PT meter tidak boleh dikalikan lagi. Pada sekitar 412 ribu kWh, resolusi Float32 adalah 0,03125 kWh per langkah.

## Keamanan histori dan profil

| ID | Key | Fungsi |
| --- | --- | --- |
| 6 | `schneider-pm5100` | Histori **Delivered-only** yang harus tetap dapat dibaca, tetapi tidak boleh dipilih untuk pencatatan baru |
| **7** | **`schneider-pm5100-dr`** | **Pencatatan PM5110 baru menggunakan D+R** |

Saat konfigurasi tersimpan dengan ID 6 dibuka oleh firmware baru, reader aktif otomatis diarahkan ke ID 7 dan ikatan perangkat lama dilepas. Nilai arsip ID 6 tidak diubah. Perbedaan ID memutus kontinuitas sehingga delta harian tidak mencampur D dan D+R. Konfigurasi NVS lama tidak ditimpa sampai pengaturan meter disimpan lagi secara normal.

Checkpoint tetap menyimpan payload Float32 32-bit dalam format arsip yang sama. Counter pembanding 64-bit pada register 3212 memiliki satuan Wh, **bukan** sumber utama yang dipotong menjadi 32-bit.

## Register tambahan (bukan syarat pencatatan kWh)

| Parameter | Alamat PDU (HEX) | Format |
| --- | --- | --- |
| Energi D+R | **`0x0A8F`** | **Float32, 2 word** |
| Current A | `0x0BB7` | Float32, 2 word, A |
| Voltage A-N | `0x0BD3` | Float32, 2 word, V |
| Active Power Total | `0x0BF3` | Float32, 2 word, kW |
| Power Factor Total | `0x0C0B` | 4Q Float32, 2 word |
| Frequency | `0x0C25` | Float32, 2 word, Hz |
| Reactive Delivered (bukan energi D+R) | `0x0A93` | Float32, 2 word, kvarh |

Scanner LCD memakai arus rata-rata di register 3010 dan tegangan L-L rata-rata di 3026, sedangkan descriptor produksi memakai arus fase A di 3000 dan tegangan A-N di 3028. Keduanya tidak boleh dibandingkan langsung. Decoder power factor tetap mengikuti encoding empat kuadran dari worksheet *Data Types*.

## Pengaturan dan debugging

Pada dashboard ESP32, buka **Meter setup**, pilih **Schneider PM5110 (D+R kWh)**, isi ID **1**, baud **19200**, parity **Even**, arah **Manual GPIO16**, lalu simpan.

Di **Meter setup → Diagnostic console**, masukkan baris berikut **satu per satu** pada kolom **Command input**, tekan **Run command** setiap kali. Alternatifnya gunakan **Arduino IDE Serial Monitor INPUT** dengan baud **115200**, line ending **Newline**, lalu Enter/Send.

```text
profile schneider-pm5100-dr
id 1
baud 19200
parity E
status
intermediate
read
deeper
```

Perintah `read` seharusnya menghasilkan kWh **D+R**, bukan Delivered-only. Angka 412691.0 kWh adalah pembacaan saat pengujian, bukan nilai konstan. Lakukan *Resume measurement* setelah diagnostik aktif selesai.

Pengiriman website memakai key **`schneider-pm5100-dr`** dan identitas profil internal 7. Apabila website atau registrasi perangkat masih mengunci key `schneider-pm5100`, perlu pembaruan integrasi secara eksplisit; jangan melabeli D+R sebagai D hanya agar data diterima. Gateway LoRa yang memvalidasi ID juga perlu mengenali ID 7.

Uji hardware membuktikan CRC dan decoding counter. Firmware terpasang, gateway, dan website masih memerlukan uji end-to-end sebelum dinyatakan siap produksi.

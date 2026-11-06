# Profil pembacaan Schneider PM5100

[Bahasa Indonesia](SCHNEIDER_PM5100.md) · [English](../SCHNEIDER_PM5100.md)

Profil `schneider-pm5100` memakai ID permanen **6**. Definisi profil lama dan arsip checkpoint 446 byte tidak berubah. Pemetaan dibaca dari workbook milik pengguna `PM51xx_PM53xx_PMC Register List_v2011_v2021_R01.xls`, terutama worksheet Register List dan Data Types. File sumber tidak diubah. [Catatan teknis English](../SCHNEIDER_PM5100.md) memuat tabel lengkap, baris sumber dan referensi Schneider, sedangkan [rekaman sumber](../references/pm5100-registers.json) memuat hash workbook serta alamat yang dipakai.

## Register dan decoding

Nomor register dalam workbook memakai desimal mulai dari satu. Firmware mengirim alamat PDU mulai dari nol, sehingga energi register 2700 dibaca melalui FC03 alamat **2699 (`0x0A8B`) sebanyak dua register**. Parameter lain juga dibaca dua register dengan word paling signifikan lebih dahulu.

| Parameter | Nomor di workbook | Alamat PDU | Satuan |
| --- | --- | --- | --- |
| Energi aktif delivered/import | 2700 | 2699 | kWh |
| Energi reaktif delivered | 2708 | 2707 | kvarh |
| Arus fase A | 3000 | 2999 | A |
| Tegangan fase A–netral | 3028 | 3027 | V |
| Daya aktif total | 3060 | 3059 | kW |
| Power factor total | 3084 | 3083 | cos φ setelah decoding |
| Frekuensi | 3110 | 3109 | Hz |

Energi yang dicatat adalah delivered/import, bukan ekspor, energi neto, atau jumlah delivered + received pada register 2704. Nilai sudah mengikuti pengukuran dan konfigurasi CT/VT meter, sehingga ESP32 tidak mengalikan CT/PT lagi. Tidak ada penulisan register meter.

Power factor memakai encoding empat kuadran. Raw di atas +1 diubah menjadi `2 − raw`, sedangkan raw di bawah −1 menjadi `−2 − raw`. Misalnya +1,2 menjadi +0,8 dan −1,2 menjadi −0,8. NaN dan nilai di luar −2…+2 ditolak. Informasi leading/lagging tidak ditampilkan sebagai kolom terpisah.

Register energi INT64 pada 3204 memakai empat register dalam Wh. Profil ini tidak memotongnya menjadi 32 bit. Firmware memakai salinan Float32 kWh yang memang disediakan di register 2700 agar penyimpanan tetap kompatibel. Resolusinya terbatas: sekitar satu juta kWh, satu langkah Float32 sebesar 0,0625 kWh. Tiga angka desimal pada layar tidak menambah ketelitian sumber. Dukungan INT64 presisi penuh kelak memerlukan profil dan format penyimpanan baru.

Hanya kWh valid yang wajib tersedia untuk checkpoint. PF yang tidak tersedia saat tanpa beban atau tegangan A–N yang tidak tersedia pada rangkaian tanpa netral tidak boleh menggagalkan energi. Parameter tambahan yang gagal tetap tidak tersedia, bukan nol. Pembacaan dilakukan berurutan, sehingga bukan snapshot listrik serentak.

## Pemakaian

Buka **Meter setup** pada dashboard ESP32, pilih **Schneider PM5100**, lalu isi slave ID, baud dan parity sesuai menu komunikasi meter. Baud yang didokumentasikan adalah 9600, 19200 dan 38400. Simpan dan cocokkan kWh dengan halaman delivered/import pada LCD, termasuk satuan dan pengali tampilannya. Wiring UART tetap TX17 ke DI, RX18 dari RO, dan GPIO16 ke DE serta /RE yang disatukan. Gunakan label polaritas RS485 milik Schneider, jangan memakai nomor terminal Fort.

Scanner memakai PDU 2699/2 dengan bukti respons CRC valid berulang. Respons tidak membuktikan merek perangkat. Untuk diagnostik, ketik perintah berikut satu per satu pada **web Command input lalu Run command**, atau **Arduino IDE Serial Monitor INPUT lalu Enter/Send, baud 115200 dengan Newline**:

```text
profile schneider-pm5100
status
intermediate
read
deeper
```

Profil diagnostik berada di RAM. `intermediate` memeriksa alamat energi, `read` mendekode kWh dan `deeper` memeriksa rencana register profil. Gunakan kontrol `id`, `baud` dan `parity` yang sudah tersedia bila diperlukan. Salin **OUTPUT** ke chat dukungan bila diminta, lalu resume produksi setelah diagnostik aktif.

Binding perangkat di website terpisah harus memakai `schneider-pm5100`. Gateway LoRa terpisah yang membatasi ID profil perlu mengenali ID 6. Pengujian pada PM5100 fisik belum dilakukan untuk perubahan ini.

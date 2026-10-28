# Profil meter dan pemeriksaan lapangan

[Bahasa Inggris](../METER_PROFILES.md) · **Bahasa Indonesia** · [Ringkasan](../../README.id.md)

Satu station membaca satu meter yang dipilih melalui transport FC03 bersama. Pilih model terpasang pada **Meter setup → Model meter**, samakan slave ID, baud serta parity, kemudian simpan pengaturan. Profil menentukan register produksi, probe scanner, baud rate pencarian, dekode rasio dan plan diagnostik. Scan menemukan perangkat yang membalas pada setting tertentu, tetapi tidak dapat membuktikan merek atau arti fisik angka hanya dari CRC yang benar.

![Pemilih model representatif dengan data perangkat simulasi](../assets/ui-meter-profile.png)

## Referensi dan batas revisi

Fort tetap memakai peta yang sudah digunakan dari [manual Fort](../../Fort%20Power%20Meter_User%20Manual_compressed.pdf). Profil Acrel mengikuti [manual ADL400 V1.3 yang disediakan](../../Acrel%20ADL400%20Manual.pdf), sebanyak 27 halaman. Nomor halaman PDF dihitung dari cover sebagai halaman 1 dan berbeda empat halaman dari nomor cetak.

Catatan tabel energi pada halaman PDF 16 menggabungkan tulisan kVarh untuk energi aktif dan reaktif. [Manual ADL400 T1.4 dari produsen](https://www.acrel.ng/uploads/file/20240819/adl400-installation-and-operation-instruction.pdf), tabel 8, memperjelas bahwa blok energi aktif alamat rendah adalah UINT32 dengan resolusi 0,01 kWh dan memerlukan PT×CT satu kali. Dokumen tersebut dipakai sebagai pemeriksaan silang, bukan pengganti revisi milik pengguna. Resolusi daya dan register float pada revisi ADL400/MID yang lebih baru berbeda, sehingga profil ini tidak mengklaim mendukung seluruh varian ADL400, ADL400-D, MID atau ADL400W.

Produksi memakai high word terlebih dahulu untuk nilai dua register, dengan byte big-endian di dalam masing-masing register Modbus. Manual yang disediakan tidak secara eksplisit menjelaskan urutan dua word. Diagnostik menampilkan word mentah dan menyediakan pembandingan `order normal` dengan `order swap`. Pilihan tidak dilakukan otomatis berdasarkan angka yang terlihat masuk akal. Bandingkan kWh impor dengan LCD meter dan hentikan penerimaan bila urutan atau skalanya tidak sesuai. Override diagnostik tidak mengubah peta produksi secara diam-diam.

## Peta register FC03

Alamat berikut merupakan offset PDU heksadesimal tanpa penambahan 40001 atau pengurangan otomatis. Kolom length manual Acrel dihitung dalam byte, sehingga length 4 berarti dua register 16-bit.

| Pembacaan | Fort | Acrel ADL400 V1.3 | Halaman PDF Acrel |
| --- | --- | --- | --- |
| Arus fasa A / probe awal | `0x0058`, 2 word, UINT32 × 0,0001 A | `0x0064`, 1 word, UINT16 × 0,01 A | 17 |
| Tegangan fasa A | `0x0042`, 2 word, UINT32 × 0,0001 V | `0x0061`, 1 word, UINT16 × 0,1 V | 17 |
| Daya aktif total | `0x006A`, 2 word, INT32 × 0,0001 kW | `0x016A`, 2 word, INT32 × 0,0001 kW | 19 |
| Frekuensi | `0x0080`, 1 word, UINT16 × 0,01 Hz | `0x0077`, 1 word, UINT16 × 0,01 Hz | 17 |
| Energi aktif forward/impor | `0x0082`, 2 word, UINT32 × 0,001 kWh | `0x000A`, 2 word, UINT32 × 0,01 kWh | 16 |
| Rasio trafo | `0x002C`, 3 word: pembilang PT, penyebut PT, CT | `0x008D`, 2 word: PT, CT. Penyebut 1 | 18 |
| Float energi primer opsional | `0x008A`, kandidat diagnostik | Tidak tersedia pada peta yang diberikan | 22–23 |

Rumus energi adalah `raw × skala profil × pembilang PT / penyebut PT × CT`, menggunakan `double`. Rasio harus berada dalam 1–9999 sebelum energi kumulatif diterima. Contohnya, raw Acrel 123456, PT 2 dan CT 40 menghasilkan `123456 × 0,01 × 2 × 40 = 98764,8 kWh`. Counter sekunder tetap terlihat sebelum rasio diterima. Panel register menampilkan arus, tegangan dan daya hasil dekode register tanpa perkalian trafo tambahan.

Counter yang dipilih adalah energi forward `0x000A`, bukan total gabungan `0x0000` atau reverse `0x0014`. Register frozen history opsional milik meter berbeda dari checkpoint ESP32 dan belum diimpor atau diubah konfigurasinya. Seluruh request tetap FC03 meskipun manual meter juga menjelaskan fungsi tulis.

## Wiring

![Diagram wiring Acrel lengkap](../assets/wiring-acrel-adl400.svg)

Sisi ESP32 tetap sama: GPIO17 ke DI, GPIO18 menerima RO, dan GPIO16 ke DE serta /RE yang disatukan. Callback arah memberi waktu settling 100 µs sebelum TX dan sesudah beralih ke RX. Modul yang dibeli sudah dikonfirmasi kompatibel dengan TTL 3,3 V ESP32.

Untuk Acrel ADL400 V1.3, sambungkan A transceiver ke **21/A1** dan B ke **22/B1**, sesuai gambar 7 pada halaman PDF 9. Terminal **17/Ep+ dan 18/Ep− adalah keluaran pulsa**, bukan RS485. Gambar komunikasi tidak menyediakan terminal ground RS485. Fort tetap memakai **58/A dan 59/B**, sedangkan 70/COM adalah common input switch. Penjelasan ini hanya membahas terminal sinyal, bukan rangkaian mains atau CT meter. [Netlist Acrel](../wiring-acrel-adl400.csv) mencatat sambungan periferal lainnya.

## Langkah di panel

1. Pilih model yang terpasang, samakan ID, baud, parity dan arah, lalu simpan. Scanner baru mengikuti model tersebut setelah penyimpanan selesai. Upgrade instalasi lama mempertahankan profil Fort.
2. Periksa panel register. Respons sah memenuhi CRC, ID, fungsi, byte count dan panjang frame. Exception sah membuktikan komunikasi, tetapi belum menerima peta register. RX noise, echo dan CRC error tidak membuktikan meter sudah tersambung dengan benar.
3. Jika setting belum diketahui, mulai dengan rentang ID kecil dan **Format tersimpan**. Matrix penuh memakai enam baud rate untuk Fort atau lima untuk ADL400 V1.3, masing-masing dengan N/E/O. Manual Acrel halaman PDF 15 mencantumkan 1200, 2400, 4800, 9600 dan 19200 baud. Paragrafnya menyebut None tetapi menunya mencantumkan None/Odd/Even, sehingga ikuti setting menu aktual. Gunakan ID Modbus standar 1–247 meskipun batas atas pada tabel menu tidak konsisten.
4. Pada web **Command input**, ketik `status` lalu pilih **Run command**. Ketik `intermediate` secara terpisah dan pilih Run command, lalu gunakan `deeper` untuk plan read-only profil tersebut. Periksa word, rasio, kWh impor dan exception. Bila membutuhkan bantuan, salin hasil **OUTPUT** ke chat dukungan. Label log bukan perintah.
5. Bandingkan kWh dengan halaman energi forward di LCD pada waktu yang sama. Cocokkan CT/PT dengan menu serta periksa arus, tegangan dan frekuensi. CRC sah tidak mendeteksi revisi meter. Simpan output raw bila nilainya berbeda faktor sepuluh atau tidak mengikuti LCD.
6. Pilih **Resume measurement**, pastikan model produksi kembali sesuai yang disimpan, lalu tunggu checkpoint terjadwal menjadi Tercatat. Model asal setiap baris tersedia pada tabel checkpoint dan CSV.

## Diagnostik Arduino mandiri

Lokasi [FortSerialDebug](../../examples/FortSerialDebug/README.id.md) dipertahankan agar kompatibel, tetapi library yang sama mendukung kedua profil. Setelah upload, buka **Arduino IDE Serial Monitor**, pilih **115200 baud** dan **Newline**. Ketik setiap baris berikut pada Serial Monitor **INPUT**, lalu tekan **Enter/Send** untuk masing-masing baris:

```text
profile acrel-adl400
id 1
baud 9600
parity N
status
intermediate
deeper
report
```

ID serta setting serial di atas adalah contoh yang harus disamakan dengan meter aktual. Gunakan `profile fort` untuk Fort. Perintah profil mengatur ulang probe, alamat/skala energi, susunan rasio, raw plan dan baud matrix, tetapi mempertahankan pin serta setting link. Perintah ini tidak menyimpan konfigurasi produksi. `primary` menampilkan unsupported pada Acrel karena manual tersebut tidak mendokumentasikan register float energi primer. Delapan resep kasus yang sudah ada tetap merupakan contoh Fort kecuali resep secara eksplisit memilih profil lain.

## Arsitektur dan migrasi

[MeterProfiles.h](../../firmware/lib/FortMeter/src/MeterProfiles.h) menyimpan descriptor register, tipe angka, skala, susunan rasio, batas baud dan label terminal tanpa alokasi dinamis. Akuisisi serta diagnostik memakai descriptor yang sama melalui ModbusMaster. API status mengirim katalog yang sama ke browser untuk pilihan model dan label register, sehingga tidak ada parser CRC khusus merek yang terpisah.

Setting tetap berukuran 12 byte dengan magic baru dan byte profil eksplisit. Setting v1/v2 dengan CRC benar dimigrasikan ke Fort tanpa kehilangan ID, baud atau parity. Pilihan arah manual/otomatis v2 juga dipertahankan. Record checkpoint tetap 426 byte. Schema 2 memakai satu byte cadangan per baris untuk ID profil, sementara schema 1 dibaca sebagai Fort. Dekoder memeriksa kembali energi menggunakan profil serta rasio asal baris itu. Pergantian model memutus kontinuitas dan membersihkan nilai live. Selisih hanya tersedia jika model, setting serial, rasio dan generasinya cocok.

Meter tambahan memerlukan ID profil baru, sumber yang diperiksa, descriptor serta pengujian. Jangan mengganti arti ID lama dengan skala atau peta baru karena arsip bergantung pada arti tersebut. Encoding, susunan rasio atau urutan byte baru memerlukan dekoder bertipe serta fixture yang eksplisit, bukan fallback otomatis setelah probe gagal. Kepemilikan core, retry, Wi-Fi, RTC dan jendela checkpoint tetap digunakan bersama. Pengujian native dan browser bukan pengganti penerimaan pada Acrel fisik di pabrik.

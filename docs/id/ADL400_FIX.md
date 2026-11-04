# Perbaikan ADL400 berdasarkan scan lapangan

Jalur pembacaan 3.5.0 mengikuti request diagnostik independen yang diberikan: kontak dan energi memakai FC03 `0x0000/2`, PT `0x008D/1`, dan CT `0x008E/1`. Arus opsional pada `0x0064` tidak lagi menjadi syarat pembacaan energi. UInt32 ABCD /100 dan PT × CT satu kali tetap dipakai. Instalasi lapangan tersebut tetap memerlukan slave 220, 9600 dan 8N1 disimpan pada Pengaturan meter.

[English](../ADL400_FIX.md) · **Bahasa Indonesia** · [Profil meter](METER_PROFILES.md)

Versi 3.3.0 langsung mengikuti hasil scan pengguna. Tetap hanya ada satu pilihan **Acrel ADL400**. Sumber produksi sekarang total energi aktif `0x0000`, dua register UInt32 ABCD. Counter forward `0x000A` dan reverse `0x0014` hanya menjadi pembanding diagnostik, tanpa opsi sumber tambahan atau fallback otomatis.

## Temuan dan koreksi

Reader sebelumnya memakai forward `0x000A`, sementara raw 9798 yang dikonfirmasi scan berasal dari total `0x0000`. Jadi reader dapat menanyakan kuantitas berbeda dari pengujian yang berhasil. Skala energi 0,01 dan rumus PT/CT sebelumnya sudah benar. Tidak ada dasar untuk menyebut CRC rusak atau menambahkan faktor koreksi. Power `0x016A` pada reader lama memang tercantum dalam manual V1.3, tetapi pengujian lapangan memakai blok integer rendah `0x006A` dan float `0x53xx`. Reader baru mengikuti alamat hasil scan tersebut.

Instalasi yang diuji memakai **ID 220, 9600, 8N1**, TX17, RX18 dan GPIO16 ke DE-/RE. ID 220 tetap konfigurasi per meter, bukan default seluruh ADL400. Atur ID aktual pada Meter setup. Scan rentang 1–16 tidak akan menemukan 220, dan scan tidak diperlukan jika alamat sudah diketahui.

## Mapping produksi

Semua request FC03 memakai alamat heksadesimal tanpa offset 40001. Float32 memakai dua register **ABCD**, berdasarkan hasil scan voltage/frequency serta tabel float manual. CT/PT tidak diterapkan pada nilai listrik.

| Parameter | Alamat | Register | Dekode |
| --- | --- | --- | --- |
| Probe kontak | `0x0000` | 1 | UInt16 × 0,01 A |
| Total energi aktif | `0x0000` | 2 | UInt32 ABCD × 0,01 kWh, lalu PT × CT sekali |
| PT / CT | `0x008D` / `0x008E` | 2 bersama | UInt16 PT dan CT, keduanya 1–9999 |
| Voltage A | `0x5300` | 2 | Float32 ABCD, V |
| Current A | `0x530C` | 2 | Float32 ABCD, A |
| Total active power | `0x006A` | 1 | Int16 × 0,001 kW |
| Total PF | `0x5330` | 2 | Float32 ABCD |
| Frequency | `0x5332` | 2 | Float32 ABCD, Hz |

Halaman PDF 16–19 dan 22–23 pada [manual lokal](../../Acrel%20ADL400%20Manual.pdf) sudah diperiksa secara visual. Panjang 4 pada tabel adalah byte, berarti dua register. Halaman 17 mencetak kWh di sebelah active power, yang tidak konsisten secara dimensi. Descriptor power rendah menggunakan 0,001 kW sesuai kuantitas dan blok power kW yang terpisah. Nilai power terkalibrasi belum disertakan dalam hasil scan, sehingga kecocokan power tetap perlu dibandingkan dengan LCD. Float power `0x5318`, reactive power `0x5320` dan apparent power `0x5328` dicetak sebagai pembanding raw tanpa mengarang skalanya.

## Perhitungan energi

```text
raw = (uint32_t(highWord) << 16) | lowWord
energi_sekunder = double(raw) / 100.0
energi_kWh = energi_sekunder × PT × CT
9798 / 100 × 1 × 80 = 7838,40 kWh
```

LCD referensi menunjukkan 7838,9 kWh. Pada PT 1 dan CT 80, satu raw count setara **0,8 kWh**, sehingga selisih 0,5 kWh lebih kecil dari satu langkah counter. Kuantisasi, waktu baca berurutan atau halaman counter berbeda dapat menjelaskan selisih sebesar ini, tetapi data belum mengisolasi satu penyebab. Tidak ada offset, gain tambahan atau pemaksaan hasil menjadi 7838,9. Reverse dicatat sesuai respons, bukan diasumsikan nol.

Word digabung sebelum scaling. Nilai register sekunder di API adalah 97,98 kWh, sementara counter utama menerima 7838,40 kWh. `profiles::primaryEnergy` menerapkan PT/CT sekali. Checkpoint menerima raw UInt32 beserta metadata rasio dan menghitung hasil yang sama, bukan mengalikan ulang angka API yang sudah diskalakan. Browser tidak menambahkan pengali energi.

## Cara memakai

1. Pilih **Acrel ADL400** pada **Meter setup**, isi ID aktual dan serial, lalu simpan. Untuk instalasi yang dilaporkan gunakan 220, 9600 dan parity N. Tidak ada penulisan setting meter.
2. Overview menampilkan **Total energi aktif**. Panel register menunjukkan `0x0000` serta nilai sekunder, sedangkan counter utama sudah mencakup PT/CT. Voltage dan frequency memakai float hasil scan.
3. Untuk pembandingan lengkap, buka **Diagnostic console**, pilih preset **Pembandingan energi ADL400**, lalu tekan **Run command**. Ini investigasi aktif read-only sehingga pencatatan dijeda sampai **Resume measurement**. Diagnostik bukan syarat memilih sumber energi.

Atau ketik setiap baris berikut pada **web Command input + Run command** setelah masuk diagnostik, satu baris per klik. Pada INO mandiri gunakan **Arduino IDE Serial Monitor INPUT, 115200 baud, Newline, lalu Enter/Send** per baris. Ini bukan perintah terminal komputer.

```text
profile acrel-adl400
id 220
baud 9600
parity N
read
adl400
resume
```

`read` memakai jalur energi produksi. `adl400` menjalankan satu report terbatas yang membaca PT/CT sebelum dan sesudah counter, total/forward/reverse/reaktif UInt32, energi sekunder dan hasil rasio, langkah counter, waktu baca serta tujuh pembanding float ABCD/CDAB. Bila rasio gagal atau berubah, hasil scaled tidak dicetak. Register gagal dilaporkan unavailable, bukan nol. Report menyatakan bahwa pembacaan berurutan, bukan serentak dengan LCD. Stop tetap dilayani melalui hook transport. Report ini tidak dicetak berulang dalam polling normal.

Counter reaktif `0x001E` dicetak dalam kvarh dengan interpretasi rasio yang sama, tanpa menambah seri checkpoint. Perintah `reg 0000 2`, `reg 000A 2`, `reg 0014 2`, `reg 001E 2`, `reg 008D 2` atau `reg 5300 2` dapat dimasukkan pada input perangkat yang sama. Salin **OUTPUT** ke support hanya bila ingin membagikan hasil, bukan mengetik label output sebagai perintah.

## Kompatibilitas

ID internal historis 1 mempertahankan arti energi forward dan konversi lama untuk arsip. ID ini disembunyikan dari pilihan dan tidak dapat diaktifkan sebagai binding produksi tersimpan. ADL400 saat ini menggunakan ID internal tetap 5 dengan public key `acrel-adl400` yang sama. Konfigurasi aktif lama otomatis diperbarui tanpa mengubah slave ID, baud, parity atau direction. Binding nama lama dilepas agar tidak diklaim memverifikasi counter baru, tetapi record registry dan capture tetap dipertahankan. Perubahan sumber memutus kontinuitas tanpa menulis ulang rekaman yang sudah lengkap. Tidak ada opsi sumber ADL400 tambahan.

Mapping Fort dan Habuer tetap, tanpa parser Modbus baru, perubahan Wi-Fi, jadwal checkpoint atau schema flash. Tes host mereplay frame valid untuk ID 220 melalui library ModbusMaster sebenarnya dan memeriksa 7838,40 kWh, dekode float, rasio berubah, reverse nol/nonzero, gagal tanpa fallback, migrasi konfigurasi serta round trip checkpoint. Tes web menggunakan byte gzip sebenarnya dan memastikan hanya satu pilihan ADL400.

Hasil scan fisik pengguna diterima sebagai referensi. Firmware terintegrasi ini belum dihubungkan ke meter tersebut dalam pengujian lokal. Penerimaan hardware yang tersisa adalah menjalankan firmware baru dan membandingkan halaman LCD yang sama pada waktu berdekatan, termasuk total, forward dan reverse. Ini tidak meminta pengulangan investigasi wiring atau perubahan terhadap finding scan.

## Audit tambahan sesudah merge

- Preset diagnosis mendalam ADL400 sekarang membaca PT `0x008D/1` dan CT `0x008E/1` melalui dua permintaan FC03 terpisah. Ini sama dengan sketch referensi dan jalur produksi, bukan `0x008D/2` sekaligus.
- Echo delapan byte dari TX tetap **tidak dianggap respons meter**. Parser ADL400 kini tetap menunggu respons terpisah yang lolos ID, function, panjang data, dan CRC setelah echo. Uji sintetis mencakup echo saja (gagal), echo lalu respons valid (berhasil), CRC salah (gagal), dan frame CRC salah lalu respons valid (berhasil).
- Perubahan ini tidak membuktikan komunikasi dengan meter fisik. ID 220 adalah pengaturan meter yang pernah dites, bukan nilai default semua ADL400. Jalur ModbusMaster Fort/Habuer tidak berubah.

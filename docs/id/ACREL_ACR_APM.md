# Acrel ACR10R-DxxTE dan APM800/801/810

[English](../ACREL_ACR_APM.md) · **Bahasa Indonesia** · [Profil meter](METER_PROFILES.md)

Versi 3.2.0 menambahkan ID profil 3 (`acrel-acr`) dan 4 (`acrel-apm`) tanpa mengubah ID, register atau konversi Fort, ADL400 dan Habuer. Semua transaksi tetap FC03 read-only melalui library bersama. [Tabel teknis lengkap](../ACREL_ACR_APM.md#production-read-descriptors) mencatat alamat, jumlah word, decoder, skala, unit dan klasifikasi setiap parameter produksi.

## Perbedaan manual yang ditangani

[Manual ACR](../../Acrel%20ACR%20Modbus.pdf) berlaku untuk ACR10R-DxxTE. Halaman PDF 14–15 (cetak 11–12) menyebut energi Float32 primer pada `0x0047`, tetapi tidak menjelaskan unit energi dan urutan word. Profil ini adalah kandidat ABCD dengan unit kWh/kvarh, sehingga perlu pembandingan dengan LCD sebelum diaktifkan. Jangan menganggap seluruh seri ACR mempunyai peta yang sama.

[Manual APM](../../Acrel%20APM%20Modbus.pdf) menggunakan alamat **desimal**, tanpa dikurangi satu. Tabel halaman PDF 63 (cetak 60) menyebut energi primer pada **3000–3001 dalam Wh**, sedangkan contoh halaman 74 (cetak 71) menyebut **3050–3051**. Firmware mengikuti tabel pada 3000 (`0x0BB8`) dan tidak berganti alamat otomatis berdasarkan angka yang terlihat masuk akal. ABCD mengikuti contoh Float32 high-word-first pada manual. Ilustrasi konversi 52140 ke kWh pada halaman 74 juga tidak konsisten. Konversi mengikuti unit 1Wh pada tabel dan contoh eksplisit 589000Wh = 589kWh pada halaman yang sama. Wh dibagi 1000 menjadi kWh, dan varh dibagi 1000 menjadi kvarh.

Pembacaan APM memakai arus primer 1100, tegangan primer 1120, daya aktif primer 1156, PF 1183, frekuensi 252, energi aktif primer 3000 dan energi reaktif primer 3004. Data sekunder tidak digabungkan sebagai pengganti energi primer. Tidak ada pengali CT/PT tambahan pada kedua profil baru, meskipun trafo fisik bukan 1:1.

## Aktivasi dari web lokal

1. Pilih model pada **Meter setup → Meter model**, cocokkan slave ID/baud/parity dan simpan. GPIO tetap TX17→DI, RX18←RO, serta GPIO16→DE dan /RE yang disatukan. Firmware tidak mengubah setting meter. Kedua manual mendokumentasikan baud hingga 38400.
2. Buka **Pencarian meter dan perangkat tersimpan**. Plan awal ACR adalah `0064:1,0047:2`, sedangkan APM `00FC:1,0BB8:2`. Editor plan memakai **heksadesimal**, termasuk alamat yang dikonversi dari manual APM. Jalankan scan terbatas sesuai setting yang diketahui.
3. Periksa dua respons terkonfirmasi dan kandidat Float32 ABCD/CDAB. Cocokkan energi aktif dan reaktif dengan halaman LCD yang sesuai melalui diagnostik di bawah. CRC sah belum membuktikan merek, unit atau urutan word. Bila ACR ternyata Wh/CDAB, atau APM hanya cocok pada 3050, jangan verifikasi profil ini.
4. Isi nama instalasi dan model/revisi, pilih binding profil, lalu catat hasil perbandingan raw/LCD pada catatan atau report. Centang verifikasi hanya jika alamat, urutan word, unit kWh/kvarh dan konversi cocok. Simpan, lalu pilih **Gunakan untuk pengukuran** pada record tersebut. Langkah ini dilakukan operator dari web tanpa mengganti firmware lagi.
5. Overview menampilkan status binding terverifikasi dan mulai menerima energi produksi. Binding tersimpan memakai record CRC yang sudah ada. Mengedit setting koneksi langsung melepas binding, sehingga pilih kembali record yang sudah diverifikasi. Sebelum verifikasi, counter utama dan checkpoint tidak menerima energi kandidat.

Plan universal lama tetap dibatasi delapan probe agar layout bukti persisten tidak berubah. Signature kelima profil tidak muat sekaligus, sehingga profil baru memakai plan khusus di atas. Plan FC03 masih dapat diedit. Finding yang belum cocok tetap disimpan sebagai observasi.

## Perintah diagnostik

Ketik setiap baris pada **web Command input + Run command** setelah masuk diagnostik. Pada INO mandiri, gunakan **Arduino IDE Serial Monitor INPUT, 115200 baud, Newline, lalu Enter/Send** per baris. Perintah ini bukan untuk terminal komputer. Nilai percobaan hanya RAM dan tidak memberikan otorisasi pencatatan produksi.

```text
profile acrel-acr
id 1
baud 9600
parity N
read
reg 0047 2
reg 004B 2
report
resume
```

Untuk APM, ketik `profile acrel-apm` pada input yang sama, samakan link aktual, lalu `read`. Bandingkan `reg 0BB8 2` (desimal 3000) dan `reg 0BEA 2` (desimal 3050) secara eksplisit bila menyelidiki perbedaan manual. `order normal`, `order swap` serta `scale` hanya mengubah percobaan diagnostik. Salin **OUTPUT** yang dihasilkan ke support bila perlu analisis, bukan mengetik label output sebagai perintah.

[Tabel parameter raw tambahan](../ACREL_ACR_APM.md#read-only-diagnostic-comparisons) mencakup energi ekspor, reaktif kapasitif, daya reaktif/apparent serta peta sekunder APM. Parameter tersebut tersedia lewat FC03 raw dan tidak menambah kartu dashboard atau seri checkpoint baru. Unit reaktif sekunder APM masih perlu verifikasi karena tabel mencetak 1WH.

## Validasi dan penyimpanan

PF, frekuensi, kWh aktif dan kvarh reaktif harus lolos sebagai satu snapshot. ID, FC03, panjang tepat serta CRC tetap diperiksa. Nilai harus finite, PF dalam −1,05…1,05, frekuensi dalam guard runtime bersama 40…70 Hz, dan energi nonnegatif tanpa sign bit Float32. Guard runtime bukan klaim rentang operasi vendor. Snapshot gagal tidak mengganti nilai sah sebelumnya dan tidak membuat checkpoint.

Schema checkpoint 3 tetap 446 byte. Slot raw 32-bit menyimpan bit IEEE754 untuk profil Float32, dengan ID profil menentukan dekode ke nilai double kWh. Profil integer mempertahankan perhitungan lama. Record Float32 yang non-finite, negatif atau tidak konsisten ditolak meskipun CRC sah. Pembaca schema 1/2, bank registry serta jadwal sepuluh checkpoint tidak berubah. Pemeriksaan lonjakan memakai selisih energi hasil dekode dan resolusi Float32, bukan selisih bit sebagai jumlah kWh.

Hanya kWh aktif yang disimpan sebagai checkpoint, sedangkan kvarh tersedia langsung. Pengujian software tidak menggantikan pembandingan LCD pada instalasi asli. Integrasi ini tidak mengubah Wi-Fi, LoRa, partisi flash, library Modbus maupun scheduling RTC.

Perintah `primary` juga membaca pasangan energi primer terpilih dan mencetak kedua kandidat urutan word Float32. Nilai Float32 raw APM dalam Wh, sedangkan `read` menerapkan konversi profil menjadi kWh.

Gunakan sedikitnya dua pembacaan nonnol yang berubah mengikuti konsumsi. Counter nol atau pasangan word yang tidak berubah/simetris belum membuktikan urutan word maupun konversi unit.

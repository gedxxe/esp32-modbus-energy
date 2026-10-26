# Diagnostik pada firmware utama

[English](../DIAGNOSTICS.md) · **Bahasa Indonesia** · [Ringkasan](../../README.id.md)

Firmware **1.4.0** menyediakan kemampuan diagnostik INO di aplikasi utama. Tidak perlu mengganti firmware dengan sketch terpisah. AP, dashboard, tombol dan RGB tetap berjalan; UART tetap dimiliki core 1.

## Langkah awal di panel

1. Upload firmware utama `esp32s3`. HP ke **Midea Engineering AP**, password **betahdijre12345**, buka **http://192.168.4.1**. Pastikan footer Firmware 1.4.0.
2. Buka **Koneksi meter → Konsol diagnostik**. Kolom **Input perintah** adalah tempat mengetik perintah web, bukan chat.
3. Pilih **Tes bertahap**. Ini mengisi `diagnose`, belum menjalankan. Klik **Jalankan perintah**.
4. Tunggu `[REPORT_END]` pada **Output diagnostik perangkat**. Default tanpa respons biasanya sekitar 10 detik. Tes mendengarkan tanpa TX selama 3 detik, mencoba probe komunikasi, tetap menjalankan daftar raw register meski probe pertama diam, lalu mencoba energi/rasio terkonfigurasi. Tidak otomatis scan seluruh ID/format.
5. Klik **Unduh output**, simpan TXT di HP. Jika perlu analisis, kirim file/teks OUTPUT itu ke chat. Jangan tempel OUTPUT ke Input perintah.
6. Selesai tes, klik **Lanjutkan pengukuran**. Peringatan maintenance harus hilang dan polling normal kembali. Nilai uji tidak pernah masuk pencatatan interval produksi.

Sampling RTC/interval juga dijeda saat tes aktif; waktu pada dashboard adalah sampel RTC terakhir sampai resume.

Tes UART aktif memasuki maintenance dan memutus kontinuitas interval. Interval terputus menjadi parsial, interval selesai tetap tersimpan. Menutup panel/browser tidak membatalkan tes atau melanjutkan pengukuran. Buka kembali dashboard dan Lanjutkan pengukuran. Reboot memulihkan setting produksi tersimpan dan menghapus RAM diagnostik.

`status`, `help`, `report`, `compare` dan `plan show` dapat membaca hasil uji tersimpan tanpa memasuki maintenance. Status saat tidak aktif juga mencetak setting produksi terpisah. Sesi baru menyalin default engine dan ID/format tersimpan; override/capture hanya milik sesi itu. Perintah LED biasa dapat menunggu tes panjang selesai. Perubahan koneksi/waktu produksi serta scan produksi diblokir selama maintenance.

## Pilih tes terbatas

Ketik **satu perintah di Input perintah web**, klik Jalankan perintah, tunggu output. Perintah yang sama tersedia melalui Serial sesudah masuk mode diagnostik.

| Kondisi | Perintah / tindakan | Makna |
| --- | --- | --- |
| Bukti awal | `diagnose` | ID/format saat ini, beberapa query independen, bukan pencarian seluruh setting |
| RX pasif | `listen 5` | Tanpa request Modbus selama 5 detik; slave tanpa request memang dapat diam |
| Satu alamat | `probe 0080 1`, lalu `intermediate` | HEX/word; data atau exception CRC sah menjadi bukti balasan |
| Word mentah | `reg 0082 2` / `deeper` | Data gagal tidak menjadi nol; raw terpisah dari validasi kWh |
| CRC/echo/frame pendek | `status`, baca HEX, scan format terbatas | Counter saja bukan bukti Fort membalas |
| Input TTL diragukan | `rxcheck` | Observasi pull lemah, bukan deteksi pasti modul/tegangan/continuity |
| Dua keadaan A/B | `check connected`; ubah fisik hanya A/B di modul; `check isolated`; `compare` | Label operator, konfigurasi sama dan tes selesai; jangan jalankan keduanya tanpa perubahan fisik |
| Bukti sebelumnya | `report` | Tanpa request baru; counter uji terpisah dari produksi |
| Hentikan | `x` / Hentikan tes | Membatalkan wait/scan/poll/antrean tes, maintenance masih aktif |
| Selesai | `resume` / Lanjutkan pengukuran | Pulihkan UART/pin produksi tersimpan dan akuisisi normal |

[Delapan resep](../../examples/FortSerialDebug/README.id.md#pilih-pengujian-berdasarkan-hasil) juga berlaku di Input perintah web, satu baris non-komentar setiap klik. Baris `#`/path file bukan perintah web; paste multi-baris ditolak. Tidak ada write register atau broadcast.

Setting runtime mencakup `id`, `baud`, `parity`, pin terjaga `pins RX TX`, `timeout`, `interval`, `probe`, raw `plan`, `range`/`scanmode`/`bauds`/`parities`, serta diagnostik `energy`/`ratios`/`scale`/`order`/`decode`. Alamat adalah HEX PDU tanpa translasi 40001/minus1, count adalah word 16-bit. Map energi produksi tidak berubah; override hanya RAM, tidak mengubah profil produksi.

Panjang energi pada manual scan masih tidak konsisten. Resep 05 membandingkan raw 1/2/3 word tanpa menganggap word tambahan sebagai energi. Float primer berbeda dari integer sekunder. Jangan memilih scale/order karena angkanya tampak masuk akal; bandingkan display yang sesuai.

## Cari koneksi tanpa memulai scan satu jam

Pada **Koneksi meter → Cari meter Fort**, mulai dari ID yang diketahui/rentang kecil dan **Baud/parity tersimpan saat ini**. Hanya satu probe per ID. ID 1–16 tanpa respons sekitar 13 detik pada 800 ms; seluruh 247 ID sekitar 3–4 menit dengan overhead. Setting menu meter lebih kuat daripada default.

Bila diperlukan pilih **Seluruh 18 format baud/parity**. Matriks1–247 memiliki 4.446 kandidat dan bisa sekitar 70 menit tanpa respons. Pembatalan diproses setelah satu transaksi 800 ms aktif. Jangan memulai matriks penuh hanya karena satu pembacaan gagal.

Alamat/count probe bisa diedit, default 0x0034/1 word. Slave dapat mengabaikan register yang tidak didukung; gunakan tes bertahap beberapa alamat atau probe lain berdasarkan manual. Data dan exception CRC sah sama-sama dapat menghentikan pencarian. Koneksi ditemukan belum membuktikan model/profil energi. Terapkan hasil hanya menyimpan serial pembaca; validasi energi/rasio tetap wajib. Echo persis tidak menghentikan pencarian secara prematur, karena modul dapat meng-echo request pada setting yang tidak dijawab meter.

Untuk daftar custom, masuk maintenance lalu gunakan `range 1 16`, `scanmode current`, `scan`; ubah parameter bertahap. Daftar baud/parity matrix dapat dikonfigurasi, `x` diperiksa selama wait RX. Batalkan scan produksi sebelum tes konsol; race perintah ditolak eksplisit di pemilik UART.

## Arti bukti

`TRANSPORT_DATA_CONFIRMED`: ID/fungsi/count/panjang/CRC data sesuai. `TRANSPORT_EXCEPTION_ONLY`: exception sah, register belum dibaca. `PROFILE_DECODED_UNVERIFIED`: konversi terkonfigurasi berhasil, masih perlu perbandingan display. `NO_RX_UNRESOLVED`, `RX_UNVALIDATED`, `ECHO_WITHOUT_REPLY` mempertahankan ketidakpastian, bukan vonis komponen rusak.

Echo satu request utuh ditolak, lalu menunggu respons sah terpisah dalam deadline awal. Prefix request dengan byte tambahan dicatat `TX_PREFIX_WITH_TRAILING_RX`, tidak dipotong/diterima sembarang. Byte sebelum TX dibuang dengan prefix/count tercatat, bukan respons request baru. Frame invalid/terlalu panjang/tail deadline tidak menjadi energi. Counter frame rusak/pendek/ditolak tetap bertambah meski respons sah berikutnya berhasil dalam budget request yang sama.

Modul empat pin tidak memberi telemetry presence/power-good. Modul dicabut/mati/rusak, RX terbuka, setting salah dan jalur diam lain bisa identik. Uji bias lemah/perbandingan keadaan mempersempit observasi tanpa membuktikan komponen rusak tertentu. Keduanya diam tetap perlu ukur suplai/TTL atau link pembanding sehat.

## Aliran komunikasi dan monitor proses

Panel gelap mengambil **event UART baru saja** setiap 200 ms: TX masuk antrean/selesai, RX pertama diamati, frame selesai/ditolak, tail deadline, timeout tanpa byte, RX sebelum TX, error driver dan RX pasif. Timestamp mikrodetik dan pin uji sebenarnya ikut dicatat. Ring 32 event menandai kehilangan; history awal tidak diputar seolah traffic baru. Tanpa event baru tidak ada paket bergerak. Reduced motion menonaktifkan gerakan.

Browser mereplay jeda timestamp teramati 1× dengan polling sekitar 200 ms ditambah jaringan. Posisi paket dan afterglow 250 ms adalah skematik, bukan pengukuran propagasi A/B atau bukti Fort menerima request. `micros()` berputar sekitar 71 menit; selisih memakai unsigned. HEX/nomor urut tetap dapat diperiksa tanpa animasi.

Buka **Perangkat & arsip → Monitor proses**: sampel task non-idle dua core, heap bebas/minimum/blok terbesar, PSRAM, state/prioritas/high-water stack task milik aplikasi/idle. Hook tick bekerja terbatas di IRAM tanpa task sampler tambahan. Persentase adalah estimasi batas tick sekitar 1 detik, bukan waktu instruksi; interrupt teratribusi ke task yang diinterupsi. Tabel hanya task aplikasi/idle, jumlah sistem juga meliputi Wi-Fi/USB. Stack adalah minimum byte bebas historis, bukan penggunaan saat ini. Refresh 1,5 detik hanya ketika panel terbuka.

## Alternatif Serial Monitor pada firmware utama

Buka **Serial Monitor Arduino IDE/PlatformIO,115200 baud, Newline**. **Ketik `debug` di INPUT Serial Monitor, Enter** untuk masuk maintenance. Lalu `status`, Enter, `diagnose`, Enter. Salin OUTPUT bergulir ke chat bila meminta analisis. `x`, Enter menghentikan tes; **`resume`, Enter** memulihkan pengukuran. Output USB dikirim dalam potongan terbatas tanpa memblokir UART. Log web menyimpan 8 KiB terbaru dan melaporkan byte terbuang. Simpan log antar-tes; restart menghapusnya.

## Implementasi dan batas

![Kepemilikan diagnostik dan event](../assets/diagnostic-flow.svg)

`tools/build-diagnostic.py` menghasilkan class C++ committed dari INO mandiri. Compiler utama memakai `firmware/include/diagnostic_engine.h`, bukan include file contoh. Polling normal dan tes memakai transport yang sama. PlatformIO memverifikasi provenance tanpa Node. Tes host menjalankan engine produksi termasuk seluruh resep dan kasus CRC/ID/fungsi/count/echo/exception. Tes browser memakai byte gzip asli dengan API simulasi. Lihat [validasi](VALIDATION.md); tes tersebut bukan penerimaan meter fisik.
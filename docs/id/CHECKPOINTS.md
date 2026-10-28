# Checkpoint kWh terjadwal

[English](../CHECKPOINTS.md) · **Bahasa Indonesia**

## Data yang disimpan

Rekaman utama adalah angka kWh impor kumulatif pada checkpoint, bukan konsumsi sejak checkpoint sebelumnya. Jika 07.00 mencatat 128.400,000 kWh dan 07.30 mencatat 128.408,000 kWh, kedua angka tetap tersedia ketika petugas datang pukul 09.20. Selisih opsionalnya 8,000 kWh, sedangkan angka kumulatif asli tidak berubah.

Penyimpanan berada di ESP32. Firmware tidak menambahkan memori ke Fort maupun menulis register meter. Logger perlu tetap menyala, mempunyai waktu DS3231 yang valid dan menerima pembacaan meter yang sah saat checkpoint berlangsung. Pencatatan tidak memerlukan router, HP tersambung maupun halaman web terbuka.

## Jadwal harian

Tanggal arsip serta jadwal memakai WIB, UTC+7. DS3231 menyimpan UTC.

| Pagi | Sore |
| --- | --- |
| 06.00 | 17.00 |
| 06.30 | 17.30 |
| 07.00 | 18.00 |
| 07.30 | 18.30 |
| 08.00 | 19.00 |

![Jadwal checkpoint dan penyimpanan lokal](../assets/checkpoint-schedule.svg)

Jadwal berada pada `CAPTURE_MINUTES` di [checkpoint_log.h](../../firmware/include/checkpoint_log.h), dengan schema terpisah dari sembilan interval lama. Rilis ini tidak mengubah batas interval hasil interpolasi menjadi checkpoint mentah. Pencatatan baru dimulai ketika firmware baru berjalan, sehingga waktu yang sudah terlewat sebelum pemasangan tidak dapat direkonstruksi.

## Mengambil rekaman setelah jadwal lewat

1. Hubungkan HP ke **Midea Engineering AP**, password **betahdijre12345**, lalu buka **http://192.168.4.1**. Alamat lokal client juga dapat dipakai pada mode client atau gabungan.
2. Buka **Ringkasan → Checkpoint kWh terjadwal** dan periksa tanggal rekamannya. Pilih Pagi atau Sore untuk melihat lima baris. Kolom angka meter berisi kWh kumulatif, sedangkan Dibaca pada menunjukkan waktu respons aktual dalam WIB.
3. Pilih **Unduh CSV checkpoint** untuk menyimpan hari yang ditampilkan. Untuk hari sebelumnya, buka **Rekaman checkpoint sebelumnya**, pilih tanggal lalu unduh CSV. Rentang retensi adalah hari tersimpan terbaru beserta hari-hari yang tersedia sebelumnya, sebanyak 31 hari kalender.
4. Aktifkan **Tampilkan selisih antar-checkpoint** jika membutuhkan konsumsi antar-rekaman. Tampilan awal tetap berupa angka meter asli. Pada OLED, PREV/NEXT memilih halaman capture pagi atau sore, sedangkan OK singkat berpindah di antara lima rekaman beserta tanggalnya.

Baris Tercatat tidak ditimpa lagi pada hari itu, dan menyimpan waktu aktual serta keterlambatan pembacaan. Menunggu berarti checkpoint belum difinalisasi. Terlewat berarti tidak ada pembacaan sah dalam jendela penerimaan. Badge penyimpanan mengacu pada hari rekaman, sehingga tanggal perlu diperhatikan ketika penyimpanan sedang melaporkan backlog.

![Contoh tabel checkpoint dengan data simulasi](../assets/ui-checkpoints-desktop.png)

## Timing dan penerimaan

Akuisisi normal menargetkan dua detik. Dari dua detik sebelum checkpoint yang belum terisi sampai lima detik setelahnya, task akuisisi memprioritaskan kWh dibanding pembacaan daya serta nilai pembanding, dan menunda refresh rasio jika rasio yang ada masih valid. Retry checkpoint menargetkan 250 ms dengan tetap mengikuti jarak request dan timeout terbatas pada transport bersama. Backoff handshake tidak menghalangi pembacaan energi langsung pada periode prioritas tersebut.

Checkpoint menerima pembacaan sah pertama dengan bracket RTC yang dimulai pada atau setelah jadwal, lalu selesai paling lambat lima detik sesudah jadwal. Urutan waktu harus benar, rentang bracket maksimal dua detik RTC serta durasi monotonic maksimal 2500 ms. Nilai berasal dari counter sekunder UINT32 yang dikonversi memakai PT numerator/denominator dan CT tervalidasi sekali. Input tersebut ikut disimpan bersama nilai kumulatif `double`.

Pembacaan DS3231 beresolusi satu detik dan meter memperbarui counternya secara internal. Label 07.00 tidak mengklaim sampling atomik tepat 07:00:00.000. Kedua timestamp bracket serta keterlambatan respons disimpan agar observasi aktual terlihat. Checkpoint mentah tidak diinterpolasi, tidak diisi nol ketika gagal, dan tidak diisi belakangan memakai angka di luar jendelanya.

RTC invalid, daya terputus, pembacaan tidak tersedia atau maintenance aktif dapat membuat checkpoint terlewat. Data uji diagnostik tidak masuk rekaman produksi. Koreksi jam tidak menimpa baris yang sudah tercatat maupun terlewat, dan tanggal yang mendahului rekaman tersimpan ditolak.

## Selisih merupakan data turunan

Ada empat pasangan berurutan dalam setiap shift, sehingga tersedia delapan kemungkinan selisih sehari. Tidak ada pengurangan dari 08.00 ke 17.00. Kedua endpoint harus tercatat, mempunyai metadata meter/serial/rasio yang sama serta generasi pencatatan kontinu yang sama. Reboot, reset yang terdeteksi, perubahan setting maupun gap sampel berlebih membuat selisih yang melintasi gangguan tidak tersedia tanpa menghapus angka mentah.

Selisih yang sah mewakili perubahan di antara waktu pembacaan aktual, yang dapat sedikit berbeda dari label jadwal. Selisih yang tidak tersedia dibiarkan kosong pada CSV dan ditampilkan sebagai tanda pisah di UI. Sembilan interval lama tetap mempunyai aturan tersendiri dan dapat dibuka melalui **Jadwal interval sebelumnya dan analisis** atau diekspor lewat `/api/export`.

## Penyimpanan dan pemulihan

Core 1 menjadi pemilik tunggal mutasi rekaman serta Preferences. Setiap hari dikodekan menjadi 426 byte dengan schema little-endian eksplisit, penanda jadwal, nomor hari/revisi/generasi, sepuluh baris berukuran tetap dan CRC16. Format tidak bergantung pada padding struktur C++. Setiap rekaman menyimpan waktu UTC bracket, counter raw, kWh kumulatif, metadata serial, rasio serta generasi kontinuitas. Decoder memeriksa CRC, schema, kalender, batas waktu dan kesesuaian angka dengan sumbernya.

Record yang berubah segera ditulis ke key NVS `captures` serta `/c<day>.bin` pada LittleFS. File sementara yang sudah di-flush diperiksa sebelum dipasang, dengan kebijakan rename yang mempertahankan backup saat operasi gagal. Saat boot, pemulihan membandingkan record sah dari NVS, arsip, backup, file sementara serta hari sebelumnya. Data rusak tidak memicu format otomatis.

Jika filesystem gagal mengarsipkan satu hari, satu hari sebelumnya dapat dilindungi lewat `capture_prev` sementara hari berjalan memakai `captures`. Pergantian berikutnya diblokir bila akan menimpa hari lama yang belum berhasil diarsipkan. UI menampilkan kebutuhan pemeriksaan storage dan mempertahankan data yang tersedia untuk diunduh. Storage gagal dicoba kembali, sementara record NVS yang sudah tersimpan identik tidak ditulis ulang pada setiap retry. Arsip yang berhasil mengikuti rentang retensi 31 hari.

Upgrade mempertahankan key NVS `day` serta `/d<day>.csv` milik interval lama. Konfirmasi format fisik setelah mount gagal menghapus arsip filesystem, termasuk hari checkpoint, sehingga unduh data yang masih tersedia terlebih dahulu. Perilaku putus daya NVS serta filesystem masih perlu diterima pada perangkat melalui [COMMISSIONING](COMMISSIONING.md).

## API dan CSV

`/api/status` membawa proyeksi `checkpoints` hari tersimpan terbaru. Endpoint baca `/api/checkpoints?day=<day>` mengembalikan satu hari, dan tambahan `&format=csv` mengunduhnya. `/api/history?kind=checkpoints` mencantumkan hari tersedia, termasuk record NVS saat filesystem gagal. Endpoint history/export lama mempertahankan format intervalnya.

Kolom CSV meliputi tanggal, shift, jadwal WIB, waktu bracket aktual, keterlambatan respons, kWh kumulatif, counter raw sekunder, PT numerator/denominator, CT, slave ID, baud/parity, status serta selisih opsional dari checkpoint sebelumnya. Kolom kosong berarti data tidak tersedia, bukan nol. Nama file diawali `fort-checkpoints-` agar mudah dibedakan dari CSV interval lama.


Schema 2 tetap berukuran 426 byte dan menyimpan profil meter asal pada byte 38 setiap baris. Schema 1 tetap dibaca sebagai Fort. Validasi energi memakai skala profil dan rasio yang tersimpan, sedangkan selisih tidak melintasi pergantian model. Tabel dan CSV mempertahankan identitas model asal tiap baris. Baca [Profil meter](METER_PROFILES.md) untuk migrasi serta konversi.

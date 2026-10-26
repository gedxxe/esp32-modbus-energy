[English](../AUDIT.md) · **Bahasa Indonesia**

# Audit total — 22 Oktober 2026

Audit mencakup source firmware, tracker, CRC, konfigurasi board/dependency, persistence, antarmuka, wiring terhadap manual, dokumentasi, aset GitHub, tooling, dan isi paket rilis. Pemeriksaan source/simulasi berbeda dari pengujian perangkat; hasil hardware belum diklaim.



## Review 1.3.0

Capture mengamati traffic yang sudah berjalan melalui buffer tetap dan queue snapshot. CRC/frame tetap ketat; echo persis tidak menentukan lokasi sumbernya. Polling trace hanya saat panel dibuka. Sketch Arduino terpisah memakai fungsi baca 03, batas buffer/deadline dan tanpa storage persisten. Perlakuan tunggu echo pada sketch tidak mengubah reader utama. Timing/stack/perilaku modul masih membutuhkan uji hardware.

## Review 1.2.1

Rotasi OLED diatur sebelum boot/prompt. RMT asinkron dimiliki core 0; NVS LED ber-CRC dimiliki core 1. Status simpan LED terpisah dari storage energi. Form hanya mengirim saat Terapkan dan draft tetap aman saat polling. Variasi GPIO serta batas tiga kanal dijelaskan; hasil mock tidak dianggap bukti LED/rotasi fisik.

## Temuan yang diperbaiki pada 1.0.1

| Temuan | Dampak sebelumnya | Perbaikan dan verifikasi |
| --- | --- | --- |
| Tidak ada `AGENTS.md` | Aturan hardware, sumber kebenaran dan verifikasi belum terdokumentasi | Tambah panduan root, invariants firmware, scope, pin/register, visual dan aturan rilis |
| README tanpa visual yang ditautkan | Screenshot yang dibuat tidak terlihat di GitHub | Tambah preview web desktop/HP, OLED SVG, diagram menyeluruh dan diagram kontak tombol |
| Wiring belum memberi terminal Fort persis | Instalasi harus menebak sambungan A/B | Baca scan PDF 7/Tabel 3 dan 9/Gambar 5: A→58, B→59; bedakan 70/COM switch input |
| Tutorial belum menjelaskan setiap kabel | Rail, shared I2C, kontak switch dan driver buzzer kurang konkret | Tambah langkah per kabel dan netlist CSV dengan 3V3/GND bersama |
| Versi dependency hanya ada di konfigurasi | Pembaca README tidak bisa melihat stack dan versinya | Badge SVG lokal dan tabel dari versi yang dipakai; generator/checker memeriksa konsistensi |
| Restorasi backup gagal tetapi kode masih dapat menghapus backup | Salinan yang masih dapat dipulihkan berisiko hilang | Helper `installArchive` berhenti ketika restore gagal; fault-injection test memakai helper yang sama dengan firmware |
| Retry rasio CT/PT dapat berulang tiap loop setelah masa refresh | Bus terlalu sering mencoba membaca rasio saat gagal | Jeda minimal lima detik antarpencobaan; tes deadline dan rollover `millis()` |
| Watchdog hanya di-feed sekali per siklus beberapa transaksi | Siklus retry panjang dapat mendekati batas watchdog | Feed pada setiap percobaan Modbus; RX overflow dan stale drain dibatasi |
| Versi web dan paket ditulis terpisah | Metadata bisa berbeda setelah perubahan firmware | `version.h` sebagai sumber versi; API menyediakan versi; generator dan packager membacanya |
| Paket awal tidak memasukkan aset docs karena seleksi file terlalu sempit | README visual tidak akan lengkap di ZIP | Packager mencakup docs/aset, root guides, workflow, netlist dan tool konfigurasi |
| Belum ada audit link dan pipeline repository | Nama/path salah berpotensi lolos saat dipush | Tambah checker case-sensitive, anchor, SVG, versi, pin/netlist dan workflow build/test |

## Hal yang diaudit dan dipertahankan

- ESP32 satu-satunya master pada port Fort yang kosong; kompatibilitas TTL 3,3 V modul merupakan fakta yang diberikan pengguna.
- Pin sesuai [board.h](../../firmware/include/board.h), bukan urutan fisik header dari board yang belum difoto.
- Firmware hanya memakai fungsi Modbus 03. Default baud/parity berasal dari manual; slave 1 adalah nilai awal contoh, dapat diubah lewat web.
- Counter sekunder, scaling CT/PT sekali, perhitungan `double`, dan kualitas data gap/reboot tetap diuji.
- CRC contoh query manual benar; CRC respons tercetak tidak cocok dan tetap ditolak oleh firmware.
- RTC disimpan UTC, interval memakai WIB; waktu compile tidak menjadi pengganti RTC invalid.
- Queue memisahkan snapshot/perintah antartask; I2C dan filesystem dilindungi mutex; UART/Preferences dimiliki core akuisisi.
- Web tidak menggunakan CDN; format filesystem tetap memerlukan prosedur fisik; interval parsial tidak diisi nol.

## Batas yang tetap ada

1. Tes native tidak menjalankan UART/I2C/RTOS atau power-cut flash fisik. Boot, suplai, komponen, ketelitian meter dan ketahanan noise memerlukan [commissioning](COMMISSIONING.md).
2. Timestamp per byte UART tidak tersedia dari polling Arduino ini; firmware memeriksa CRC/struktur dan silent gap, bukan t1.5 per byte secara presisi.
3. Interpolasi batas maksimal 15 detik adalah estimasi. Resolusi nyata tergantung counter dan rasio CT/PT, bukan jumlah angka desimal CSV.
4. Checkpoint NVS memakai CRC dan commit NVS, tanpa journal aplikasi ganda atau rekonstruksi state dari CSV. Kerusakan checkpoint/flash dapat menghilangkan data sejak penyimpanan valid terakhir. Backup file melindungi jalur penggantian normal, bukan jaminan terhadap semua kerusakan fisik.
5. RTC battery backup menjaga jam, bukan pembacaan saat ESP32 mati. Counter rollover/reset diperlakukan sebagai putus kontinuitas.
6. HTTP lokal memakai akses AP dan token sesi, tanpa login/TLS terpisah. Tidak ada OTA/cloud/captive portal pada versi ini.
7. Hasil CI remote dicatat terpisah dari tes lokal; lihat workflow dan laporan VALIDATION untuk commit yang diperiksa.
8. Sesudah audit awal, pemilik memilih GPLv3 (`GPL-3.0-only`); LICENSE, NOTICE dan badge telah ditambahkan. Manual Fort/dependency tetap mengikuti hak asalnya. Tidak ada klaim hardware-tested.

Hasil pemeriksaan yang benar-benar dijalankan dan ukuran build tercatat di [VALIDATION](VALIDATION.md).

## Review 1.2.0

Dashboard diganti menjadi source React yang dapat dipelihara, dengan React Bits asli, katalog English/Indonesia dan aset gzip lokal. Guard build memeriksa source/header agar UI lama tidak ikut compile tanpa disadari. Penjelasan wiring, boot, core scheduling, queue/mutex dan timing RTU ditulis ulang pada dokumen utama English; README Indonesia juga diperbarui. Register, skala, pin, layout NVS dan prosedur format fisik dipertahankan. Komponen pihak ketiga dicatat pada NOTICE dengan lisensinya sendiri.

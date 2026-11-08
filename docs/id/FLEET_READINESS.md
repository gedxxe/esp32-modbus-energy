# Deployment banyak unit dan kesiapan firmware

[English](../FLEET_READINESS.md) · **Bahasa Indonesia** · [Validasi](VALIDATION.md) · [Gangguan daya](POWER_LOSS.md) · [Audit software satu tahun](YEAR_SOFTWARE_READINESS.md)

Versi 3.5.0 menjadi kandidat deployment terkendali pada ESP32-S3 N16R8 dengan pinout yang didokumentasikan. Pengujian software membuktikan perilaku dan aturan pemulihan tertentu, tetapi belum membuktikan kestabilan regulator, kapasitas RF, akurasi meter atau tingkat kegagalan sepanjang umur perangkat. Lakukan pilot kecil sebelum menyatakan satu image diterima untuk seluruh instalasi.

## Temuan dan perbaikan

Staging yang lebih lama sebelumnya dapat mengganti arsip valid yang lebih baru. Promosi sekarang membandingkan versi dan checkpoint immutable. Staging hanya dihapus jika salinan existing yang tervalidasi sudah mencakupnya. Dua record yang bertentangan dipertahankan dan promosinya dihentikan.

Konfigurasi meter dan Wi-Fi sekarang memakai bank bergantian dengan CRC, generation dan readback persis. Write yang gagal tidak mengganti konfigurasi meter aktif yang sebelumnya sudah terverifikasi. Status bank bermasalah tampil pada dashboard. Unit baru menunggu operator menyimpan pengaturan meter sebelum produksi, sementara web, OLED, discovery dan diagnostik tetap tersedia. Pengaturan existing yang valid dimigrasikan.

Identitas unit, nama AP dan hostname memakai factory MAC lengkap. Event Wi-Fi yang tiba melewati deadline tidak diterima, dan RTC yang kembali valid tidak terus mengulang persiapan koneksi yang masih aktif. Uptime serta masa tenggang buzzer memakai timer ESP 64-bit agar tidak kembali nol pada rollover millis sekitar 49 hari. Umur sample, status kontak dan flags telemetry radio disusun dari publikasi core 1 yang sama.

Mapping meter, profile ID, schema checkpoint 3, protokol Energy Station v1, pinout dan offset partisi tetap mengikuti kontrak existing. Gateway terpisah masih harus mengimplementasikan serta menguji kontrak radio tersebut.

## Identitas setiap unit

Nama AP default menjadi `Midea Engineering-XXXXXXXXXXXX`, dengan dua belas karakter heksadesimal dari factory MAC sebagai Device ID. Hostname adalah `energy-xxxxxxxxxxxx`. Nilainya diturunkan saat boot, bukan disalin dari NVS. Password AP mengikuti konfigurasi `board.h`.

Baca nama lengkap pada halaman Network OLED atau menu AP password. Cocokkan Device ID pada **Perangkat & arsip → Unit ini** dengan inventori commissioning. Dashboard menampilkan generation konfigurasi meter/Wi-Fi dan peringatan bank invalid. ID merupakan metadata publik instalasi, bukan secret autentikasi. Bila identitas factory gagal dibaca, status unavailable dan nama AP fallback ditampilkan. Unit tersebut belum memenuhi penerimaan commissioning.

`AP_UNIQUE_NAME=false` mempertahankan nama lama `Midea Engineering AP` untuk instalasi satu unit yang memang memerlukannya. Pertahankan default aktif untuk fleet. Opsi tersebut tidak mengubah slave ID Modbus atau Node ID LoRa.

## Commissioning pertama dan pembaruan

Gunakan factory image saat menyiapkan unit kosong. Untuk logger terpasang, gunakan PlatformIO Upload atau app pada `0x10000` dengan layout partisi yang sama. Factory image gabungan mencakup padding NVS dan bukan jalur update normal untuk mempertahankan pengaturan. Ikuti [panduan flashing](FLASHING.md).

Unit kosong mungkin memerlukan inisialisasi LittleFS karena factory image tidak membawa filesystem terinisialisasi. Ikuti konfirmasi fisik OK saat boot dan tahan lima detik hanya bila filesystem memang boleh dihapus. Prosedur tersebut menghapus arsip filesystem pada unit existing dan tidak dijalankan otomatis.

Pada unit baru, hubungkan ke nama AP yang ditampilkan lalu buka alamatnya, biasanya `http://192.168.4.1`. Di **Pengaturan meter**, pilih model yang terpasang, samakan slave ID/baud/parity serta manual direction, kemudian simpan. Default Fort yang sebelumnya implisit tidak lagi memberi izin produksi. ACR/APM tetap membutuhkan binding perangkat tersimpan yang telah diverifikasi. Cocokkan kWh kumulatif dengan LCD meter, periksa RTC/UTC dan tampilan WIB, lalu pastikan status penyimpanan sebelum meninggalkan panel.

Setiap station radio harus memiliki Node ID dan pairing key sendiri yang cocok dengan inventori gateway. Network ID dan Gateway ID boleh sama untuk jaringan/receiver yang dituju. Jangan mengaktifkan banyak station dengan Node ID default 2. Device ID tidak ditambahkan ke wire format v1 dan tidak otomatis mengalokasikan alamat radio.

Jangan menggandakan seluruh flash dump dari unit yang sudah dikomisioning ke unit lain. Dump dapat membawa kredensial Wi-Fi, ID/key radio, inventori meter dan riwayat pembacaan. Pakai image hasil build lalu komisioning setiap unit secara terpisah. Jangan memasukkan kredensial ke worksheet publik.

Catat Device ID, aset/lokasi, revisi board, versi dan hash image, model/profile meter, pengaturan serial, ID radio bila dipakai, nilai pembanding LCD, pemeriksaan jam/storage, operator/tanggal dan hasil penerimaan. Identitas sumber arsip lama harus tetap dipertahankan.

## Format pemulihan konfigurasi

Payload meter 16 byte disimpan dalam envelope 30 byte pada `config_a` dan `config_b`. Payload Wi-Fi versi 3 tetap 779 byte dalam envelope 793 byte pada `wifi_a` dan `wifi_b`. Envelope berisi magic `SBK1`, versi 1, byte reserved nol, panjang payload eksplisit, generation 32-bit bukan nol, payload dan CRC16. Format tidak bergantung pada padding struct.

Core 1 merupakan satu-satunya owner Preferences tersebut. Save menulis bank tidak aktif, membandingkan readback persis, lalu menerapkan konfigurasi baru. Save meter yang gagal mempertahankan produksi dengan pengaturan terverifikasi sebelumnya. Save pertama memiliki satu bank terkonfirmasi, sedangkan save berikutnya mempertahankan versi sebelumnya. Ini melindungi update yang terputus, bukan menjamin setiap record awal langsung memiliki dua salinan identik.

Record legacy `config` dan `wifi` 104/108/779 byte hanya diimpor bila belum ada bank baru. Key lama dipertahankan, tetapi setelah bank ada, journal rusak tidak boleh diam-diam memuat nilai legacy yang mungkin kedaluwarsa. Bila kedua bank gagal, produksi meter dijeda atau Wi-Fi memakai default AP pemulihan. Migrasi awal yang terputus dapat memerlukan konfirmasi ulang operator. Bank dengan generation sama tetapi isi berbeda juga memerlukan pemeriksaan. Bank lama yang valid dapat memulihkan layanan, dengan peringatan salinan alternatif invalid sampai diperbaiki oleh save yang terverifikasi.

Firmware lama tidak membaca key bank baru dan key legacy tidak lagi diperbarui. Downgrade bisa memakai konfigurasi legacy lama, sehingga ekspor arsip dan konfirmasi ulang seluruh pengaturan saat sengaja melakukan downgrade. Partisi NVS tetap 20 KiB. Kapasitasnya terbatas dan kegagalan save ditampilkan, bukan ditangani dengan format atau perubahan offset partisi.

## Penerimaan pilot

Mulai dari satu unit bench, kemudian kelompok kecil. Uji boot tanpa USB Serial, OLED/RTC/radio tidak terpasang, meter dilepas/disambung, timeout/CRC, login Wi-Fi benar/salah, DHCP gagal, storage terganggu dan pemulihannya. Ulangi pemutusan daya saat save dan di sekitar checkpoint memakai unit test khusus serta ekspor arsip. Reboot harus memuat versi tersimpan yang lengkap dan valid atau menyatakan unavailable, tanpa menciptakan checkpoint.

Jalankan soak 24–72 jam dengan trafik browser, reconnect Wi-Fi dan pekerjaan radio yang representatif. Pantau minimum heap, blok bebas terbesar, stack watermark task, watchdog/reset reason dan status storage. Durasi ini merupakan usulan prosedur penerimaan, belum hasil pengujian yang telah dilakukan. Pengujian listrik/endurance lebih lama mengikuti kebutuhan keandalan instalasi.

Untuk LoRa, uji satu pasangan gateway/node dahulu, kemudian tiga node dengan alamat/key terpisah. Periksa duplikasi ID, kehilangan paket/retry, transfer checkpoint dan restart receiver. Satu SX1262 dengan pacing 1% per radio tidak mendukung jumlah node tak terbatas pada interval default. Gunakan [perencanaan airtime PTMP](LORA_PTMP.md) dan catat penerimaan gateway/RF secara terpisah.

Catu daya, baterai RTC, instalasi RS485 serta suplai/antena radio tetap menentukan keandalan sistem lengkap. NVS dan LittleFS berada pada chip flash yang sama. CRC, retry dan ACK tidak memulihkan pembacaan jika seluruh salinan tersimpan hilang.

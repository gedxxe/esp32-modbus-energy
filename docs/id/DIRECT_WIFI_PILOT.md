# Uji coba industri: ESP32-S3 langsung ke website melalui Wi-Fi

[English](../DIRECT_WIFI_PILOT.md) · **Bahasa Indonesia** · [Teknis HTTPS](../IOT_HTTPS.md) · [Pemulihan listrik padam](POWER_LOSS.md)

Panduan ini untuk **pilot terawasi**, bukan bukti perangkat siap beroperasi tanpa pengawasan. ESP32-S3 membaca power meter melalui Modbus RTU/RS485 lalu mengirim data HTTPS langsung ke **website development** melalui Wi-Fi 2,4 GHz yang diizinkan. **Tidak diperlukan ESP32-C3 gateway atau LoRa**; biarkan LoRa nonaktif. Seluruh pembacaan dan checkpoint lokal tetap berjalan ketika Wi-Fi tidak tersambung.

## Persiapan unit yang sudah terpasang

1. Ekspor seluruh checkpoint kWh yang tersedia. Catat nilai LCD meter, profil Modbus, ID perangkat, CT/PT, firmware dan pengaturan jaringan yang sudah berfungsi. Jangan gunakan merged factory image atau hapus partisi untuk memperbarui unit yang sudah dipakai.
2. Daftarkan sumber/meter unik di **website development**. Kunci profil harus sama persis dengan profil yang tersimpan pada firmware. Unduh konfigurasi resmi `iot_credentials.local.h` ke `firmware/include/`, jangan masukkan ke Git, chat, tangkapan layar atau log. Build aplikasi ESP32-S3 sendiri; **binary CI dengan kredensial sintetis tidak untuk diflash**.
3. Dari dashboard lokal, pilih mode **Client with recovery AP** atau **Dual**, kemudian jaringan Wi-Fi 2,4 GHz yang disetujui untuk akses HTTPS. Mode AP-only tidak dapat mengunggah data ke internet. Gunakan SSID/VLAN pilot terisolasi, batasi akses fisik dan jangan biarkan kata sandi AP bawaan terekspos pada jaringan tidak tepercaya.
4. Periksa RTC DS3231 dalam UTC, jadwal WIB, CRC pembacaan Modbus, slave ID/baud/parity, profil serta kWh kumulatif dibanding LCD. Pastikan LittleFS ter-mount dan konfigurasi meter tervalidasi. Uploader HTTPS **tidak mengubah** RTC maupun register meter.

## Cara membedakan status

| Status | Makna |
| --- | --- |
| Wi-Fi connected | ESP32 tersambung ke access point; **belum** berarti data diterima website |
| `iot.configured` / `iot.enabled` | Provisioning diterima / task dibuat; belum membuktikan HTTPS berhasil |
| `iot.heartbeatAccepted` | Server memberikan ACK HTTP 200 yang cocok untuk heartbeat, biasanya setiap 60 detik |
| `iot.accepted` / `iot.duplicates` | ACK terverifikasi untuk event **pembacaan** (201 baru / 200 duplikat) |
| Live telemetry | Pada awal pengiriman, lalu sekitar 10 menit setelah keberhasilan terakhir; manual refresh harus mendapat sampel Modbus baru |
| Checkpoint | Sepuluh jadwal asli dalam WIB, jendela pembacaan lima detik; nilai hilang tidak boleh dibuat-buat |
| `iot.lastAckDay` / `iot.lastAckSlot` | Bergerak hanya setelah server ACK dan penyimpanan cursor lokal berhasil |
| `iot.archiveError` | Ada masalah arsip historis yang perlu diperiksa; telemetry baru/heartbeat belum tentu terganggu |

Jika `archive_window_exceeded`, arsip historis lebih lama daripada jendela backfill 30 hari dan tidak boleh dilewati dengan menggeser cursor secara paksa. Jika `archive_corrupt_needs_review`, backup arsip harus diperiksa. Jika `archive_lock_busy`, worker mencoba kembali saat mutex filesystem tersedia. Error sejarah ini **tidak berarti sejarah sudah tersinkronisasi**, meskipun heartbeat dan telemetry baru berhasil.

## Skenario trial yang wajib dilakukan secara terawasi

- Putuskan Wi-Fi, ubah SSID dan simulasi kegagalan DHCP. Pembacaan Modbus serta checkpoint lokal tetap berjalan, kemudian upload pulih setelah jaringan kembali.
- Uji timeout server, 429, 503, sertifikat TLS invalid, ACK tidak cocok dan respons duplikat pada website development. Pastikan tidak ada bearer tersebar, HTTP downgrade, data kWh nol palsu atau angka hasil interpolasi.
- Matikan dan hidupkan perangkat di luar serta dekat jadwal checkpoint. Periksa catatan raw, waktu RTC, NVS/LittleFS dan idempotency event ID.
- Pada perangkat uji khusus, buat `/capture.pending` rusak. Pastikan file rusak dikarantina, bukan langsung ditimpa. Ketika rename gagal atau sembilan slot karantina penuh, penyimpanan harus gagal secara aman.
- Pada filesystem uji, buat lebih dari 256 entri direktori; arsip terbaru harus tetap ditemukan. Bila melebihi 4.096 entri yang diinspeksi, pemulihan harus ditandai tidak lengkap dan pergantian hari checkpoint diblokir.
- Simulasikan outage backfill lebih dari 30 hari menggunakan pengujian terkontrol **tanpa memutar waktu RTC di meter operasional**. Heartbeat dan nilai baru tetap dikirim; sejarah yang tertinggal harus diekspor dan direkonsiliasi oleh operator.
- Lakukan soak test fisik 24–72 jam: catat reset reason, heap minimum dan largest block, stack watermark, status Modbus, timestamp, missed checkpoint, retry HTTPS, Wi-Fi reconnect, serta penerimaan heartbeat.

## Batas keamanan

Jangan menjalankan erase NVS, format LittleFS, flashing ulang seluruh dump, penghapusan otomatis `iot_ack_*.bin`, atau memakai binary sintetis dari CI. Simpan backup dan hash firmware lama sebelum trial. Build dan unit test hijau **bukan** pengganti pengujian nyata di panel pabrik.

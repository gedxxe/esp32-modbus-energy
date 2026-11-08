# Audit keandalan software firmware selama satu tahun

**Bahasa Indonesia** · [English](../YEAR_SOFTWARE_READINESS.md) · [Kesiapan banyak unit](FLEET_READINESS.md) · [Enterprise Wi-Fi](ENTERPRISE_WIFI.md)

## Cakupan

Audit kode untuk target pengoperasian 365 hari. Validasi suplai listrik, RF, akurasi power meter dan penerimaan perangkat fisik menjadi tanggung jawab engineering hardware. Simulasi satu tahun pada unit test menguji kebijakan dan perhitungan waktu, **bukan** pengujian endurance selama satu tahun secara nyata.

## Proteksi software yang tersedia

| Bagian | Kebijakan software | Bukti pengujian |
| --- | --- | --- |
| Wi-Fi Enterprise | `STA.begin(false)` agar tidak ada koneksi prematur; konfigurasi SSID baru tanpa BSSID/channel lama, RSSI bebas, verifikasi baca ulang, PEAP/MSCHAPv2 dan CA; hanya satu `esp_wifi_connect()` setelah EAP aktif | Adapter produksi diuji melalui stub dengan fault injection |
| Uptime | Deadline Wi-Fi rollover-safe; timer backoff HTTPS jangka panjang 64-bit monotonic | Simulasi retry 365 hari melewati rollover 32-bit |
| Retensi | File canonical untuk hari ini dan 30 hari sebelumnya, tanpa menghapus file forensik/karantina | Simulasi 365 hari retensi 31 hari |
| Flash | NVS/CSV identik tidak ditulis ulang jika isinya diverifikasi sama; data rusak tetap diperbaiki melalui jalur recovery | PR #22, fault test |
| Capture | Sepuluh checkpoint kWh aktual per hari, timestamp dan CRC, staging/backup | Unit test lama dan regresi |
| Cloud | ACK cursor setelah tanda terima server dan penulisan lokal terverifikasi; retry checkpoint immutable | IoT native test dan PR #23 terpisah untuk expiry live |
| LoRa | Radio opsional dan protokol v1 tidak diubah | Regression test existing |

## Risiko software yang belum selesai

1. **Keamanan panel HTTP:** token perintah dapat diperoleh dari endpoint status tanpa autentikasi, sedangkan password AP default ada di source publik. Ini harus diperbaiki melalui autentikasi control plane dan kredensial AP unik dengan strategi migrasi. Jangan mengekspos panel pada LAN perusahaan yang tidak terisolasi.
2. **Trust CA privat RADIUS:** Windows mungkin sudah mendapat root CA internal melalui kebijakan TI. ESP32 tidak otomatis mewarisi trust Windows; tanpa unggah manual, bundle CA publik langsung bekerja hanya jika chain RADIUS sudah dipercaya. Jangan menghilangkan verifikasi sertifikat. Firmware saat ini mendukung PEAP/MSCHAPv2, bukan semua metode 802.1X.
3. **Penolakan autentikasi:** alasan disconnect 23/202 menghentikan retry Enterprise sampai profil disimpan kembali. Kebijakan tersebut menghindari lockout akun, tetapi gangguan sementara RADIUS bisa memerlukan pemulihan operator. Perlu rancangan retry terbatas yang tidak menebak password.
4. **Histori setahun:** retensi lokal hanya **31 hari**, bukan 365 hari. Seluruh histori tahunan perlu backup, penerimaan checkpoint cloud dan rekonsiliasi gap yang terverifikasi. File tanggal hilang tidak otomatis berarti perangkat sengaja dimatikan.
5. **Lonjakan kalender:** retensi menghapus satu tanggal tertentu per rollover untuk melindungi dari RTC salah. Setelah gap waktu besar, file lama/karantina bisa menumpuk sampai batas indeks recovery. Penghapusan harus aman, diaudit dan atas persetujuan operator, bukan sweeping otomatis.
6. **Pemeliharaan trust:** root CA bundle, private CA dan kredensial tidak dapat diperbarui otomatis tanpa prosedur update yang tepercaya. NVS standar bukan jaminan enkripsi.
7. **Observabilitas:** persentase penggunaan RAM hasil kompilasi tidak membuktikan fragmentasi heap/stack aman selama setahun. Pertahankan watchdog dan diagnostik heap TLS serta rancang ambang peringatan terukur.

## Persyaratan software sebelum klaim siap satu tahun

- CI untuk commit persis yang akan dirilis: build ESP32-S3 default dan HTTPS, native Modbus/RTC/LoRa/network/storage, browser dan dokumentasi harus lulus.
- Uji software untuk error EAP/CA, larangan koneksi sebelum PEAP, rollover waktu, retry/kursor idempoten, arsip korup, dan downtime panjang.
- Panel administrasi harus terisolasi sampai autentikasi yang kuat tersedia; heartbeat tidak boleh dianggap bukti bahwa kWh sudah masuk database.
- Rekonsiliasi dan backup website harus dapat mempertahankan histori selama 365 hari; penyimpanan lokal hanya cache recovery.

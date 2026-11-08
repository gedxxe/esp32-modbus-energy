# Software / firmware remediation — phase 3 (2026-11-08)

## Baseline dan batasan

Baseline: `main` sesudah PR #26 (`2169021b6681abc16a8676ee0a9e73135086c25a`). Fokus eksklusif **software/firmware**. Tidak mengubah hardware, wiring, parameter Modbus, jadwal capture, payload IoT v1, URL website, LoRa frame, ACK cursor, maupun repository website.

## A. P0 — PEAP RADIUS DNS certificate name, enrollment default

**Masalah:** Profil PEAP dengan `serverName=""` tetap dapat memuat dari penyimpanan lama, dan adapter memanggil `esp_eap_client_set_domain_name(nullptr)`; pemeriksaan identitas server berdasarkan nama domain tidak aktif. Trust-chain dan waktu sertifikat bukan pengganti verifikasi identitas server.

**Kebijakan baru di POST `/api/network`:**

- **Login PEAP baru** atau **penggantian kredensial** wajib mengandung nama DNS sertifikat RADIUS dari IT yang valid. Nilai kosong ditolak HTTP 400 dengan kode `invalid_enterprise` yang sudah dikenal UI.
- **Profil lama PEAP tanpa nama** tetap dapat didekode, dinyalakan, dan dicoba ulang bila *login yang disimpan dipakai persis tanpa perubahan* pada SSID, identitas, password, CA atau nama server. Ini merupakan pengecualian pemulihan sementara.
- Mengubah SSID, CA, username, password, outer EAP identity, atau mengganti nama server memerlukan provisioning yang memenuhi aturan baru.
- Beralih ke mode AP atau Personal tidak diberi syarat nama RADIUS.
- Tidak pernah mengarang nama RADIUS, menonaktifkan validasi tanggal/CA, mengubah catatan penyimpanan otomatis, atau memaksa restart.
- Aturan ditulis sebagai `peapProvisioningAllowed()` yang murni sehingga dapat diuji native. `validNetwork()` tetap menerima rekaman lama saat proses *decode*, dan penolakan dilakukan pada *admission* perubahan konfigurasi baru.

**Catatan UX / syarat sebelum merge:** Frontend hasil bundle yang saat ini dikirim perangkat masih melabeli kolom DNS di bagian **Advanced Wi-Fi settings** sebagai *optional*. Secara keamanan, firmware akan menolak login baru bila kolom ini kosong. Pengguna perlu membuka pengaturan lanjut dan memasukkan nama sertifikat DNS yang diverifikasi IT. Label UI dan penanganan error yang lebih eksplisit harus diperbarui bersamaan dengan regenerasi aset `firmware/include/web_ui.h` sebelum dianggap siap rollout luas. Tidak boleh mengubah sumber JSX saja dan membiarkan bundle perangkat tertinggal.

**Keterbatasan keamanan:** Profil lama tetap memiliki risiko *RADIUS identity mismatch* sampai dimigrasikan; PR ini bukan bukti keamanan penuh jaringan Enterprise. Untuk jaringan yang menggunakan EAP-TLS/WPA3 Enterprise, adapter PEAP saat ini bukan implementasi penggantinya.

## B. P1 — peringatan dini gap ACK archive

- `backfillHealth(latest, acknowledged, 30)` menghasilkan nilai eksplisit `cursorKnown`, `lagDays`, `remainingDays`, `nearLimit` dan `windowExceeded`.
- Batas peringatan: mulai **25 hari sejak hari ACK terakhir** (tersisa lima hari). Pada lag **30** window belum dilampaui; pada **31** dilewati dan butuh rekonsiliasi historis.
- Kalkulasi dijalankan task HTTPS bahkan ketika WiFi offline, **selama** status cursor terverifikasi dan snapshot capture durable tersedia.
- Cursor tidak ada, belum diverifikasi, atau lebih baru dibanding hari active diklasifikasi **unknown**, bukan *healthy*.
- Hasil tercantum di `/api/status` `iot.backfillCursorKnown`, `iot.backfillDaysBehind`, `iot.backfillRemainingDays`, `iot.backfillNearLimit` dan `iot.backfillWindowExceeded`.
- Field hanya diagnostik. Tidak ada perubahan `lastAckDay`, `lastAckSlot`, pembuatan event ID, retry, atau timestamp yang bersifat mutable.

## C. Regresi dan deployment acceptance

- Native: pengecualian **legacy saved retry**, penolakan **fresh PEAP** tanpa nama, penolakan SSID, password, dan CA yang berubah, penerimaan PIN baru, dan mode AP.
- Native: backlog `unknown`, 0, 25, 30, 31 hari serta backward RTC dan `backfillWindowExceeded` yang tetap konsisten.
- CI: native suite, source audit, browser bundle (tidak ada perubahan UI pada PR ini), ESP32-S3 build, standalone diagnostic build, dan synthetic HTTPS credentials.
- **Staging wajib:** uji actual Enterprise RADIUS DNS + CA milik IT, migrasi akun lama, 400 error bila nama kosong, client & AP recovery, dan reboot dengan saved profile.
- **Staging wajib:** putuskan koneksi selama beberapa hari simulasi dengan *synthetic durable snapshots*; verifikasi warning muncul pada 25 hari, tidak melakukan ACK palsu, live/heartbeat tetap berfungsi ketika backlog terlalu tua.
- **30 hari soak wajib sebelum menyebut production-ready**: tunda fatal reconnect/heap/HTTP hang dan verifikasi tidak ada data capture hilang.

## D. P1 HTTP slow header dan dependency hardening

Pinned `platformio.ini` saat baseline menggunakan `pioarduino/platform-espressif32` `55.03.311` (Arduino-ESP32 3.3.11). Ada bug upstream `WebServer::handleClient()` pada core tersebut saat header parsial diteteskan. Arduino 3.3.12 resmi menyertakan perbaikan `#12794`; pioarduino `55.03.312-1` tersedia dengan perbaikan kompatibilitas SCons dibanding `55.03.312`.

**Upgrade harus PR terpisah**: verifikasi firmware, boot, HTTP load / slow-header staging, station AP/PEAP, PSRAM/TLS config, Arduino event semantics, upload HTTPS, heap minimum; jangan menganggap build success = soak success. Jangan langsung mengganti WebServer vendor dengan implementasi yang tidak diuji.

## Sisa roadmap

1. **P0**: UI naming + certificate enrollment migration dengan IT, serta test live PEAP. No rollover release without this gate.
2. **P0**: staging website HTTPS ingest dengan token tes, receipt/ACK end-to-end, connection loss, rate limiting, transient proxy.
3. **P1**: pin update Arduino 3.3.12 / WebServer hardened (PR terpisah), uji slow-client dan watchdog.
4. **P1**: event-generation WiFi same SSID/BSSID dan replay mock; filter berbeda-SSID bukan bukti total.
5. **P1**: soak heap/stack/task HTTP & 30-day gap reconciliation operator.
6. **P2**: LoRa ISR/task synchronization audit, tidak mengubah wire protocol.
7. **P3**: pecah `main.cpp` jadi modul HTTP, network, recorder, storage secara incremental.

Skor kesiapan produksi belum dinaikkan tanpa pengujian staging. Semua komponen ini masih PR draft, tidak merge otomatis.
